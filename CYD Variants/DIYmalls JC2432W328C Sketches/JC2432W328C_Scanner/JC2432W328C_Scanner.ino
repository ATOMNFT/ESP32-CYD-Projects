/*
  ============================================================================
  JC2432W328C Scanner
  ============================================================================
  Created by ATOMNFT
  GitHub: https://github.com/ATOMNFT

  Board / Hardware:
    - ESP32-based JC2432W328C display board
    - ST7789 TFT display
    - CST820 touch controller
    - Onboard RGB LED

  Display / Graphics:
    - Uses TFT_eSPI for display output
    - Uses LVGL for the user interface
    - Uses a non-DMA display flush for stability
    - LVGL draw buffers are allocated in DMA-capable internal RAM,
      but TFT DMA transfers are currently disabled

  Notes:
    - This sketch is configured for reliability over display DMA speed
    - Screen updates are pushed with standard TFT_eSPI pixel writes
    - If DMA support is re-enabled later, comments should be updated to match

  Purpose:
    - Touch-based scanner interface for Wi-Fi / BLE style tools
    - Built as a working base for further UI and scanning feature expansion

  Tip:
    - Check pin mappings, display rotation, touch settings, and backlight config
      before flashing to a different version of this board

  ============================================================================
*/

#include <lvgl.h>
#include <TFT_eSPI.h>
#include "CST820.h"
#include <Wire.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ─────────────────────────────────────────────────────────────────────────────
// RGB LED
// ─────────────────────────────────────────────────────────────────────────────
#define LED_R_PIN    4
#define LED_G_PIN    16
#define LED_B_PIN    17
#define RGB_INVERTED 1          // Common-anode

static const int LED_PWM_FREQ = 5000;
static const int LED_PWM_BITS = 8;
static const int LEDC_CH_R = 0;
static const int LEDC_CH_G = 1;
static const int LEDC_CH_B = 2;

static inline uint8_t rgb_fix(uint8_t v) {
#if RGB_INVERTED
  return 255 - v;
#else
  return v;
#endif
}
static void rgb_init() {
  ledcSetup(LEDC_CH_R, LED_PWM_FREQ, LED_PWM_BITS);
  ledcSetup(LEDC_CH_G, LED_PWM_FREQ, LED_PWM_BITS);
  ledcSetup(LEDC_CH_B, LED_PWM_FREQ, LED_PWM_BITS);
  ledcAttachPin(LED_R_PIN, LEDC_CH_R);
  ledcAttachPin(LED_G_PIN, LEDC_CH_G);
  ledcAttachPin(LED_B_PIN, LEDC_CH_B);
  ledcWrite(LEDC_CH_R, rgb_fix(0));
  ledcWrite(LEDC_CH_G, rgb_fix(0));
  ledcWrite(LEDC_CH_B, rgb_fix(0));
}
static void rgb_set(uint8_t r, uint8_t g, uint8_t b) {
  ledcWrite(LEDC_CH_R, rgb_fix(r));
  ledcWrite(LEDC_CH_G, rgb_fix(g));
  ledcWrite(LEDC_CH_B, rgb_fix(b));
}

// ─────────────────────────────────────────────────────────────────────────────
// Config
// ─────────────────────────────────────────────────────────────────────────────
#define GATE_READS_BY_INT    0
#define INT_MODE_INPUT_PULLUP 1
#define DO_TOUCH_RESET_PULSE 1
#define I2C_CLOCK_HZ         400000
#define BLE_SCAN_SECS        5

// ─────────────────────────────────────────────────────────────────────────────
// Pins
// ─────────────────────────────────────────────────────────────────────────────
#define I2C_SDA 33
#define I2C_SCL 32
#define TP_RST  25
#define TP_INT  21

static const uint16_t SCREEN_W = 240;
static const uint16_t SCREEN_H = 320;

// ─────────────────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────────────────
TFT_eSPI tft;
CST820   touch(I2C_SDA, I2C_SCL, TP_RST, TP_INT);

// LVGL draw buffers
static lv_draw_buf_t draw_buf1, draw_buf2;
static lv_color_t   *buf_mem1, *buf_mem2;
static const uint32_t BUF_LINES = 10;

// ── Scan result storage ───────────────────────────────────────────────────────
#define MAX_APS   20
#define MAX_BLES  20

struct APEntry {
  String  ssid;
  int32_t rssi;
  uint8_t encType;
  int32_t channel;
};
APEntry apList[MAX_APS];
int     apCount = 0;

struct BLEEntry {
  String name;
  String address;
  int    rssi;
  bool   hasName;
};
BLEEntry bleList[MAX_BLES];
int      bleCount = 0;

static BLEScan *gBleScan = nullptr; // cached BLE scan pointer

// ── LVGL screens ─────────────────────────────────────────────────────────────
static lv_obj_t *scr_home    = nullptr;
static lv_obj_t *scr_wifi    = nullptr;
static lv_obj_t *scr_ble     = nullptr;
static lv_obj_t *lbl_wifi_status = nullptr;
static lv_obj_t *lbl_ble_status  = nullptr;
static lv_obj_t *list_wifi   = nullptr;
static lv_obj_t *list_ble    = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// BLE scan callback
// ─────────────────────────────────────────────────────────────────────────────
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    if (bleCount >= MAX_BLES) return;
    String addr = dev.getAddress().toString().c_str();
    for (int i = 0; i < bleCount; i++) {
      if (bleList[i].address == addr) return; // dedup
    }
    BLEEntry &e = bleList[bleCount++];
    e.address = addr;
    e.rssi    = dev.getRSSI();
    e.hasName = dev.haveName();
    e.name    = e.hasName ? dev.getName().c_str() : "(unnamed)";
  }
};

static ScanCallbacks scanCB; // keep callback alive (global)

// ─────────────────────────────────────────────────────────────────────────────
// BLE heap management (no-PSRAM friendly)
// ─────────────────────────────────────────────────────────────────────────────
static bool ble_inited = false;

static void ble_init_if_needed() {
  if (ble_inited) return;

  // Ensure WiFi fully off before BLE init
  WiFi.scanDelete();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(80);

  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  BLEDevice::init("");
  gBleScan = BLEDevice::getScan();

  ble_inited = true;
}

static void ble_deinit_if_needed() {
  if (!ble_inited) return;

  if (gBleScan) {
    gBleScan->stop();
    gBleScan->clearResults();
  }

  // Release memory back to heap so WiFi can start
  BLEDevice::deinit(true);
  gBleScan = nullptr;
  ble_inited = false;

  delay(50);
}


// ─────────────────────────────────────────────────────────────────────────────
// LVGL display flush
// ─────────────────────────────────────────────────────────────────────────────
static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  const uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
  const uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h); // TFT_eSPI expects (x,y,w,h)
  tft.pushPixels((uint16_t *)px_map, w * h);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

// ─────────────────────────────────────────────────────────────────────────────
// LVGL touch read
// ─────────────────────────────────────────────────────────────────────────────
static void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  (void)indev;

#if GATE_READS_BY_INT
  if (digitalRead(TP_INT) == HIGH) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
#endif

  uint16_t x = 0, y = 0;
  uint8_t  gesture = 0;
  bool touched = touch.getTouch(&x, &y, &gesture);

  if (!touched) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  // Rotation 0 — adjust mapping if needed
  uint16_t tx = x;
  uint16_t ty = y;

  // Uncomment ONE if touch is mirrored/rotated:
  // tx = y;                         ty = x;                         // swap
  // tx = SCREEN_W - 1 - y;          ty = x;                         // swap+mirrorX
  // tx = y;                          ty = SCREEN_H - 1 - x;         // swap+mirrorY
  // tx = SCREEN_W - 1 - y;          ty = SCREEN_H - 1 - x;         // swap+mirror both

  tx = constrain(tx, 0, SCREEN_W - 1);
  ty = constrain(ty, 0, SCREEN_H - 1);

  data->state   = LV_INDEV_STATE_PRESSED;
  data->point.x = tx;
  data->point.y = ty;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: RSSI → LVGL color
// ─────────────────────────────────────────────────────────────────────────────
static lv_color_t rssiColor(int rssi) {
  if (rssi >= -60) return lv_palette_main(LV_PALETTE_GREEN);
  if (rssi >= -75) return lv_palette_main(LV_PALETTE_YELLOW);
  return lv_palette_main(LV_PALETTE_RED);
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi scan + populate list
// ─────────────────────────────────────────────────────────────────────────────
static void run_wifi_scan() {
  rgb_set(0, 0, 255); // Blue = WiFi

  lv_label_set_text(lbl_wifi_status, "Scanning...");
  lv_obj_clean(list_wifi);
  lv_timer_handler(); // flush UI

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  apCount = 0;

  if (n > 0) {
    int lim = min(n, MAX_APS);
    for (int i = 0; i < lim; i++) {
      apList[i].ssid    = WiFi.SSID(i);
      apList[i].rssi    = WiFi.RSSI(i);
      apList[i].encType = WiFi.encryptionType(i);
      apList[i].channel = WiFi.channel(i);
    }
    // Sort strongest first (bubble)
    for (int i = 0; i < lim - 1; i++)
      for (int j = 0; j < lim - 1 - i; j++)
        if (apList[j].rssi < apList[j+1].rssi) {
          APEntry tmp = apList[j]; apList[j] = apList[j+1]; apList[j+1] = tmp;
        }
    apCount = lim;
  }
  WiFi.scanDelete();

  // Update status label
  char buf[40];
  snprintf(buf, sizeof(buf), "%d network%s found", apCount, apCount == 1 ? "" : "s");
  lv_label_set_text(lbl_wifi_status, buf);

  // Populate list
  for (int i = 0; i < apCount; i++) {
    // Each entry: one list button with multi-line label
    lv_obj_t *item = lv_list_add_button(list_wifi, NULL, "");
    lv_obj_set_style_bg_color(item,
      i % 2 == 0 ? lv_color_hex(0x1a1a2e) : lv_color_hex(0x16213e), 0);

    // Replace the auto-created label with a formatted one
    lv_obj_t *lbl = lv_obj_get_child(item, 0);

    bool open = (apList[i].encType == WIFI_AUTH_OPEN);
    String ssid = apList[i].ssid.length() > 0 ? apList[i].ssid : "(hidden)";
    if (ssid.length() > 20) ssid = ssid.substring(0, 19) + "~";

    char row[80];
    snprintf(row, sizeof(row), "%s %s\n%d dBm  CH%ld",
      open ? "[OPEN]" : "[SEC]",
      ssid.c_str(),
      (int)apList[i].rssi,
      (long)apList[i].channel
    );
    lv_label_set_text(lbl, row);
    lv_obj_set_style_text_color(lbl, rssiColor(apList[i].rssi), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  }

  if (apCount == 0) {
    lv_list_add_text(list_wifi, "No networks found");
  }

  rgb_set(0, 0, 40); // dim blue = done
}

// ─────────────────────────────────────────────────────────────────────────────
// BLE scan + populate list
// ─────────────────────────────────────────────────────────────────────────────
static void run_ble_scan() {
  rgb_set(128, 0, 255); // Purple = BLE

  lv_label_set_text(lbl_ble_status, "Scanning 5s...");
  lv_obj_clean(list_ble);
  lv_timer_handler();

  // Release SPI bus before touching the radio — holding startWrite() open
  // while BLE stack initialises causes a crash on ESP32.
  tft.endWrite();

  // Turn WiFi off before BLE — they share the same radio on ESP32
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  // Init BLE on-demand (and keep heap sane for WiFi)
  ble_init_if_needed();

  bleCount = 0;
  if (!gBleScan) gBleScan = BLEDevice::getScan();
  BLEScan *pScan = gBleScan;
  pScan->clearResults();
  pScan->setAdvertisedDeviceCallbacks(&scanCB, false);
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(BLE_SCAN_SECS, false); // blocking
  pScan->clearResults();

  // Reclaim SPI bus for display
  tft.startWrite();

  // Sort by RSSI
  for (int i = 0; i < bleCount - 1; i++)
    for (int j = 0; j < bleCount - 1 - i; j++)
      if (bleList[j].rssi < bleList[j+1].rssi) {
        BLEEntry tmp = bleList[j]; bleList[j] = bleList[j+1]; bleList[j+1] = tmp;
      }

  char buf[40];
  snprintf(buf, sizeof(buf), "%d device%s found", bleCount, bleCount == 1 ? "" : "s");
  lv_label_set_text(lbl_ble_status, buf);

  for (int i = 0; i < bleCount; i++) {
    lv_obj_t *item = lv_list_add_button(list_ble, NULL, "");
    lv_obj_set_style_bg_color(item,
      i % 2 == 0 ? lv_color_hex(0x1a0a2e) : lv_color_hex(0x16082a), 0);

    lv_obj_t *lbl = lv_obj_get_child(item, 0);

    String nm = bleList[i].name;
    if (nm.length() > 18) nm = nm.substring(0, 17) + "~";
    String addr = bleList[i].address.substring(0, 11) + "..";

    char row[80];
    snprintf(row, sizeof(row), "%s\n%s  %d dBm",
      nm.c_str(),
      addr.c_str(),
      bleList[i].rssi
    );
    lv_label_set_text(lbl, row);
    lv_obj_set_style_text_color(lbl, rssiColor(bleList[i].rssi), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  }

  if (bleCount == 0) {
    lv_list_add_text(list_ble, "No BLE devices found");
  }

    rgb_set(20, 0, 40); // dim purple = done

  // Free BLE heap so WiFi can start cleanly
  ble_deinit_if_needed();
}

// ─────────────────────────────────────────────────────────────────────────────
// Button callbacks
// ─────────────────────────────────────────────────────────────────────────────
static void cb_go_wifi(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_screen_load(scr_wifi);
  run_wifi_scan();
}

static void cb_go_ble(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_screen_load(scr_ble);
  run_ble_scan();
}

static void cb_back_home(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  rgb_set(0, 0, 0);
  lv_screen_load(scr_home);
}

static void cb_rescan_wifi(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  run_wifi_scan();
}

static void cb_rescan_ble(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  run_ble_scan();
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: styled button
// ─────────────────────────────────────────────────────────────────────────────
static lv_obj_t* make_button(lv_obj_t *parent, const char *label_text,
                               lv_color_t bg, lv_event_cb_t cb) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_style_bg_color(btn, bg, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 10, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(0x07FFFF), 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, label_text);
  lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
  lv_obj_center(lbl);
  return btn;
}

// ─────────────────────────────────────────────────────────────────────────────
// Build UI
// ─────────────────────────────────────────────────────────────────────────────
static void build_ui() {
  lv_color_t bg_dark = lv_color_hex(0x0D0D1A);

  // ── HOME SCREEN ──────────────────────────────────────────────────────────
  scr_home = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_home, bg_dark, 0);
  lv_obj_set_style_bg_opa(scr_home, LV_OPA_COVER, 0);

  // Title
  lv_obj_t *title = lv_label_create(scr_home);
  lv_label_set_text(title, LV_SYMBOL_WIFI "  ESP32 Scanner");
  lv_obj_set_style_text_color(title, lv_color_hex(0x07FFFF), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

  // Divider
  lv_obj_t *line = lv_obj_create(scr_home);
  lv_obj_set_size(line, SCREEN_W - 20, 1);
  lv_obj_set_style_bg_color(line, lv_color_hex(0x07FFFF), 0);
  lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 44);

  // Subtitle
  lv_obj_t *sub = lv_label_create(scr_home);
  lv_label_set_text(sub, "Select a scan mode");
  lv_obj_set_style_text_color(sub, lv_color_hex(0x8888AA), 0);
  lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
  lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 56);

  // WiFi button
  lv_obj_t *btn_wifi = make_button(scr_home,
    LV_SYMBOL_WIFI "  WiFi Scan",
    lv_color_hex(0x003580), cb_go_wifi);
  lv_obj_set_size(btn_wifi, 200, 60);
  lv_obj_align(btn_wifi, LV_ALIGN_CENTER, 0, -50);

  // BLE button
  lv_obj_t *btn_ble = make_button(scr_home,
    LV_SYMBOL_BLUETOOTH "  BLE Scan",
    lv_color_hex(0x500080), cb_go_ble);
  lv_obj_set_size(btn_ble, 200, 60);
  lv_obj_align(btn_ble, LV_ALIGN_CENTER, 0, 30);

  // Footer
  lv_obj_t *footer = lv_label_create(scr_home);
  lv_label_set_text(footer, "JC2432W328C  |  ATOMNFT");
  lv_obj_set_style_text_color(footer, lv_color_hex(0x444466), 0);
  lv_obj_set_style_text_font(footer, &lv_font_montserrat_14, 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -10);

  // ── WIFI SCREEN ──────────────────────────────────────────────────────────
  scr_wifi = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_wifi, bg_dark, 0);
  lv_obj_set_style_bg_opa(scr_wifi, LV_OPA_COVER, 0);

  // Header row
  lv_obj_t *wifi_hdr = lv_obj_create(scr_wifi);
  lv_obj_set_size(wifi_hdr, SCREEN_W, 46);
  lv_obj_set_style_bg_color(wifi_hdr, lv_color_hex(0x001040), 0);
  lv_obj_set_style_border_width(wifi_hdr, 0, 0);
  lv_obj_align(wifi_hdr, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_pad_all(wifi_hdr, 4, 0);

  lv_obj_t *wifi_title = lv_label_create(wifi_hdr);
  lv_label_set_text(wifi_title, LV_SYMBOL_WIFI "  WiFi Networks");
  lv_obj_set_style_text_color(wifi_title, lv_color_hex(0x07FFFF), 0);
  lv_obj_set_style_text_font(wifi_title, &lv_font_montserrat_14, 0);
  lv_obj_align(wifi_title, LV_ALIGN_LEFT_MID, 4, 0);

  // Status label (top right of header)
  lbl_wifi_status = lv_label_create(wifi_hdr);
  lv_label_set_text(lbl_wifi_status, "");
  lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xAABBCC), 0);
  lv_obj_set_style_text_font(lbl_wifi_status, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl_wifi_status, LV_ALIGN_RIGHT_MID, -4, 0);

  // Scrollable results list
  list_wifi = lv_list_create(scr_wifi);
  lv_obj_set_size(list_wifi, SCREEN_W, SCREEN_H - 46 - 44);
  lv_obj_align(list_wifi, LV_ALIGN_TOP_LEFT, 0, 46);
  lv_obj_set_style_bg_color(list_wifi, bg_dark, 0);
  lv_obj_set_style_border_width(list_wifi, 0, 0);
  lv_obj_set_style_pad_row(list_wifi, 2, 0);

  // Bottom bar: Back + Rescan
  lv_obj_t *wifi_bar = lv_obj_create(scr_wifi);
  lv_obj_set_size(wifi_bar, SCREEN_W, 44);
  lv_obj_align(wifi_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(wifi_bar, lv_color_hex(0x001020), 0);
  lv_obj_set_style_border_width(wifi_bar, 0, 0);
  lv_obj_set_style_pad_all(wifi_bar, 4, 0);
  lv_obj_set_flex_flow(wifi_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(wifi_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *wb1 = make_button(wifi_bar, LV_SYMBOL_HOME "  Home",
    lv_color_hex(0x222244), cb_back_home);
  lv_obj_set_size(wb1, 100, 34);

  lv_obj_t *wb2 = make_button(wifi_bar, LV_SYMBOL_REFRESH "  Rescan",
    lv_color_hex(0x003580), cb_rescan_wifi);
  lv_obj_set_size(wb2, 100, 34);

  // ── BLE SCREEN ───────────────────────────────────────────────────────────
  scr_ble = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_ble, bg_dark, 0);
  lv_obj_set_style_bg_opa(scr_ble, LV_OPA_COVER, 0);

  lv_obj_t *ble_hdr = lv_obj_create(scr_ble);
  lv_obj_set_size(ble_hdr, SCREEN_W, 46);
  lv_obj_set_style_bg_color(ble_hdr, lv_color_hex(0x180030), 0);
  lv_obj_set_style_border_width(ble_hdr, 0, 0);
  lv_obj_align(ble_hdr, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_pad_all(ble_hdr, 4, 0);

  lv_obj_t *ble_title = lv_label_create(ble_hdr);
  lv_label_set_text(ble_title, LV_SYMBOL_BLUETOOTH "  BLE Devices");
  lv_obj_set_style_text_color(ble_title, lv_color_hex(0xCC88FF), 0);
  lv_obj_set_style_text_font(ble_title, &lv_font_montserrat_14, 0);
  lv_obj_align(ble_title, LV_ALIGN_LEFT_MID, 4, 0);

  lbl_ble_status = lv_label_create(ble_hdr);
  lv_label_set_text(lbl_ble_status, "");
  lv_obj_set_style_text_color(lbl_ble_status, lv_color_hex(0xAABBCC), 0);
  lv_obj_set_style_text_font(lbl_ble_status, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl_ble_status, LV_ALIGN_RIGHT_MID, -4, 0);

  list_ble = lv_list_create(scr_ble);
  lv_obj_set_size(list_ble, SCREEN_W, SCREEN_H - 46 - 44);
  lv_obj_align(list_ble, LV_ALIGN_TOP_LEFT, 0, 46);
  lv_obj_set_style_bg_color(list_ble, bg_dark, 0);
  lv_obj_set_style_border_width(list_ble, 0, 0);
  lv_obj_set_style_pad_row(list_ble, 2, 0);

  lv_obj_t *ble_bar = lv_obj_create(scr_ble);
  lv_obj_set_size(ble_bar, SCREEN_W, 44);
  lv_obj_align(ble_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(ble_bar, lv_color_hex(0x100020), 0);
  lv_obj_set_style_border_width(ble_bar, 0, 0);
  lv_obj_set_style_pad_all(ble_bar, 4, 0);
  lv_obj_set_flex_flow(ble_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ble_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *bb1 = make_button(ble_bar, LV_SYMBOL_HOME "  Home",
    lv_color_hex(0x222244), cb_back_home);
  lv_obj_set_size(bb1, 100, 34);

  lv_obj_t *bb2 = make_button(ble_bar, LV_SYMBOL_REFRESH "  Rescan",
    lv_color_hex(0x500080), cb_rescan_ble);
  lv_obj_set_size(bb2, 100, 34);
}

// ─────────────────────────────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(50);

  rgb_init();
  rgb_set(0, 0, 0);

  Serial.println("=== ESP32 Scanner — WiFi + BLE ===");

  lv_init();

  // Backlight power enable
  pinMode(27, OUTPUT);
  digitalWrite(27, LOW);

  // TFT
  tft.begin();
  tft.setRotation(0);
  tft.setSwapBytes(true);  // match v5 color order for LVGL RGB565
  //// tft.initDMA(); // disabled: using non-DMA LVGL flush for stability

  // Touch I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_CLOCK_HZ);

#if INT_MODE_INPUT_PULLUP
  pinMode(TP_INT, INPUT_PULLUP);
#else
  pinMode(TP_INT, INPUT);
#endif

#if DO_TOUCH_RESET_PULSE
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW); delay(10);
  digitalWrite(TP_RST, HIGH); delay(50);
#endif

  touch.begin();
  delay(20);

  digitalWrite(27, HIGH); // backlight ON

  // LVGL buffers (DMA)
  const uint32_t buf_pixels = (uint32_t)SCREEN_W * BUF_LINES;
  const size_t   buf_bytes  = buf_pixels * sizeof(lv_color_t);

  buf_mem1 = (lv_color_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  buf_mem2 = nullptr; // single buffer to save heap (no PSRAM)
  if (!buf_mem1) {
    Serial.println("ERROR: LVGL buffer allocation failed!");
    while (1) delay(1000);
  }

  lv_draw_buf_init(&draw_buf1, SCREEN_W, BUF_LINES, LV_COLOR_FORMAT_RGB565, 0, buf_mem1, buf_bytes);
    lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_draw_buffers(disp, &draw_buf1, NULL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  // Init BLE stack once — must not be called again after this

  tft.startWrite();

  build_ui();
  lv_screen_load(scr_home);

  Serial.println("Ready. Touch [WiFi Scan] or [BLE Scan].");
}

// ─────────────────────────────────────────────────────────────────────────────
// loop
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  static uint32_t last_ms = 0;
  uint32_t now     = millis();
  uint32_t elapsed = now - last_ms;
  last_ms = now;

  lv_tick_inc(elapsed);
  lv_timer_handler();

  delay(5);
}

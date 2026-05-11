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

  Arduino IDE Settings:
    - Board: ESP32 Dev Module
    - Partition Scheme: Huge APP  

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
#include <stdint.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <string>

#include "config.h"  // Some settings chose to live here.

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
static bool g_rgb_enabled = CFG_RGB_ENABLED; // settings default

static void rgb_set(uint8_t r, uint8_t g, uint8_t b) {
  if (!g_rgb_enabled) {
    ledcWrite(LEDC_CH_R, rgb_fix(0));
    ledcWrite(LEDC_CH_G, rgb_fix(0));
    ledcWrite(LEDC_CH_B, rgb_fix(0));
    return;
  }
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


#define MAX_APS   20
#define MAX_BLES  20


// ─────────────────────────────────────────────────────────────────────────────
// Settings (runtime)
// ─────────────────────────────────────────────────────────────────────────────
static uint8_t  g_ble_scan_secs    = CFG_BLE_SCAN_SECS;   // seconds
static uint8_t  g_wifi_scan_secs   = CFG_WIFI_SCAN_SECS;  // seconds
static uint8_t  g_wifi_max_results  = CFG_WIFI_MAX_RESULTS; // display cap
static uint8_t  g_ble_max_results   = CFG_BLE_MAX_RESULTS;  // display cap
static bool     g_wifi_show_hidden  = CFG_WIFI_SHOW_HIDDEN;
static bool     g_ble_active_scan   = CFG_BLE_ACTIVE_SCAN;
static uint8_t  g_brightness_pct    = CFG_BRIGHTNESS_PCT;   // 0-100
static uint16_t g_sleep_timeout_s   = CFG_SLEEP_TIMEOUT_S;  // 0 = disabled
static uint8_t  g_sleep_dim_pct    = CFG_SLEEP_DIM_PCT;    // dim brightness percent
// Backlight control (GPIO27). Uses LEDC so brightness slider works.
#define BL_PIN 27
#define BL_PWM_FREQ 5000
#define BL_PWM_BITS 8
#define BL_PWM_CH   3
static void bl_init() {
  ledcSetup(BL_PWM_CH, BL_PWM_FREQ, BL_PWM_BITS);
  ledcAttachPin(BL_PIN, BL_PWM_CH);
}
static void bl_write(uint8_t pct) {
  if (pct > 100) pct = 100;
  uint32_t duty = (uint32_t)pct * 255 / 100;
  ledcWrite(BL_PWM_CH, duty);
}

static void bl_apply(uint8_t pct) {
  // Apply user brightness (and store it)
  if (pct > 100) pct = 100;
  g_brightness_pct = pct;
  bl_write(g_brightness_pct);
}

// Inactivity dim/off
static uint32_t g_last_touch_ms = 0;
static bool g_dimmed = false;
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
struct APEntry {
  String  ssid;
  String  bssid;   // AP MAC (BSSID)
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

  // Extra BLE advertising info
  uint8_t  svcCount;     // advertised service UUID count
  uint16_t mfgLen;       // manufacturer data length (bytes)
  bool     hasTxPower;
  int8_t   txPower;      // TX power (dBm) when available
};
BLEEntry bleList[MAX_BLES];
int      bleCount = 0;

static BLEScan *gBleScan = nullptr; // cached BLE scan pointer

// ── LVGL screens ─────────────────────────────────────────────────────────────
static lv_obj_t *scr_home    = nullptr;
static lv_obj_t *scr_wifi    = nullptr;
static lv_obj_t *scr_ble     = nullptr;
static lv_obj_t *scr_settings = nullptr;
static lv_obj_t *scr_wifi_detail = nullptr;
static lv_obj_t *scr_ble_detail  = nullptr;
static lv_obj_t *lbl_wifi_status = nullptr;
static lv_obj_t *lbl_ble_status  = nullptr;
static lv_obj_t *list_wifi   = nullptr;
static lv_obj_t *list_ble    = nullptr;
static lv_obj_t *btn_settings = nullptr;
static lv_obj_t *slider_bright = nullptr;
static lv_obj_t *slider_sleep  = nullptr; // (deprecated, replaced by dd_sleep)
static lv_obj_t *dd_sleep        = nullptr;
static lv_obj_t *dd_ble_secs    = nullptr;
static lv_obj_t *dd_wifi_secs   = nullptr;
static lv_obj_t *dd_wifi_max    = nullptr;
static lv_obj_t *dd_ble_max     = nullptr;
static lv_obj_t *sw_hidden      = nullptr;
static lv_obj_t *sw_active      = nullptr;
static lv_obj_t *sw_rgb         = nullptr;
static lv_obj_t *lbl_wifi_detail = nullptr;
static lv_obj_t *lbl_ble_detail  = nullptr;


// ── Selection tracking (clickable list items)
static lv_obj_t *wifi_selected_item = nullptr;
static lv_obj_t *ble_selected_item  = nullptr;
// ─────────────────────────────────────────────────────────────────────────────
// BLE scan callback
// ─────────────────────────────────────────────────────────────────────────────
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    if (bleCount >= MAX_BLES) return;
    String addr = dev.getAddress().toString().c_str();
    ble_selected_item = nullptr;
  int bleShow = bleCount;
  if (bleShow > g_ble_max_results) bleShow = g_ble_max_results;

  for (int i = 0; i < bleShow; i++) {
      if (bleList[i].address == addr) return; // dedup
    }
    BLEEntry &e = bleList[bleCount++];
    e.address = addr;
    e.rssi    = dev.getRSSI();
    e.hasName = dev.haveName();
    e.name    = e.hasName ? dev.getName().c_str() : "(unnamed)";
  
    // Service UUID count
    e.svcCount = (uint8_t)dev.getServiceUUIDCount();

    // Manufacturer data length
    {
      std::string md = dev.getManufacturerData();
      e.mfgLen = (uint16_t)md.length();
    }

    // TX power (if available)
    e.hasTxPower = dev.haveTXPower();
    e.txPower = e.hasTxPower ? (int8_t)dev.getTXPower() : (int8_t)0;
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
// Radio transition helper: prepare WiFi after BLE
// ─────────────────────────────────────────────────────────────────────────────
static void radio_prepare_wifi() {
  // Stop any ongoing BLE scan and fully deinit BLE stack to free radio/heap.
  if (ble_inited) {
    if (gBleScan) {
      gBleScan->stop();
      gBleScan->clearResults();
    }
    BLEDevice::deinit(true);
    gBleScan = nullptr;
    ble_inited = false;
    delay(120);
  }

  // Reset WiFi state cleanly
  WiFi.scanDelete();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(150);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  delay(150);
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


  // Inactivity tracking: any touch counts as activity + wake from dim
  g_last_touch_ms = millis();
  if (g_dimmed) {
    g_dimmed = false;
    bl_apply(g_brightness_pct);
  }
  // Inactivity tracking: any touch counts as activity
  g_last_touch_ms = millis();
  if (g_dimmed) { g_dimmed = false; bl_apply(g_brightness_pct); }
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
// WiFi encryption/auth mode name helper
// ─────────────────────────────────────────────────────────────────────────────
static const char* authModeName(uint8_t encType) {
  switch (encType) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
    case WIFI_AUTH_WAPI_PSK:        return "WAPI";
    default:                        return "UNKNOWN";
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi hard reset helper (fixes WL_NO_SHIELD / scan returning -1)
// ─────────────────────────────────────────────────────────────────────────────
static void wifi_hard_reset_sta() {
  esp_wifi_stop();
  esp_wifi_deinit();
  delay(50);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  delay(50);
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi scan + populate list
// ─────────────────────────────────────────────────────────────────────────────
static void run_wifi_scan() {
  rgb_set(0, 255, 0); // Green = WiFi

  lv_label_set_text(lbl_wifi_status, "Scanning...");
  lv_obj_clean(list_wifi);
  lv_timer_handler(); // flush UI

  Serial.printf("\n[WiFi] ---- Scan start (Arduino WiFi) ----\n");
  Serial.printf("[WiFi] heap=%u minheap=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap());

  // Prepare radio for WiFi scanning (prevents reboot after BLE)
  radio_prepare_wifi();


  Serial.printf("[WiFi] status=%d\n", (int)WiFi.status());

  // Blocking scan (this was the original reliable path)
  int n = g_wifi_show_hidden ? WiFi.scanNetworks(false, true) : WiFi.scanNetworks(false, false);
  Serial.printf("[WiFi] scanNetworks n=%d\n", n);

  apCount = 0;

  if (n > 0) {
    int lim = min(n, (int)MAX_APS);
    if ((int)g_wifi_max_results > 0 && lim > (int)g_wifi_max_results) lim = (int)g_wifi_max_results;

    for (int i = 0; i < lim; i++) {
      apList[i].ssid    = WiFi.SSID(i);
      apList[i].bssid   = WiFi.BSSIDstr(i);
      apList[i].rssi    = WiFi.RSSI(i);
      apList[i].encType = WiFi.encryptionType(i);
      apList[i].channel = WiFi.channel(i);
    }

    for (int i = 0; i < lim - 1; i++)
      for (int j = 0; j < lim - 1 - i; j++)
        if (apList[j].rssi < apList[j+1].rssi) {
          APEntry tmp = apList[j];
          apList[j] = apList[j+1];
          apList[j+1] = tmp;
        }

    apCount = lim;
  }

  WiFi.scanDelete();

  char buf[40];
  snprintf(buf, sizeof(buf), "%d found", apCount);
  lv_label_set_text(lbl_wifi_status, buf);

  wifi_selected_item = nullptr;
  for (int i = 0; i < apCount; i++) {
    lv_obj_t *item = lv_list_add_button(list_wifi, NULL, "");
    lv_obj_add_event_cb(item, cb_wifi_item_select, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_set_style_outline_width(item, 0, 0);

    lv_obj_set_style_bg_color(item,
      i % 2 == 0 ? lv_color_hex(0x1a1a2e) : lv_color_hex(0x16213e), 0);

    lv_obj_t *lbl = lv_obj_get_child(item, 0);

    String ssid = apList[i].ssid.length() > 0 ? apList[i].ssid : "(hidden)";
    if (ssid.length() > 20) ssid = ssid.substring(0, 19) + "~";

    char row[96];
    snprintf(row, sizeof(row), "%s  %s\n%d dBm  CH%ld",
      authModeName(apList[i].encType),
      ssid.c_str(),
      (int)apList[i].rssi,
      (long)apList[i].channel
    );

    lv_label_set_text(lbl, row);
    lv_obj_set_style_text_color(lbl, rssiColor(apList[i].rssi), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  }

  if (apCount == 0) lv_list_add_text(list_wifi, "No networks found");

  Serial.printf("[WiFi] ---- Scan end: %d found ----\n", apCount);
  rgb_set(0, 40, 0); // dim green = done
}

// ─────────────────────────────────────────────────────────────────────────────
// BLE scan + populate list
// ─────────────────────────────────────────────────────────────────────────────
static void run_ble_scan() {
  rgb_set(0, 0, 255); // Blue = BLE

  lv_label_set_text(lbl_ble_status, "Scanning 5s...");
  lv_obj_clean(list_ble);
  lv_timer_handler();


  // Free BLE heap before WiFi (prevents netif allocation asserts)

  // Release SPI bus before touching the radio — holding startWrite() open
  // while BLE stack initialises causes a crash on ESP32.
  tft.dmaWait();
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
  pScan->setActiveScan(g_ble_active_scan);
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(g_ble_scan_secs, false); // blocking
  pScan->stop();
  delay(30);
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
  snprintf(buf, sizeof(buf), "%d found", bleCount);
  lv_label_set_text(lbl_ble_status, buf);

  for (int i = 0; i < bleCount; i++) {
    lv_obj_t *item = lv_list_add_button(list_ble, NULL, "");
    // Make item selectable
    lv_obj_add_event_cb(item, cb_ble_item_select, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_set_style_outline_width(item, 0, 0);
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

    rgb_set(0, 0, 40); // dim blue = done

  // Note: keep BLE initialized so Rescan works reliably.
}


// ─────────────────────────────────────────────────────────────────────────────
// Detail view helpers
// ─────────────────────────────────────────────────────────────────────────────
static void show_wifi_detail(int idx);
static void show_ble_detail(int idx);

// ─────────────────────────────────────────────────────────────────────────────
// List item callbacks (make results selectable)
// ─────────────────────────────────────────────────────────────────────────────
static void cb_wifi_item_select(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  lv_obj_t *item = (lv_obj_t *)lv_event_get_target(e);
  int idx = (int)(intptr_t)lv_event_get_user_data(e);

  // Clear previous highlight
  if (wifi_selected_item && wifi_selected_item != item) {
    lv_obj_set_style_outline_width(wifi_selected_item, 0, 0);
  }
  wifi_selected_item = item;

  // Highlight this item
  lv_obj_set_style_outline_width(item, 2, 0);
  lv_obj_set_style_outline_color(item, lv_color_hex(0x07FFFF), 0);
  lv_obj_set_style_outline_opa(item, LV_OPA_COVER, 0);
  // Open detail screen
  show_wifi_detail(idx);
}

static void cb_ble_item_select(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  lv_obj_t *item = (lv_obj_t *)lv_event_get_target(e);
  int idx = (int)(intptr_t)lv_event_get_user_data(e);

  if (ble_selected_item && ble_selected_item != item) {
    lv_obj_set_style_outline_width(ble_selected_item, 0, 0);
  }
  ble_selected_item = item;

  lv_obj_set_style_outline_width(item, 2, 0);
  lv_obj_set_style_outline_color(item, lv_color_hex(0xCC88FF), 0);
  lv_obj_set_style_outline_opa(item, LV_OPA_COVER, 0);

  if (idx >= 0 && idx < bleCount) {
  }

  show_ble_detail(idx);
}

// ─────────────────────────────────────────────────────────────────────────────
// Detail screens: show selected item
// ─────────────────────────────────────────────────────────────────────────────
static void cb_detail_back(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const char *which = (const char *)lv_event_get_user_data(e);
  if (which && strcmp(which, "wifi") == 0) {
    lv_screen_load(scr_wifi);
  } else if (which && strcmp(which, "ble") == 0) {
    lv_screen_load(scr_ble);
  } else {
    lv_screen_load(scr_home);
  }
}

static void show_wifi_detail(int idx) {
  if (!scr_wifi_detail || !lbl_wifi_detail) return;
  if (idx < 0 || idx >= apCount) return;

  String ssid = apList[idx].ssid.length() ? apList[idx].ssid : "(hidden)";

  char buf[256];
  snprintf(buf, sizeof(buf),
    "SSID: %s\n"
    "MAC: %s\n"
    "Security: %s\n"
    "RSSI: %d dBm\n"
    "Channel: %ld",
    ssid.c_str(),
    apList[idx].bssid.c_str(),
    authModeName(apList[idx].encType),
    (int)apList[idx].rssi,
    (long)apList[idx].channel
  );

  lv_label_set_text(lbl_wifi_detail, buf);
  lv_screen_load(scr_wifi_detail);
}

static void show_ble_detail(int idx) {
  if (!scr_ble_detail || !lbl_ble_detail) return;
  if (idx < 0 || idx >= bleCount) return;

  String nm = bleList[idx].name.length() ? bleList[idx].name : "(unnamed)";

  char txBuf[16];
  if (bleList[idx].hasTxPower) {
    snprintf(txBuf, sizeof(txBuf), "%d dBm", (int)bleList[idx].txPower);
  } else {
    snprintf(txBuf, sizeof(txBuf), "N/A");
  }

  char buf[256];
  snprintf(buf, sizeof(buf),
    "Name: %s\n"
    "MAC: %s\n"
    "RSSI: %d dBm\n"
    "Svc UUIDs: %u\n"
    "Mfg Data: %u bytes\n"
    "TX Power: %s",
    nm.c_str(),
    bleList[idx].address.c_str(),
    (int)bleList[idx].rssi,
    (unsigned)bleList[idx].svcCount,
    (unsigned)bleList[idx].mfgLen,
    txBuf
  );

  lv_label_set_text(lbl_ble_detail, buf);
  lv_screen_load(scr_ble_detail);
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
// Settings callbacks
// ─────────────────────────────────────────────────────────────────────────────
static void cb_open_settings(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (scr_settings) lv_screen_load(scr_settings);
}

static void cb_settings_back(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_screen_load(scr_home);
}

// Apply dropdown/switch/slider values into runtime vars
static void cb_settings_apply(lv_event_t *e) {
  (void)e;

  // BLE scan seconds (dropdown text like "3", "5", etc.)

  if (dd_ble_secs) {
    char opt[8] = {0};
    lv_dropdown_get_selected_str(dd_ble_secs, opt, sizeof(opt));
    int v = atoi(opt);
    if (v < 1) v = 1;
    if (v > 20) v = 20;
    g_ble_scan_secs = (uint8_t)v;
  }


  // WiFi scan seconds
  if (dd_wifi_secs) {
    char opt[8] = {0};
    lv_dropdown_get_selected_str(dd_wifi_secs, opt, sizeof(opt));
    int v = atoi(opt);
    if (v < 1) v = 1;
    if (v > 20) v = 20;
    g_wifi_scan_secs = (uint8_t)v;
  }

  // WiFi max results
  if (dd_wifi_max) {
    char opt[8] = {0};
    lv_dropdown_get_selected_str(dd_wifi_max, opt, sizeof(opt));
    int v = atoi(opt);
    if (v < 5) v = 5;
    if (v > MAX_APS) v = MAX_APS;
    g_wifi_max_results = (uint8_t)v;
  }

  // BLE max results
  if (dd_ble_max) {
    char opt[8] = {0};
    lv_dropdown_get_selected_str(dd_ble_max, opt, sizeof(opt));
    int v = atoi(opt);
    if (v < 5) v = 5;
    if (v > MAX_BLES) v = MAX_BLES;
    g_ble_max_results = (uint8_t)v;
  }

  if (sw_hidden) g_wifi_show_hidden = lv_obj_has_state(sw_hidden, LV_STATE_CHECKED);
  if (sw_active) g_ble_active_scan  = lv_obj_has_state(sw_active, LV_STATE_CHECKED);
  if (sw_rgb)    g_rgb_enabled      = lv_obj_has_state(sw_rgb, LV_STATE_CHECKED);

  if (slider_bright) {
    g_brightness_pct = (uint8_t)lv_slider_get_value(slider_bright);
    bl_apply(g_brightness_pct);
  }


  // Sleep timeout dropdown
  if (dd_sleep) {
    char opt[12] = {0};
    lv_dropdown_get_selected_str(dd_sleep, opt, sizeof(opt));
    // Options look like: Off, 30s, 60s, 120s, 300s
    if (strcmp(opt, "Off") == 0) {
      g_sleep_timeout_s = 0;
    } else {
      int v = atoi(opt);
      if (v < 0) v = 0;
      if (v > 3600) v = 3600;
      g_sleep_timeout_s = (uint16_t)v;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: no-op callback
// ─────────────────────────────────────────────────────────────────────────────
static void cb_noop(lv_event_t *e) { (void)e; }

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
    lv_color_hex(0x0F7A4A), cb_go_wifi);
  lv_obj_set_size(btn_wifi, 200, 60);
  lv_obj_align(btn_wifi, LV_ALIGN_CENTER, 0, -50);

  // BLE button
  lv_obj_t *btn_ble = make_button(scr_home,
    LV_SYMBOL_BLUETOOTH "  BLE Scan",
    lv_color_hex(0x1E3A8A), cb_go_ble);
  lv_obj_set_size(btn_ble, 200, 60);
  lv_obj_align(btn_ble, LV_ALIGN_CENTER, 0, 30);

  // Settings button (small) under BLE scan, right side
  btn_settings = make_button(scr_home, LV_SYMBOL_SETTINGS, lv_color_hex(0x222244), cb_open_settings);
  lv_obj_set_size(btn_settings, 60, 36);
  lv_obj_align(btn_settings, LV_ALIGN_CENTER, 70, 94);

  // Footer
  lv_obj_t *footer = lv_label_create(scr_home);
  lv_label_set_text(footer, "Created By ATOMNFT");
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

  // ── WIFI DETAIL SCREEN ─────────────────────────────────────────────────────
  scr_wifi_detail = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_wifi_detail, bg_dark, 0);
  lv_obj_set_style_bg_opa(scr_wifi_detail, LV_OPA_COVER, 0);

  lv_obj_t *wd_hdr = lv_obj_create(scr_wifi_detail);
  lv_obj_set_size(wd_hdr, SCREEN_W, 46);
  lv_obj_set_style_bg_color(wd_hdr, lv_color_hex(0x001040), 0);
  lv_obj_set_style_border_width(wd_hdr, 0, 0);
  lv_obj_align(wd_hdr, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_pad_all(wd_hdr, 4, 0);

  lv_obj_t *wd_title = lv_label_create(wd_hdr);
  lv_label_set_text(wd_title, "WiFi Details");
  lv_obj_set_style_text_color(wd_title, lv_color_hex(0x07FFFF), 0);
  lv_obj_set_style_text_font(wd_title, &lv_font_montserrat_14, 0);
  lv_obj_align(wd_title, LV_ALIGN_LEFT_MID, 4, 0);

  lv_obj_t *wd_back = make_button(wd_hdr, LV_SYMBOL_LEFT " Back", lv_color_hex(0x222244), cb_noop);
  lv_obj_add_event_cb(wd_back, cb_detail_back, LV_EVENT_CLICKED, (void*)"wifi");
  lv_obj_set_size(wd_back, 80, 30);
  lv_obj_align(wd_back, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_user_data(wd_back, (void*)"wifi");

  lv_obj_t *wd_box = lv_obj_create(scr_wifi_detail);
  lv_obj_set_size(wd_box, SCREEN_W - 16, SCREEN_H - 46 - 16);
  lv_obj_align(wd_box, LV_ALIGN_TOP_MID, 0, 54);
  lv_obj_set_style_bg_color(wd_box, lv_color_hex(0x0D0D1A), 0);
  lv_obj_set_style_border_color(wd_box, lv_color_hex(0x00FF66), 0);
  lv_obj_set_style_border_width(wd_box, 1, 0);
  lv_obj_set_style_radius(wd_box, 10, 0);
  lv_obj_set_style_pad_all(wd_box, 10, 0);

  lbl_wifi_detail = lv_label_create(wd_box);
  lv_label_set_text(lbl_wifi_detail, "");
  lv_obj_set_style_text_color(lbl_wifi_detail, lv_color_white(), 0);
  lv_obj_set_style_text_font(lbl_wifi_detail, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(lbl_wifi_detail, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_wifi_detail, SCREEN_W - 36);
  lv_obj_align(lbl_wifi_detail, LV_ALIGN_TOP_LEFT, 0, 0);

  // ── BLE DETAIL SCREEN ──────────────────────────────────────────────────────
  scr_ble_detail = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_ble_detail, bg_dark, 0);
  lv_obj_set_style_bg_opa(scr_ble_detail, LV_OPA_COVER, 0);

  lv_obj_t *bd_hdr = lv_obj_create(scr_ble_detail);
  lv_obj_set_size(bd_hdr, SCREEN_W, 46);
  lv_obj_set_style_bg_color(bd_hdr, lv_color_hex(0x180030), 0);
  lv_obj_set_style_border_width(bd_hdr, 0, 0);
  lv_obj_align(bd_hdr, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_pad_all(bd_hdr, 4, 0);

  lv_obj_t *bd_title = lv_label_create(bd_hdr);
  lv_label_set_text(bd_title, "BLE Details");
  lv_obj_set_style_text_color(bd_title, lv_color_hex(0xCC88FF), 0);
  lv_obj_set_style_text_font(bd_title, &lv_font_montserrat_14, 0);
  lv_obj_align(bd_title, LV_ALIGN_LEFT_MID, 4, 0);

  lv_obj_t *bd_back = make_button(bd_hdr, LV_SYMBOL_LEFT " Back", lv_color_hex(0x222244), cb_noop);
  lv_obj_add_event_cb(bd_back, cb_detail_back, LV_EVENT_CLICKED, (void*)"ble");
  lv_obj_set_size(bd_back, 80, 30);
  lv_obj_align(bd_back, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_user_data(bd_back, (void*)"ble");

  lv_obj_t *bd_box = lv_obj_create(scr_ble_detail);
  lv_obj_set_size(bd_box, SCREEN_W - 16, SCREEN_H - 46 - 16);
  lv_obj_align(bd_box, LV_ALIGN_TOP_MID, 0, 54);
  lv_obj_set_style_bg_color(bd_box, lv_color_hex(0x0D0D1A), 0);
  lv_obj_set_style_border_color(bd_box, lv_color_hex(0x1E90FF), 0);
  lv_obj_set_style_border_width(bd_box, 1, 0);
  lv_obj_set_style_radius(bd_box, 10, 0);
  lv_obj_set_style_pad_all(bd_box, 10, 0);

  lbl_ble_detail = lv_label_create(bd_box);
  lv_label_set_text(lbl_ble_detail, "");
  lv_obj_set_style_text_color(lbl_ble_detail, lv_color_white(), 0);
  lv_obj_set_style_text_font(lbl_ble_detail, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(lbl_ble_detail, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_ble_detail, SCREEN_W - 36);
  lv_obj_align(lbl_ble_detail, LV_ALIGN_TOP_LEFT, 0, 0);

  // ── SETTINGS SCREEN ────────────────────────────────────────────────────────
  scr_settings = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_settings, bg_dark, 0);
  lv_obj_set_style_bg_opa(scr_settings, LV_OPA_COVER, 0);

  // Header
  lv_obj_t *st_hdr = lv_obj_create(scr_settings);
  lv_obj_set_size(st_hdr, SCREEN_W, 46);
  lv_obj_set_style_bg_color(st_hdr, lv_color_hex(0x101020), 0);
  lv_obj_set_style_border_width(st_hdr, 0, 0);
  lv_obj_align(st_hdr, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_pad_all(st_hdr, 4, 0);

  lv_obj_t *st_title = lv_label_create(st_hdr);
  lv_label_set_text(st_title, "Settings");
  lv_obj_set_style_text_color(st_title, lv_color_hex(0xAABBCC), 0);
  lv_obj_set_style_text_font(st_title, &lv_font_montserrat_14, 0);
  lv_obj_align(st_title, LV_ALIGN_LEFT_MID, 4, 0);

  lv_obj_t *st_back = make_button(st_hdr, LV_SYMBOL_LEFT " Back", lv_color_hex(0x222244), cb_settings_back);
  lv_obj_set_size(st_back, 80, 30);
  lv_obj_align(st_back, LV_ALIGN_RIGHT_MID, -4, 0);

  // Body container
  lv_obj_t *st = lv_obj_create(scr_settings);
  lv_obj_set_size(st, SCREEN_W - 16, SCREEN_H - 46 - 16);
  lv_obj_align(st, LV_ALIGN_TOP_MID, 0, 54);
  lv_obj_set_style_bg_color(st, bg_dark, 0);
  lv_obj_set_style_border_color(st, lv_color_hex(0x2a2a44), 0);
  lv_obj_set_style_border_width(st, 1, 0);
  lv_obj_set_style_radius(st, 10, 0);
  lv_obj_set_style_pad_all(st, 10, 0);
  lv_obj_set_style_pad_row(st, 8, 0);
  lv_obj_set_flex_flow(st, LV_FLEX_FLOW_COLUMN);

  // Helper macro to create a row
  #define MAKE_ROW(_label, _row) \
    lv_obj_t *_row = lv_obj_create(st); \
    lv_obj_set_size(_row, SCREEN_W - 36, LV_SIZE_CONTENT); \
    lv_obj_set_style_bg_opa(_row, LV_OPA_TRANSP, 0); \
    lv_obj_set_style_border_width(_row, 0, 0); \
    lv_obj_set_style_pad_all(_row, 0, 0); \
    lv_obj_set_flex_flow(_row, LV_FLEX_FLOW_ROW); \
    lv_obj_set_flex_align(_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER); \
    lv_obj_t *_row##_lab = lv_label_create(_row); \
    lv_label_set_text(_row##_lab, _label); \
    lv_obj_set_style_text_color(_row##_lab, lv_color_hex(0xDDE6FF), 0); \
    lv_obj_set_style_text_font(_row##_lab, &lv_font_montserrat_14, 0);

  // BLE scan seconds
  {
    MAKE_ROW("BLE Scan (sec)", row1);
    dd_ble_secs = lv_dropdown_create(row1);
    lv_dropdown_set_options(dd_ble_secs, "3\n5\n8\n10\n12\n15");
    // select based on current value (fallback to 5)
    int sel = 1;
    if (g_ble_scan_secs == 3) sel = 0;
    else if (g_ble_scan_secs == 5) sel = 1;
    else if (g_ble_scan_secs == 8) sel = 2;
    else if (g_ble_scan_secs == 10) sel = 3;
    else if (g_ble_scan_secs == 12) sel = 4;
    else if (g_ble_scan_secs == 15) sel = 5;
    lv_dropdown_set_selected(dd_ble_secs, sel);
    lv_obj_set_width(dd_ble_secs, 70);
    lv_obj_add_event_cb(dd_ble_secs, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // WiFi scan seconds
  {
    MAKE_ROW("WiFi Scan (sec)", row1b);
    dd_wifi_secs = lv_dropdown_create(row1b);
    lv_dropdown_set_options(dd_wifi_secs, "3\n5\n8\n10\n12\n15");
    int sel = 1;
    if (g_wifi_scan_secs == 3) sel = 0;
    else if (g_wifi_scan_secs == 5) sel = 1;
    else if (g_wifi_scan_secs == 8) sel = 2;
    else if (g_wifi_scan_secs == 10) sel = 3;
    else if (g_wifi_scan_secs == 12) sel = 4;
    else if (g_wifi_scan_secs == 15) sel = 5;
    lv_dropdown_set_selected(dd_wifi_secs, sel);
    lv_obj_set_width(dd_wifi_secs, 70);
    lv_obj_add_event_cb(dd_wifi_secs, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // WiFi max results
  {
    MAKE_ROW("WiFi Max", row2);
    dd_wifi_max = lv_dropdown_create(row2);
    lv_dropdown_set_options(dd_wifi_max, "10\n20");
    lv_dropdown_set_selected(dd_wifi_max, (g_wifi_max_results <= 10) ? 0 : 1);
    lv_obj_set_width(dd_wifi_max, 70);
    lv_obj_add_event_cb(dd_wifi_max, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // BLE max results
  {
    MAKE_ROW("BLE Max", row3);
    dd_ble_max = lv_dropdown_create(row3);
    lv_dropdown_set_options(dd_ble_max, "10\n20");
    lv_dropdown_set_selected(dd_ble_max, (g_ble_max_results <= 10) ? 0 : 1);
    lv_obj_set_width(dd_ble_max, 70);
    lv_obj_add_event_cb(dd_ble_max, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // WiFi show hidden
  {
    MAKE_ROW("WiFi Hidden", row4);
    sw_hidden = lv_switch_create(row4);
    if (g_wifi_show_hidden) lv_obj_add_state(sw_hidden, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_hidden, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // BLE active scan
  {
    MAKE_ROW("BLE Active", row5);
    sw_active = lv_switch_create(row5);
    if (g_ble_active_scan) lv_obj_add_state(sw_active, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_active, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // Brightness
  {
    MAKE_ROW("Brightness", row6);
    slider_bright = lv_slider_create(row6);
    lv_slider_set_range(slider_bright, 5, 100);
    lv_slider_set_value(slider_bright, g_brightness_pct, LV_ANIM_OFF);
    lv_obj_set_width(slider_bright, 120);
    lv_obj_add_event_cb(slider_bright, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // Sleep (presets)
  {
    MAKE_ROW("Sleep", row7);
    dd_sleep = lv_dropdown_create(row7);
    lv_dropdown_set_options(dd_sleep, "Off\n30\n60\n120\n300\n600");

    int sel = 0;
    if (g_sleep_timeout_s == 30) sel = 1;
    else if (g_sleep_timeout_s == 60) sel = 2;
    else if (g_sleep_timeout_s == 120) sel = 3;
    else if (g_sleep_timeout_s == 300) sel = 4;
    else if (g_sleep_timeout_s == 600) sel = 5;

    lv_dropdown_set_selected(dd_sleep, sel);
    lv_obj_set_width(dd_sleep, 90);
    lv_obj_add_event_cb(dd_sleep, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // RGB LED enable
  {
    MAKE_ROW("RGB LED", row8);
    sw_rgb = lv_switch_create(row8);
    if (g_rgb_enabled) lv_obj_add_state(sw_rgb, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_rgb, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  #undef MAKE_ROW

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


  // TFT + Touch init (required before LVGL flush can draw)
  tft.begin();
  tft.setRotation(0);
  tft.setSwapBytes(true);

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


  // Backlight (PWM)
  bl_init();
  bl_apply(g_brightness_pct);

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

  build_ui();
  lv_screen_load(scr_home);

  g_last_touch_ms = millis();

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

  // Sleep timeout handling: dim backlight after inactivity
  if (g_sleep_timeout_s > 0) {
    uint32_t idle_ms = millis() - g_last_touch_ms;
    if (!g_dimmed && idle_ms > (uint32_t)g_sleep_timeout_s * 1000UL) {
      g_dimmed = true;
      bl_write(g_sleep_dim_pct);
    }
  }

  delay(5);
}
/*
  ============================================================================
  JC2432W328C Scanner
  ============================================================================
  Created by ATOMNFT.
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
#include <Preferences.h>
#include "esp_wifi.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <string>

#include "config.h"  // Some settings chose to live here.


static Preferences prefs;
static const char *PREF_NS = "jcscan";

// ─────────────────────────────────────────────────────────────────────────────
// Theme runtime colors
// ─────────────────────────────────────────────────────────────────────────────
static uint8_t g_theme_idx = (uint8_t)CFG_THEME_DEFAULT;

// Runtime LVGL colors (set by theme_apply_palette)
static lv_color_t COL_STATUS_BG;
static lv_color_t COL_SCREEN_BG;
static lv_color_t COL_BORDER;
static lv_color_t COL_WIFI_BTN;
static lv_color_t COL_BLE_BTN;
static lv_color_t COL_HDR_WIFI_BG;
static lv_color_t COL_HDR_BLE_BG;
static lv_color_t COL_HDR_SETTINGS_BG;
static lv_color_t COL_LIST_WIFI_EVEN;
static lv_color_t COL_LIST_WIFI_ODD;
static lv_color_t COL_LIST_BLE_EVEN;
static lv_color_t COL_LIST_BLE_ODD;
static lv_color_t COL_BTN_TEXT;
static lv_color_t COL_TITLE_TEXT;
static lv_color_t COL_SUBTITLE_TEXT;
static lv_color_t COL_HDR_WIFI_TEXT;
static lv_color_t COL_HDR_BLE_TEXT;
static lv_color_t COL_HDR_SETTINGS_TEXT;
static lv_color_t COL_LIST_TEXT;
static lv_color_t COL_PANEL_BG;
static lv_color_t COL_PANEL_TEXT;
static lv_color_t COL_BACK_BTN_BG;
static lv_color_t COL_SEL_BG;

static void theme_apply_palette() {
  switch (g_theme_idx) {
    default:
    case 0:
      COL_STATUS_BG       = lv_color_hex(T0_STATUS_BG);
      COL_SCREEN_BG       = lv_color_hex(T0_SCREEN_BG);
      COL_BORDER          = lv_color_hex(T0_BORDER);
      COL_WIFI_BTN        = lv_color_hex(T0_WIFI_BTN);
      COL_BLE_BTN         = lv_color_hex(T0_BLE_BTN);
      COL_HDR_WIFI_BG     = lv_color_hex(T0_HDR_WIFI_BG);
      COL_HDR_BLE_BG      = lv_color_hex(T0_HDR_BLE_BG);
      COL_HDR_SETTINGS_BG = lv_color_hex(T0_HDR_SETTINGS_BG);
      COL_LIST_WIFI_EVEN  = lv_color_hex(T0_LIST_WIFI_EVEN);
      COL_LIST_WIFI_ODD   = lv_color_hex(T0_LIST_WIFI_ODD);
      COL_LIST_BLE_EVEN   = lv_color_hex(T0_LIST_BLE_EVEN);
      COL_LIST_BLE_ODD    = lv_color_hex(T0_LIST_BLE_ODD);
      COL_BTN_TEXT       = lv_color_hex(T0_BTN_TEXT);
      COL_TITLE_TEXT     = lv_color_hex(T0_TITLE_TEXT);
      COL_SUBTITLE_TEXT  = lv_color_hex(T0_SUBTITLE_TEXT);
      COL_HDR_WIFI_TEXT  = lv_color_hex(T0_HDR_WIFI_TEXT);
      COL_HDR_BLE_TEXT   = lv_color_hex(T0_HDR_BLE_TEXT);
      COL_HDR_SETTINGS_TEXT = lv_color_hex(T0_HDR_SETTINGS_TEXT);
      COL_LIST_TEXT       = lv_color_hex(T0_LIST_TEXT);
      COL_PANEL_BG       = lv_color_hex(T0_PANEL_BG);
      COL_PANEL_TEXT     = lv_color_hex(T0_PANEL_TEXT);
      COL_BACK_BTN_BG    = lv_color_hex(T0_BACK_BTN_BG);
      COL_SEL_BG        = lv_color_hex(T0_SEL_BG);
break;
    case 1:
      COL_STATUS_BG       = lv_color_hex(T1_STATUS_BG);
      COL_SCREEN_BG       = lv_color_hex(T1_SCREEN_BG);
      COL_BORDER          = lv_color_hex(T1_BORDER);
      COL_WIFI_BTN        = lv_color_hex(T1_WIFI_BTN);
      COL_BLE_BTN         = lv_color_hex(T1_BLE_BTN);
      COL_HDR_WIFI_BG     = lv_color_hex(T1_HDR_WIFI_BG);
      COL_HDR_BLE_BG      = lv_color_hex(T1_HDR_BLE_BG);
      COL_HDR_SETTINGS_BG = lv_color_hex(T1_HDR_SETTINGS_BG);
      COL_LIST_WIFI_EVEN  = lv_color_hex(T1_LIST_WIFI_EVEN);
      COL_LIST_WIFI_ODD   = lv_color_hex(T1_LIST_WIFI_ODD);
      COL_LIST_BLE_EVEN   = lv_color_hex(T1_LIST_BLE_EVEN);
      COL_LIST_BLE_ODD    = lv_color_hex(T1_LIST_BLE_ODD);
      COL_BTN_TEXT       = lv_color_hex(T1_BTN_TEXT);
      COL_TITLE_TEXT     = lv_color_hex(T1_TITLE_TEXT);
      COL_SUBTITLE_TEXT  = lv_color_hex(T1_SUBTITLE_TEXT);
      COL_HDR_WIFI_TEXT  = lv_color_hex(T1_HDR_WIFI_TEXT);
      COL_HDR_BLE_TEXT   = lv_color_hex(T1_HDR_BLE_TEXT);
      COL_HDR_SETTINGS_TEXT = lv_color_hex(T1_HDR_SETTINGS_TEXT);
      COL_LIST_TEXT       = lv_color_hex(T1_LIST_TEXT);
      COL_PANEL_BG       = lv_color_hex(T1_PANEL_BG);
      COL_PANEL_TEXT     = lv_color_hex(T1_PANEL_TEXT);
      COL_BACK_BTN_BG    = lv_color_hex(T1_BACK_BTN_BG);
      COL_SEL_BG        = lv_color_hex(T1_SEL_BG);
break;
    case 2:
      COL_STATUS_BG       = lv_color_hex(T2_STATUS_BG);
      COL_SCREEN_BG       = lv_color_hex(T2_SCREEN_BG);
      COL_BORDER          = lv_color_hex(T2_BORDER);
      COL_WIFI_BTN        = lv_color_hex(T2_WIFI_BTN);
      COL_BLE_BTN         = lv_color_hex(T2_BLE_BTN);
      COL_HDR_WIFI_BG     = lv_color_hex(T2_HDR_WIFI_BG);
      COL_HDR_BLE_BG      = lv_color_hex(T2_HDR_BLE_BG);
      COL_HDR_SETTINGS_BG = lv_color_hex(T2_HDR_SETTINGS_BG);
      COL_LIST_WIFI_EVEN  = lv_color_hex(T2_LIST_WIFI_EVEN);
      COL_LIST_WIFI_ODD   = lv_color_hex(T2_LIST_WIFI_ODD);
      COL_LIST_BLE_EVEN   = lv_color_hex(T2_LIST_BLE_EVEN);
      COL_LIST_BLE_ODD    = lv_color_hex(T2_LIST_BLE_ODD);
      COL_BTN_TEXT       = lv_color_hex(T2_BTN_TEXT);
      COL_TITLE_TEXT     = lv_color_hex(T2_TITLE_TEXT);
      COL_SUBTITLE_TEXT  = lv_color_hex(T2_SUBTITLE_TEXT);
      COL_HDR_WIFI_TEXT  = lv_color_hex(T2_HDR_WIFI_TEXT);
      COL_HDR_BLE_TEXT   = lv_color_hex(T2_HDR_BLE_TEXT);
      COL_HDR_SETTINGS_TEXT = lv_color_hex(T2_HDR_SETTINGS_TEXT);
      COL_LIST_TEXT       = lv_color_hex(T2_LIST_TEXT);
      COL_PANEL_BG       = lv_color_hex(T2_PANEL_BG);
      COL_PANEL_TEXT     = lv_color_hex(T2_PANEL_TEXT);
      COL_BACK_BTN_BG    = lv_color_hex(T2_BACK_BTN_BG);
      COL_SEL_BG        = lv_color_hex(T2_SEL_BG);
break;
  
    case 3:
      COL_STATUS_BG       = lv_color_hex(T3_STATUS_BG);
      COL_SCREEN_BG       = lv_color_hex(T3_SCREEN_BG);
      COL_BORDER          = lv_color_hex(T3_BORDER);
      COL_WIFI_BTN        = lv_color_hex(T3_WIFI_BTN);
      COL_BLE_BTN         = lv_color_hex(T3_BLE_BTN);
      COL_HDR_WIFI_BG     = lv_color_hex(T3_HDR_WIFI_BG);
      COL_HDR_BLE_BG      = lv_color_hex(T3_HDR_BLE_BG);
      COL_HDR_SETTINGS_BG = lv_color_hex(T3_HDR_SETTINGS_BG);
      COL_LIST_WIFI_EVEN  = lv_color_hex(T3_LIST_WIFI_EVEN);
      COL_LIST_WIFI_ODD   = lv_color_hex(T3_LIST_WIFI_ODD);
      COL_LIST_BLE_EVEN   = lv_color_hex(T3_LIST_BLE_EVEN);
      COL_LIST_BLE_ODD    = lv_color_hex(T3_LIST_BLE_ODD);
      COL_BTN_TEXT        = lv_color_hex(T3_BTN_TEXT);
      COL_TITLE_TEXT      = lv_color_hex(T3_TITLE_TEXT);
      COL_SUBTITLE_TEXT   = lv_color_hex(T3_SUBTITLE_TEXT);
      COL_HDR_WIFI_TEXT   = lv_color_hex(T3_HDR_WIFI_TEXT);
      COL_HDR_BLE_TEXT    = lv_color_hex(T3_HDR_BLE_TEXT);
      COL_HDR_SETTINGS_TEXT = lv_color_hex(T3_HDR_SETTINGS_TEXT);
      COL_LIST_TEXT       = lv_color_hex(T3_LIST_TEXT);
      COL_PANEL_BG       = lv_color_hex(T3_PANEL_BG);
      COL_PANEL_TEXT     = lv_color_hex(T3_PANEL_TEXT);
      COL_BACK_BTN_BG    = lv_color_hex(T3_BACK_BTN_BG);
      COL_SEL_BG        = lv_color_hex(T3_SEL_BG);
break;
}
}

static void theme_apply_to_ui();


// (theme_apply_to_ui removed)


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

static void rgb_boot_test() {
#if defined(CFG_RGB_BOOT_TEST)
  if (!CFG_RGB_BOOT_TEST) return;
#endif
  if (!g_rgb_enabled) return;

  const uint16_t ms =
#if defined(CFG_RGB_BOOT_TEST_MS)
    (uint16_t)CFG_RGB_BOOT_TEST_MS;
#else
    120;
#endif

  rgb_set(255, 0, 0); delay(ms);
  rgb_set(0, 255, 0); delay(ms);
  rgb_set(0, 0, 255); delay(ms);
  rgb_set(0, 0, 0);   delay(40);
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
static bool     g_rssi_absolute  = CFG_RSSI_ABSOLUTE; // false: -65 dBm, true: 65
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

static void set_backlight_pct(uint8_t pct) { bl_apply(pct); }

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


// ─────────────────────────────────────────────────────────────────────────────
// Status bar + radio state
// ─────────────────────────────────────────────────────────────────────────────
enum RadioState : uint8_t { RADIO_IDLE=0, RADIO_WIFI_SCAN=1, RADIO_BLE_SCAN=2 };
static volatile RadioState g_radio_state = RADIO_IDLE;

static lv_obj_t *lbl_status_home = nullptr;
static lv_obj_t *lbl_status_wifi = nullptr;
static lv_obj_t *lbl_status_ble  = nullptr;
static lv_obj_t *lbl_status_wifi_detail = nullptr;
static lv_obj_t *lbl_status_ble_detail  = nullptr;
static lv_obj_t *lbl_status_settings    = nullptr;

static uint32_t g_boot_ms = 0;


// ─────────────────────────────────────────────────────────────────────────────
// Scan request state machine (prevents running scans inside LVGL callbacks)
// ─────────────────────────────────────────────────────────────────────────────
enum ScanReq : uint8_t { REQ_NONE=0, REQ_WIFI=1, REQ_BLE=2 };
static volatile ScanReq g_scan_req = REQ_NONE;
static bool g_wifi_scanning = false;
static bool g_ble_scanning  = false;
// ── LVGL screens ─────────────────────────────────────────────────────────────
static lv_obj_t *scr_home    = nullptr;
static lv_obj_t *scr_wifi    = nullptr;
static lv_obj_t *scr_ble     = nullptr;
static lv_obj_t *scr_settings = nullptr;
static lv_obj_t *scr_reset        = nullptr;
static lv_obj_t *reset_hdr        = nullptr;
static lv_obj_t *reset_box        = nullptr;
static lv_obj_t *lbl_reset_body   = nullptr;
static lv_obj_t *btn_reset_ok     = nullptr;
static lv_obj_t *btn_reset_cancel = nullptr;
static lv_obj_t *scr_wifi_detail = nullptr;
static lv_obj_t *scr_ble_detail  = nullptr;
static lv_obj_t *lbl_wifi_status = nullptr;
static lv_obj_t *lbl_ble_status  = nullptr;
static lv_obj_t *list_wifi   = nullptr;
static lv_obj_t *list_ble    = nullptr;

static lv_obj_t *wifi_bar = nullptr;
static lv_obj_t *ble_bar  = nullptr;
static lv_obj_t *btn_wifi_home = nullptr;
static lv_obj_t *btn_wifi_rescan = nullptr;
static lv_obj_t *btn_ble_home = nullptr;
static lv_obj_t *btn_ble_rescan = nullptr;
// Theme-target UI objects (need globals so theme_apply_to_ui can recolor them)
static lv_obj_t *wifi_hdr = nullptr;
static lv_obj_t *ble_hdr  = nullptr;
static lv_obj_t *st_hdr   = nullptr;
static lv_obj_t *st_back = nullptr;
static lv_obj_t *st_body = nullptr;
static lv_obj_t *btn_wifi = nullptr;
static lv_obj_t *btn_ble  = nullptr;
static lv_obj_t *btn_settings = nullptr;
static lv_obj_t *lbl_home_title = nullptr;
static lv_obj_t *lbl_home_sub   = nullptr;
static lv_obj_t *lbl_wifi_title = nullptr;
static lv_obj_t *lbl_ble_title  = nullptr;
static lv_obj_t *lbl_settings_title = nullptr;
static lv_obj_t *wd_hdr = nullptr;
static lv_obj_t *bd_hdr = nullptr;
static lv_obj_t *wd_title = nullptr;
static lv_obj_t *bd_title = nullptr;
static lv_obj_t *wd_box = nullptr;
static lv_obj_t *bd_box = nullptr;
static lv_obj_t *wd_back = nullptr;
static lv_obj_t *bd_back = nullptr;
static lv_obj_t *slider_bright = nullptr;
static lv_obj_t *slider_sleep  = nullptr; // (deprecated, replaced by dd_sleep)
static lv_obj_t *dd_sleep        = nullptr;
static lv_obj_t *btn_reset_all  = nullptr;
static lv_obj_t *dd_ble_secs    = nullptr;
static lv_obj_t *dd_wifi_secs   = nullptr;
static lv_obj_t *dd_wifi_max    = nullptr;
static lv_obj_t *dd_ble_max     = nullptr;
static lv_obj_t *sw_hidden      = nullptr;
static lv_obj_t *sw_active      = nullptr;
static lv_obj_t *sw_rgb         = nullptr;
static lv_obj_t *sw_rssi_abs    = nullptr;
static lv_obj_t *dd_theme      = nullptr;
static lv_obj_t *lbl_wifi_detail = nullptr;
static lv_obj_t *lbl_ble_detail  = nullptr;


// ── Selection tracking (clickable list items)
static lv_obj_t *wifi_selected_item = nullptr;

static int wifi_selected_idx = -1;
static lv_obj_t *ble_selected_item  = nullptr;


static int ble_selected_idx = -1;
static void theme_apply_to_ui() {
  theme_apply_palette();

  // Status bar backgrounds (parent of each label)
  if (lbl_status_home) lv_obj_set_style_bg_color(lv_obj_get_parent(lbl_status_home), COL_STATUS_BG, 0);
  if (lbl_status_wifi) lv_obj_set_style_bg_color(lv_obj_get_parent(lbl_status_wifi), COL_STATUS_BG, 0);
  if (lbl_status_ble)  lv_obj_set_style_bg_color(lv_obj_get_parent(lbl_status_ble),  COL_STATUS_BG, 0);
  if (lbl_status_wifi_detail) lv_obj_set_style_bg_color(lv_obj_get_parent(lbl_status_wifi_detail), COL_STATUS_BG, 0);
  if (lbl_status_ble_detail)  lv_obj_set_style_bg_color(lv_obj_get_parent(lbl_status_ble_detail),  COL_STATUS_BG, 0);
  if (lbl_status_settings)    lv_obj_set_style_bg_color(lv_obj_get_parent(lbl_status_settings),    COL_STATUS_BG, 0);

  // Screen backgrounds
  if (scr_home) lv_obj_set_style_bg_color(scr_home, COL_SCREEN_BG, 0);
  if (scr_wifi) lv_obj_set_style_bg_color(scr_wifi, COL_SCREEN_BG, 0);
  if (scr_ble)  lv_obj_set_style_bg_color(scr_ble,  COL_SCREEN_BG, 0);
  if (scr_settings) lv_obj_set_style_bg_color(scr_settings, COL_SCREEN_BG, 0);
  if (scr_wifi_detail) lv_obj_set_style_bg_color(scr_wifi_detail, COL_SCREEN_BG, 0);
  if (scr_ble_detail)  lv_obj_set_style_bg_color(scr_ble_detail,  COL_SCREEN_BG, 0);

  // Home title/subtitle
  if (lbl_home_title) lv_obj_set_style_text_color(lbl_home_title, COL_TITLE_TEXT, 0);
  if (lbl_home_sub)   lv_obj_set_style_text_color(lbl_home_sub,   COL_SUBTITLE_TEXT, 0);

  // Header titles
  if (lbl_wifi_title) lv_obj_set_style_text_color(lbl_wifi_title, COL_HDR_WIFI_TEXT, 0);
  if (lbl_ble_title)  lv_obj_set_style_text_color(lbl_ble_title,  COL_HDR_BLE_TEXT, 0);
  if (lbl_settings_title) lv_obj_set_style_text_color(lbl_settings_title, COL_HDR_SETTINGS_TEXT, 0);

  // Headers
  if (wifi_hdr) lv_obj_set_style_bg_color(wifi_hdr, COL_HDR_WIFI_BG, 0);
  if (ble_hdr)  lv_obj_set_style_bg_color(ble_hdr,  COL_HDR_BLE_BG, 0);
  if (st_hdr)   lv_obj_set_style_bg_color(st_hdr,   COL_HDR_SETTINGS_BG, 0);

  
  
  // Settings page (header + body container + controls)
  // Reset confirm screen
  if (scr_reset) lv_obj_set_style_bg_color(scr_reset, COL_SCREEN_BG, 0);
  if (reset_hdr) lv_obj_set_style_bg_color(reset_hdr, COL_HDR_SETTINGS_BG, 0);
  if (reset_box) {
    lv_obj_set_style_bg_color(reset_box, COL_PANEL_BG, 0);
    lv_obj_set_style_border_color(reset_box, COL_BORDER, 0);
  }
  if (lbl_reset_body) lv_obj_set_style_text_color(lbl_reset_body, COL_PANEL_TEXT, 0);

  if (st_hdr) lv_obj_set_style_bg_color(st_hdr, COL_HDR_SETTINGS_BG, 0);
  if (lbl_settings_title) lv_obj_set_style_text_color(lbl_settings_title, COL_HDR_SETTINGS_TEXT, 0);

  if (st_back) {
    lv_obj_set_style_bg_color(st_back, COL_BACK_BTN_BG, 0);
    lv_obj_set_style_border_color(st_back, COL_BORDER, 0);
    lv_obj_t *lbl = lv_obj_get_child(st_back, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  }

  if (st_body) {
    lv_obj_set_style_bg_color(st_body, COL_PANEL_BG, 0);
    lv_obj_set_style_border_color(st_body, COL_BORDER, 0);
  }

  // Re-style settings controls (dropdowns/sliders/switches) if they exist
  settings_style_dropdown(dd_ble_secs);
  settings_style_dropdown(dd_wifi_secs);
  settings_style_dropdown(dd_wifi_max);
  settings_style_dropdown(dd_ble_max);
  settings_style_dropdown(dd_theme);
  settings_style_dropdown(dd_sleep);
  settings_style_slider(slider_bright);
  settings_style_switch(sw_hidden);
  settings_style_switch(sw_active);
  settings_style_switch(sw_rgb);
  settings_style_switch(sw_rssi_abs);
  settings_style_switch(sw_rssi_abs);
  settings_style_switch(sw_rssi_abs);

// Detail pages (headers + panels + text)
  if (wd_hdr) lv_obj_set_style_bg_color(wd_hdr, COL_HDR_WIFI_BG, 0);
  if (bd_hdr) lv_obj_set_style_bg_color(bd_hdr, COL_HDR_BLE_BG, 0);
  if (wd_title) lv_obj_set_style_text_color(wd_title, COL_HDR_WIFI_TEXT, 0);
  if (bd_title) lv_obj_set_style_text_color(bd_title, COL_HDR_BLE_TEXT, 0);

  if (wd_back) {
    lv_obj_set_style_bg_color(wd_back, COL_BACK_BTN_BG, 0);
    lv_obj_set_style_border_color(wd_back, COL_BORDER, 0);
    lv_obj_t *lbl = lv_obj_get_child(wd_back, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  }
  if (bd_back) {
    lv_obj_set_style_bg_color(bd_back, COL_BACK_BTN_BG, 0);
    lv_obj_set_style_border_color(bd_back, COL_BORDER, 0);
    lv_obj_t *lbl = lv_obj_get_child(bd_back, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  }

  if (wd_box) {
    lv_obj_set_style_bg_color(wd_box, COL_PANEL_BG, 0);
    lv_obj_set_style_border_color(wd_box, COL_BORDER, 0);
  }
  if (bd_box) {
    lv_obj_set_style_bg_color(bd_box, COL_PANEL_BG, 0);
    lv_obj_set_style_border_color(bd_box, COL_BORDER, 0);
  }
  if (lbl_wifi_detail) lv_obj_set_style_text_color(lbl_wifi_detail, COL_PANEL_TEXT, 0);
  if (lbl_ble_detail)  lv_obj_set_style_text_color(lbl_ble_detail,  COL_PANEL_TEXT, 0);


  // Results page bars (bottom Home/Rescan)
  if (wifi_bar) lv_obj_set_style_bg_color(wifi_bar, COL_HDR_WIFI_BG, 0);
  if (ble_bar)  lv_obj_set_style_bg_color(ble_bar,  COL_HDR_BLE_BG, 0);

  if (btn_wifi_home) {
    lv_obj_set_style_bg_color(btn_wifi_home, COL_BACK_BTN_BG, 0);
    lv_obj_set_style_border_color(btn_wifi_home, COL_BORDER, 0);
    lv_obj_t *lbl = lv_obj_get_child(btn_wifi_home, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  }
  if (btn_wifi_rescan) {
    lv_obj_set_style_bg_color(btn_wifi_rescan, COL_WIFI_BTN, 0);
    lv_obj_set_style_border_color(btn_wifi_rescan, COL_BORDER, 0);
    lv_obj_t *lbl = lv_obj_get_child(btn_wifi_rescan, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  }

  if (btn_ble_home) {
    lv_obj_set_style_bg_color(btn_ble_home, COL_BACK_BTN_BG, 0);
    lv_obj_set_style_border_color(btn_ble_home, COL_BORDER, 0);
    lv_obj_t *lbl = lv_obj_get_child(btn_ble_home, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  }
  if (btn_ble_rescan) {
    lv_obj_set_style_bg_color(btn_ble_rescan, COL_BLE_BTN, 0);
    lv_obj_set_style_border_color(btn_ble_rescan, COL_BORDER, 0);
    lv_obj_t *lbl = lv_obj_get_child(btn_ble_rescan, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  }

// Home buttons (bg + border + label text)
  if (btn_wifi) {
    lv_obj_set_style_bg_color(btn_wifi, COL_WIFI_BTN, 0);
    lv_obj_set_style_border_color(btn_wifi, COL_BORDER, 0);
    lv_obj_t *lbl = lv_obj_get_child(btn_wifi, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  }
  if (btn_ble) {
    lv_obj_set_style_bg_color(btn_ble, COL_BLE_BTN, 0);
    lv_obj_set_style_border_color(btn_ble, COL_BORDER, 0);
    lv_obj_t *lbl = lv_obj_get_child(btn_ble, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  }
  if (btn_settings) {
    lv_obj_set_style_border_color(btn_settings, COL_BORDER, 0);
    lv_obj_t *lbl = lv_obj_get_child(btn_settings, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  }


}

// ─────────────────────────────────────────────────────────────────────────────
// BLE scan callback
// ─────────────────────────────────────────────────────────────────────────────
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    if (bleCount >= MAX_BLES) return;

    String addr = dev.getAddress().toString().c_str();

    // Dedup by MAC
    for (int i = 0; i < bleCount; i++) {
      if (bleList[i].address == addr) return;
    }

    BLEEntry &e = bleList[bleCount++];
    e.address = addr;
    e.rssi    = dev.getRSSI();
    e.hasName = dev.haveName();
    e.name    = e.hasName ? String(dev.getName().c_str()) : String("(unnamed)");

    // Advertised info
    e.svcCount = (uint8_t)dev.getServiceUUIDCount();
    {
      std::string md = dev.getManufacturerData();
      e.mfgLen = (uint16_t)md.length();
    }
    e.hasTxPower = dev.haveTXPower();
    e.txPower    = e.hasTxPower ? (int8_t)dev.getTXPower() : (int8_t)0;
  }
};

static ScanCallbacks scanCB;
 // keep callback alive (global)

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

// ─────────────────────────────────────────────────────────────────────────────
// RSSI formatting
// ─────────────────────────────────────────────────────────────────────────────
static void format_rssi(int rssi, char *out, size_t out_sz) {
  if (!out || out_sz == 0) return;
  if (g_rssi_absolute) {
    snprintf(out, out_sz, "%d", (int)abs(rssi));
  } else {
    snprintf(out, out_sz, "%d dBm", (int)rssi);
  }
}


// ─────────────────────────────────────────────────────────────────────────────
// WiFi security label
// ─────────────────────────────────────────────────────────────────────────────
static const char* wifi_auth_name(uint8_t encType) {
  // Arduino-ESP32 WiFi.encryptionType returns wifi_auth_mode_t values.
  switch ((wifi_auth_mode_t)encType) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2+WPA3";
#if defined(WIFI_AUTH_WAPI_PSK)
    case WIFI_AUTH_WAPI_PSK:        return "WAPI";
#endif
    default:                        return "SEC";
  }
}

static void run_wifi_scan() {
  g_wifi_scanning = true;
  g_radio_state = RADIO_WIFI_SCAN;

  status_bar_update_now();
  lv_timer_handler();

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
    // Themed row style (remove default white borders)
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_set_style_outline_width(item, 0, 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(item, 8, 0);
    lv_obj_set_style_pad_all(item, 6, 0);
    lv_obj_add_event_cb(item, cb_wifi_item_select, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_set_style_outline_width(item, 0, 0);
    lv_obj_set_style_outline_pad(item, 0, 0);
    lv_obj_set_style_outline_color(item, COL_BORDER, 0);

    lv_obj_set_style_bg_color(item,
      i % 2 == 0 ? COL_LIST_WIFI_EVEN : COL_LIST_WIFI_ODD, 0);

    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (!lbl) continue;

    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    String ssid = apList[i].ssid.length() > 0 ? apList[i].ssid : "(hidden)";
    if (ssid.length() > 20) ssid = ssid.substring(0, 19) + "~";

    char row[96];
    char rssiBuf[16];
    format_rssi((int)apList[i].rssi, rssiBuf, sizeof(rssiBuf));

    snprintf(row, sizeof(row), "%s  %s\n%s  CH%ld",
      authModeName(apList[i].encType),
      ssid.c_str(),
      rssiBuf,
      (long)apList[i].channel
    );

    lv_label_set_text(lbl, row);
        lv_obj_set_style_text_color(lbl, COL_LIST_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  }

  if (apCount == 0) {
    lv_obj_t *t = lv_list_add_text(list_wifi, "No networks found");
    if (t) lv_obj_set_style_text_color(t, COL_LIST_TEXT, 0);
  }

  Serial.printf("[WiFi] ---- Scan end: %d found ----\n", apCount);
  g_radio_state = RADIO_IDLE;

  status_bar_update_now();
  lv_timer_handler();
  rgb_set(0, 40, 0); // dim green = done
  g_wifi_scanning = false;
  g_radio_state = RADIO_IDLE;
  status_bar_update_now();
}

// ─────────────────────────────────────────────────────────────────────────────
// BLE scan + populate list
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// BLE type hints (heuristics)
// ─────────────────────────────────────────────────────────────────────────────
static const char* ble_type_hint(const String &name_in) {
  String name = name_in;
  name.toLowerCase();

  auto has = [&](const char *s) -> bool { return name.indexOf(s) >= 0; };

  // Earbuds / headphones
  if (has("airpods") || has("beats") || has("galaxy buds") || has("buds") || has("jbl") || has("sony") || has("bose") || has("headphone") || has("earbud")) {
    return "Earbuds";
  }

  // Watches / wearables
  if (has("watch") || has("apple watch") || has("galaxy watch") || has("fitbit") || has("garmin") || has("mi band") || has("band")) {
    return "Watch";
  }

  // Phones / tablets
  if (has("iphone") || has("ipad") || has("samsung") || has("galaxy") || has("pixel") || has("oneplus") || has("motorola") || has("huawei") || has("xiaomi")) {
    return "Phone";
  }

  // Trackers / tags
  if (has("airtag") || has("tile") || has("chipolo") || has("smarttag") || has("tracker") || has("find my")) {
    return "Tracker";
  }

  // Cars / infotainment
  if (has("tesla") || has("bmw") || has("ford") || has("toyota") || has("honda") || has("car")) {
    return "Car";
  }

  // Computers / accessories
  if (has("macbook") || has("imac") || has("windows") || has("keyboard") || has("mouse") || has("logitech") || has("mx")) {
    return "PC/Accessory";
  }

  // Speakers
  if (has("speaker") || has("soundbar") || has("sonos")) {
    return "Speaker";
  }

  return "Unknown";
}

static void run_ble_scan() {
  g_ble_scanning = true;
  g_radio_state = RADIO_BLE_SCAN;

  status_bar_update_now();
  lv_timer_handler();

  Serial.printf("\n[BLE] ---- Scan start ----\n");
  Serial.printf("[BLE] secs=%u active=%d max=%u\n", (unsigned)g_ble_scan_secs, (int)g_ble_active_scan, (unsigned)g_ble_max_results);

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
  Serial.printf("[BLE] init ok\n");

  bleCount = 0;
  if (!gBleScan) gBleScan = BLEDevice::getScan();
  BLEScan *pScan = gBleScan;
  pScan->clearResults();
  pScan->setAdvertisedDeviceCallbacks(&scanCB, false);
  pScan->setActiveScan(g_ble_active_scan);
  pScan->setInterval(100);
  pScan->setWindow(99);
  uint8_t secs = g_ble_scan_secs; if (secs < 1) secs = 5;
  pScan->start(secs, false); // blocking
  pScan->stop();
  delay(30);
  pScan->clearResults();

  Serial.printf("[BLE] found=%d\n", bleCount);

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
    // Themed row style (remove default white borders)
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_set_style_outline_width(item, 0, 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(item, 8, 0);
    lv_obj_set_style_pad_all(item, 6, 0);
    // Make item selectable
    lv_obj_add_event_cb(item, cb_ble_item_select, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_set_style_outline_width(item, 0, 0);
    lv_obj_set_style_outline_pad(item, 0, 0);
    lv_obj_set_style_outline_color(item, COL_BORDER, 0);
    lv_obj_set_style_bg_color(item,
      i % 2 == 0 ? COL_LIST_BLE_EVEN : COL_LIST_BLE_ODD, 0);

    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (!lbl) continue;

    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    String nm = bleList[i].name;
    const char *typeC = ble_type_hint(nm);
    String typeHint = String(typeC);
    if (typeHint != "Unknown") {
      // Keep list clean: only append when short
      if (nm.length() <= 14) nm = nm + " (" + typeHint + ")";
    }
    if (nm.length() > 18) nm = nm.substring(0, 17) + "~";
    String addr = bleList[i].address.substring(0, 11) + "..";

    char row[80];
    char rssiBuf[16];
    format_rssi((int)bleList[i].rssi, rssiBuf, sizeof(rssiBuf));

    snprintf(row, sizeof(row), "%s\n%s  %s",
      nm.c_str(),
      addr.c_str(),
      rssiBuf
    );
    lv_label_set_text(lbl, row);
        lv_obj_set_style_text_color(lbl, COL_LIST_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  }

  if (bleCount == 0) {
    lv_obj_t *t = lv_list_add_text(list_ble, "No BLE devices found");
    if (t) lv_obj_set_style_text_color(t, COL_LIST_TEXT, 0);
  }

    g_radio_state = RADIO_IDLE;

  status_bar_update_now();
  lv_timer_handler();
  rgb_set(0, 0, 40); // dim blue = done
  Serial.printf("[BLE] ---- Scan end ----\n");
  g_ble_scanning = false;

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

  // Clear previous selection (restore even/odd row color)
  if (wifi_selected_item && wifi_selected_item != item && wifi_selected_idx >= 0) {
    lv_obj_set_style_outline_width(wifi_selected_item, 0, 0);
    lv_obj_set_style_outline_pad(wifi_selected_item, 0, 0);

    lv_color_t restore = (wifi_selected_idx % 2 == 0) ? COL_LIST_WIFI_EVEN : COL_LIST_WIFI_ODD;
    lv_obj_set_style_bg_color(wifi_selected_item, restore, 0);
  }

  // Apply selection highlight
  wifi_selected_item = item;
  wifi_selected_idx  = idx;

  lv_obj_set_style_bg_color(item, COL_SEL_BG, 0);
  lv_obj_set_style_outline_color(item, COL_BORDER, 0);
  lv_obj_set_style_outline_width(item, 2, 0);
  lv_obj_set_style_outline_pad(item, 2, 0);

  show_wifi_detail(idx);
}


static void cb_ble_item_select(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  lv_obj_t *item = (lv_obj_t *)lv_event_get_target(e);
  int idx = (int)(intptr_t)lv_event_get_user_data(e);

  // Clear previous selection (restore even/odd row color)
  if (ble_selected_item && ble_selected_item != item && ble_selected_idx >= 0) {
    lv_obj_set_style_outline_width(ble_selected_item, 0, 0);
    lv_obj_set_style_outline_pad(ble_selected_item, 0, 0);

    lv_color_t restore = (ble_selected_idx % 2 == 0) ? COL_LIST_BLE_EVEN : COL_LIST_BLE_ODD;
    lv_obj_set_style_bg_color(ble_selected_item, restore, 0);
  }

  ble_selected_item = item;
  ble_selected_idx  = idx;

  lv_obj_set_style_bg_color(item, COL_SEL_BG, 0);
  lv_obj_set_style_outline_color(item, COL_BORDER, 0);
  lv_obj_set_style_outline_width(item, 2, 0);
  lv_obj_set_style_outline_pad(item, 2, 0);

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
  char rssiBuf[16];
  format_rssi((int)apList[idx].rssi, rssiBuf, sizeof(rssiBuf));

  const char *sec = wifi_auth_name(apList[idx].encType);

  snprintf(buf, sizeof(buf),
    "SSID: %s\n"
    "MAC: %s\n"
    "Security: %s\n"
    "RSSI: %s\n"
    "Channel: %ld",
    ssid.c_str(),
    apList[idx].bssid.c_str(),
    sec,
    rssiBuf,
    (long)apList[idx].channel
  );

  lv_label_set_text(lbl_wifi_detail, buf);
  lv_screen_load(scr_wifi_detail);
}

static void show_ble_detail(int idx) {
  if (!scr_ble_detail || !lbl_ble_detail) return;
  if (idx < 0 || idx >= bleCount) return;

  String nm = bleList[idx].name.length() ? bleList[idx].name : "(unnamed)";


  const char *type = ble_type_hint(nm);
  char txBuf[16];
  if (bleList[idx].hasTxPower) {
    snprintf(txBuf, sizeof(txBuf), "%d dBm", (int)bleList[idx].txPower);
  } else {
    snprintf(txBuf, sizeof(txBuf), "N/A");
  }

  char buf[256];
  char rssiBuf[16];
  format_rssi((int)bleList[idx].rssi, rssiBuf, sizeof(rssiBuf));
  snprintf(buf, sizeof(buf),
    "Name: %s\n"
    "Type: %s\n"
    "MAC: %s\n"
    "RSSI: %s\n"
    "Svc UUIDs: %u\n"
    "Mfg Data: %u bytes\n"
    "TX Power: %s",
    nm.c_str(),
    type,
    bleList[idx].address.c_str(),
    rssiBuf,
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

  // Update UI first, then perform scan from loop() (avoids LVGL re-entrancy)
  lv_screen_load(scr_wifi);
  lv_label_set_text(lbl_wifi_status, "Scanning...");
  lv_obj_clean(list_wifi);

  g_radio_state = RADIO_WIFI_SCAN;
  status_bar_update_now();
  g_scan_req = REQ_WIFI;
}

static void cb_go_ble(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  // Update UI first, then perform scan from loop() (avoids LVGL re-entrancy)
  lv_screen_load(scr_ble);
  lv_label_set_text(lbl_ble_status, "Scanning...");
  lv_obj_clean(list_ble);

  g_radio_state = RADIO_BLE_SCAN;
  status_bar_update_now();
  g_scan_req = REQ_BLE;
}

static void cb_back_home(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  g_scan_req = REQ_NONE;
  g_wifi_scanning = false;
  g_ble_scanning  = false;

  g_radio_state = RADIO_IDLE;
  status_bar_update_now();

  rgb_set(0, 0, 0);
  lv_screen_load(scr_home);
}

static void cb_rescan_wifi(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  // Update UI first, then perform scan from loop() (avoids LVGL re-entrancy)
  lv_screen_load(scr_wifi);
  lv_label_set_text(lbl_wifi_status, "Scanning...");
  lv_obj_clean(list_wifi);

  g_radio_state = RADIO_WIFI_SCAN;
  status_bar_update_now();
  g_scan_req = REQ_WIFI;
}

static void cb_rescan_ble(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  // Update UI first, then perform scan from loop() (avoids LVGL re-entrancy)
  lv_screen_load(scr_ble);
  lv_label_set_text(lbl_ble_status, "Scanning...");
  lv_obj_clean(list_ble);

  g_radio_state = RADIO_BLE_SCAN;
  status_bar_update_now();
  g_scan_req = REQ_BLE;
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

  // Theme dropdown
  if (lv_event_get_target(e) == dd_theme) {
    g_theme_idx = (uint8_t)lv_dropdown_get_selected(dd_theme);
    theme_apply_to_ui();
    settings_save_prefs();
    return;
  }
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

  if (sw_rssi_abs) g_rssi_absolute = lv_obj_has_state(sw_rssi_abs, LV_STATE_CHECKED);
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
  }  settings_save_prefs();
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
  lv_obj_set_style_border_color(btn, COL_BORDER, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, label_text);
  lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  lv_obj_center(lbl);
  return btn;
}

// ─────────────────────────────────────────────────────────────────────────────
// Build UI
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// Status bar helpers
// ─────────────────────────────────────────────────────────────────────────────
static lv_obj_t* create_status_bar(lv_obj_t *parent) {
  lv_obj_t *bar = lv_obj_create(parent);
  lv_obj_set_size(bar, SCREEN_W, 18);
  lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);

  // Colored background (configurable)
  lv_obj_set_style_bg_color(bar, COL_STATUS_BG, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bar, 0, 0);

  lv_obj_set_style_pad_left(bar, 6, 0);
  lv_obj_set_style_pad_right(bar, 6, 0);
  lv_obj_set_style_pad_top(bar, 1, 0);
  lv_obj_set_style_pad_bottom(bar, 0, 0);

  lv_obj_t *lbl = lv_label_create(bar);
  lv_label_set_text(lbl, "IDLE");
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
  return lbl;
}

static void status_bar_set_all(const char *txt) {
  if (lbl_status_home) lv_label_set_text(lbl_status_home, txt);
  if (lbl_status_wifi) lv_label_set_text(lbl_status_wifi, txt);
  if (lbl_status_ble)  lv_label_set_text(lbl_status_ble,  txt);
  if (lbl_status_wifi_detail) lv_label_set_text(lbl_status_wifi_detail, txt);
  if (lbl_status_ble_detail)  lv_label_set_text(lbl_status_ble_detail,  txt);
  if (lbl_status_settings)    lv_label_set_text(lbl_status_settings,    txt);
}

static void status_bar_update_now() {
  const char *rs = "IDLE";
  if (g_radio_state == RADIO_WIFI_SCAN) rs = "WIFI";
  else if (g_radio_state == RADIO_BLE_SCAN) rs = "BLE";
  status_bar_set_all(rs);
}


static void status_timer_cb(lv_timer_t *t) {
  (void)t;

}

// ─────────────────────────────────────────────────────────────────────────────
// Settings theming helpers
// ─────────────────────────────────────────────────────────────────────────────
static void settings_style_dropdown(lv_obj_t *dd) {
  if (!dd) return;
  lv_obj_set_style_bg_color(dd, COL_PANEL_BG, 0);
  lv_obj_set_style_bg_opa(dd, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(dd, COL_BORDER, 0);
  lv_obj_set_style_border_width(dd, 1, 0);
  lv_obj_set_style_radius(dd, 6, 0);
  lv_obj_set_style_text_color(dd, COL_PANEL_TEXT, 0);
}

static void settings_style_slider(lv_obj_t *sl) {
  if (!sl) return;
  lv_obj_set_style_bg_color(sl, lv_color_hex(0x222222), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(sl, COL_BORDER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(sl, 6, LV_PART_MAIN);
  lv_obj_set_style_radius(sl, 6, LV_PART_INDICATOR);
  lv_obj_set_style_border_width(sl, 0, LV_PART_MAIN);
}

static void settings_style_switch(lv_obj_t *sw) {
  if (!sw) return;
  // Off
  lv_obj_set_style_bg_color(sw, lv_color_hex(0x222222), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
  // On uses indicator
  lv_obj_set_style_bg_color(sw, COL_BORDER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR);
}

// ─────────────────────────────────────────────────────────────────────────────
// Persistence (Preferences / NVS)
// ─────────────────────────────────────────────────────────────────────────────
static void settings_apply_defaults() {
  g_ble_scan_secs     = (uint8_t)CFG_BLE_SCAN_SECS;
  g_wifi_scan_secs    = (uint8_t)CFG_WIFI_SCAN_SECS;
  g_wifi_max_results  = (uint8_t)CFG_WIFI_MAX_RESULTS;
  g_ble_max_results   = (uint8_t)CFG_BLE_MAX_RESULTS;

  g_wifi_show_hidden  = (bool)CFG_WIFI_SHOW_HIDDEN;
  g_ble_active_scan   = (bool)CFG_BLE_ACTIVE_SCAN;

  g_rssi_absolute   = (bool)CFG_RSSI_ABSOLUTE;
  g_rssi_absolute   = (bool)CFG_RSSI_ABSOLUTE;
  g_rssi_absolute   = (bool)CFG_RSSI_ABSOLUTE;
  g_brightness_pct    = (uint8_t)CFG_BRIGHTNESS_PCT;
  g_sleep_timeout_s   = (uint16_t)CFG_SLEEP_TIMEOUT_S;
  g_sleep_dim_pct     = (uint8_t)CFG_SLEEP_DIM_PCT;

  g_rgb_enabled       = (bool)CFG_RGB_ENABLED;
  g_theme_idx         = (uint8_t)CFG_THEME_DEFAULT;
}

static void settings_load_prefs() {
  prefs.begin(PREF_NS, true);
  g_ble_scan_secs     = prefs.getUChar("ble_secs",  (uint8_t)CFG_BLE_SCAN_SECS);
  g_wifi_scan_secs    = prefs.getUChar("wifi_secs", (uint8_t)CFG_WIFI_SCAN_SECS);
  g_wifi_max_results  = prefs.getUChar("wifi_max",  (uint8_t)CFG_WIFI_MAX_RESULTS);
  g_ble_max_results   = prefs.getUChar("ble_max",   (uint8_t)CFG_BLE_MAX_RESULTS);

  g_wifi_show_hidden  = prefs.getBool ("wifi_hid",  (bool)CFG_WIFI_SHOW_HIDDEN);
  g_ble_active_scan   = prefs.getBool ("ble_act",   (bool)CFG_BLE_ACTIVE_SCAN);

  g_brightness_pct    = prefs.getUChar("bright",    (uint8_t)CFG_BRIGHTNESS_PCT);
  g_sleep_timeout_s   = prefs.getUShort("sleep_s",  (uint16_t)CFG_SLEEP_TIMEOUT_S);
  g_sleep_dim_pct     = prefs.getUChar("sleep_dim", (uint8_t)CFG_SLEEP_DIM_PCT);

  g_rgb_enabled       = prefs.getBool ("rgb_en",    (bool)CFG_RGB_ENABLED);
  g_theme_idx         = prefs.getUChar("theme",     (uint8_t)CFG_THEME_DEFAULT);
  prefs.end();

  if (g_ble_scan_secs < 1) g_ble_scan_secs = (uint8_t)CFG_BLE_SCAN_SECS;
  if (g_wifi_scan_secs < 1) g_wifi_scan_secs = (uint8_t)CFG_WIFI_SCAN_SECS;
  if (g_wifi_max_results < 1) g_wifi_max_results = (uint8_t)CFG_WIFI_MAX_RESULTS;
  if (g_ble_max_results < 1) g_ble_max_results = (uint8_t)CFG_BLE_MAX_RESULTS;
  if (g_brightness_pct > 100) g_brightness_pct = 100;
  if (g_sleep_dim_pct < 1) g_sleep_dim_pct = 1;
  if (g_sleep_dim_pct > 100) g_sleep_dim_pct = 100;
  if (g_theme_idx > 3) g_theme_idx = (uint8_t)CFG_THEME_DEFAULT;
}

static void settings_save_prefs() {
  prefs.begin(PREF_NS, false);
  prefs.putUChar("ble_secs",  g_ble_scan_secs);
  prefs.putUChar("wifi_secs", g_wifi_scan_secs);
  prefs.putUChar("wifi_max",  g_wifi_max_results);
  prefs.putUChar("ble_max",   g_ble_max_results);

  prefs.putBool ("wifi_hid",  g_wifi_show_hidden);
  prefs.putBool ("ble_act",   g_ble_active_scan);

  prefs.putBool("rssi_abs", g_rssi_absolute);
  prefs.putUChar("bright",    g_brightness_pct);
  prefs.putUShort("sleep_s",  g_sleep_timeout_s);
  prefs.putUChar("sleep_dim", g_sleep_dim_pct);

  prefs.putBool ("rgb_en",    g_rgb_enabled);
  prefs.putUChar("theme",     g_theme_idx);
  prefs.end();
}

static void settings_clear_prefs() {
  prefs.begin(PREF_NS, false);
  prefs.clear();
  prefs.end();
}

// ─────────────────────────────────────────────────────────────────────────────
// Reset All flow
// ─────────────────────────────────────────────────────────────────────────────
static void reset_sync_controls_from_runtime();

static void cb_reset_cancel(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_screen_load(scr_settings);
}

static void cb_reset_ok(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  settings_clear_prefs();
  settings_apply_defaults();

  theme_apply_to_ui();
  reset_sync_controls_from_runtime();
  set_backlight_pct(g_brightness_pct);

  delay(150);
  ESP.restart();
}

static void cb_reset_all(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_screen_load(scr_reset);
}


static void reset_sync_controls_from_runtime() {
  if (dd_theme) lv_dropdown_set_selected(dd_theme, g_theme_idx);

  if (sw_hidden) { if (g_wifi_show_hidden) lv_obj_add_state(sw_hidden, LV_STATE_CHECKED); else lv_obj_clear_state(sw_hidden, LV_STATE_CHECKED); }
  if (sw_active) { if (g_ble_active_scan)  lv_obj_add_state(sw_active, LV_STATE_CHECKED); else lv_obj_clear_state(sw_active, LV_STATE_CHECKED); }
  if (sw_rgb)    { if (g_rgb_enabled)      lv_obj_add_state(sw_rgb, LV_STATE_CHECKED);    else lv_obj_clear_state(sw_rgb, LV_STATE_CHECKED); }
  if (sw_rssi_abs) { if (g_rssi_absolute) lv_obj_add_state(sw_rssi_abs, LV_STATE_CHECKED); else lv_obj_clear_state(sw_rssi_abs, LV_STATE_CHECKED); }

  if (slider_bright) lv_slider_set_value(slider_bright, g_brightness_pct, LV_ANIM_OFF);

  if (dd_sleep) {
    int sel = 0;
    if (g_sleep_timeout_s == 30) sel = 1;
    else if (g_sleep_timeout_s == 60) sel = 2;
    else if (g_sleep_timeout_s == 120) sel = 3;
    else if (g_sleep_timeout_s == 300) sel = 4;
    else if (g_sleep_timeout_s == 600) sel = 5;
    lv_dropdown_set_selected(dd_sleep, sel);
  }

  settings_style_dropdown(dd_ble_secs);
  settings_style_dropdown(dd_wifi_secs);
  settings_style_dropdown(dd_wifi_max);
  settings_style_dropdown(dd_ble_max);
  settings_style_dropdown(dd_theme);
  settings_style_dropdown(dd_sleep);
  settings_style_slider(slider_bright);
  settings_style_switch(sw_hidden);
  settings_style_switch(sw_active);
  settings_style_switch(sw_rgb);
}

static void build_ui() {
  lv_color_t bg_dark = COL_SCREEN_BG;

  // ── HOME SCREEN ──────────────────────────────────────────────────────────
  scr_home = lv_obj_create(NULL);
  lbl_status_home = create_status_bar(scr_home);

  lv_obj_set_style_bg_color(scr_home, bg_dark, 0);
  lv_obj_set_style_bg_opa(scr_home, LV_OPA_COVER, 0);

  // Title
  lv_obj_t *title = lv_label_create(scr_home);
  lbl_home_title = title;
  lv_label_set_text(title, LV_SYMBOL_WIFI "  " CFG_HOME_TITLE);
  lv_obj_set_style_text_color(title, COL_TITLE_TEXT, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 26);

  // Divider
  lv_obj_t *line = lv_obj_create(scr_home);
  lv_obj_set_size(line, SCREEN_W - 20, 1);
  lv_obj_set_style_bg_color(line, lv_color_hex(0x07FFFF), 0);
  lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 46);

  // Subtitle
  lv_obj_t *sub = lv_label_create(scr_home);
  lbl_home_sub = sub;
  lv_label_set_text(sub, "Select a scan mode");
  lv_obj_set_style_text_color(sub, COL_SUBTITLE_TEXT, 0);
  lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
  lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 64);

  // WiFi button
  btn_wifi = make_button(scr_home,
    LV_SYMBOL_WIFI "  WiFi Scan",
    COL_WIFI_BTN, cb_go_wifi);
  lv_obj_set_size(btn_wifi, 170, 50);
  lv_obj_align(btn_wifi, LV_ALIGN_CENTER, 0, -40);

  // BLE button
  btn_ble = make_button(scr_home,
    LV_SYMBOL_BLUETOOTH "  BLE Scan",
    COL_BLE_BTN, cb_go_ble);
  lv_obj_set_size(btn_ble, 170, 50);
  lv_obj_align(btn_ble, LV_ALIGN_CENTER, 0, 30);

  // Settings button (small) under BLE scan, right side
  btn_settings = make_button(scr_home, LV_SYMBOL_SETTINGS, lv_color_hex(0x222244), cb_open_settings);
  lv_obj_set_size(btn_settings, 60, 36);
  lv_obj_align(btn_settings, LV_ALIGN_CENTER, 56, 100);

  // Footer
  lv_obj_t *footer = lv_label_create(scr_home);
  lv_label_set_text(footer, "Created By ATOMNFT");
  lv_obj_set_style_text_color(footer, lv_color_hex(0x444466), 0);
  lv_obj_set_style_text_font(footer, &lv_font_montserrat_14, 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -8);

  // ── WIFI SCREEN ──────────────────────────────────────────────────────────
  scr_wifi = lv_obj_create(NULL);
  lbl_status_wifi = create_status_bar(scr_wifi);

  lv_obj_set_style_bg_color(scr_wifi, bg_dark, 0);
  lv_obj_set_style_bg_opa(scr_wifi, LV_OPA_COVER, 0);

  // Header row
  wifi_hdr = lv_obj_create(scr_wifi);
lv_obj_set_size(wifi_hdr, SCREEN_W, 46);
  lv_obj_set_style_bg_color(wifi_hdr, COL_HDR_WIFI_BG, 0);
  lv_obj_set_style_border_width(wifi_hdr, 0, 0);
  lv_obj_align(wifi_hdr, LV_ALIGN_TOP_LEFT, 0, 18);
  lv_obj_set_style_pad_all(wifi_hdr, 4, 0);

  lv_obj_t *wifi_title = lv_label_create(wifi_hdr);
  lbl_wifi_title = wifi_title;
  lv_label_set_text(wifi_title, LV_SYMBOL_WIFI "  WiFi Networks");
  lv_obj_set_style_text_color(wifi_title, COL_HDR_WIFI_TEXT, 0);
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
  lv_obj_set_size(list_wifi, SCREEN_W, SCREEN_H - 46 - 44 - 18);
  lv_obj_align(list_wifi, LV_ALIGN_TOP_LEFT, 0, 64);
  lv_obj_set_style_bg_color(list_wifi, bg_dark, 0);
  lv_obj_set_style_border_width(list_wifi, 0, 0);
  lv_obj_set_style_pad_row(list_wifi, 2, 0);

  // Bottom bar: Back + Rescan
  wifi_bar = lv_obj_create(scr_wifi);
  lv_obj_set_size(wifi_bar, SCREEN_W, 44);
  lv_obj_align(wifi_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(wifi_bar, COL_HDR_WIFI_BG, 0);
  lv_obj_set_style_border_width(wifi_bar, 0, 0);
  lv_obj_set_style_pad_all(wifi_bar, 4, 0);
  lv_obj_set_flex_flow(wifi_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(wifi_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  btn_wifi_home = make_button(wifi_bar, LV_SYMBOL_HOME "  Home",
    COL_BACK_BTN_BG, cb_back_home);
  lv_obj_set_size(btn_wifi_home, 100, 34);

  btn_wifi_rescan = make_button(wifi_bar, LV_SYMBOL_REFRESH "  Rescan",
    COL_WIFI_BTN, cb_rescan_wifi);
  lv_obj_set_size(btn_wifi_rescan, 100, 34);

  // ── BLE SCREEN ───────────────────────────────────────────────────────────
  scr_ble = lv_obj_create(NULL);
  lbl_status_ble = create_status_bar(scr_ble);

  lv_obj_set_style_bg_color(scr_ble, bg_dark, 0);
  lv_obj_set_style_bg_opa(scr_ble, LV_OPA_COVER, 0);

  ble_hdr = lv_obj_create(scr_ble);
lv_obj_set_size(ble_hdr, SCREEN_W, 46);
  lv_obj_set_style_bg_color(ble_hdr, COL_HDR_BLE_BG, 0);
  lv_obj_set_style_border_width(ble_hdr, 0, 0);
  lv_obj_align(ble_hdr, LV_ALIGN_TOP_LEFT, 0, 18);
  lv_obj_set_style_pad_all(ble_hdr, 4, 0);

  lv_obj_t *ble_title = lv_label_create(ble_hdr);
  lbl_ble_title = ble_title;
  lv_label_set_text(ble_title, LV_SYMBOL_BLUETOOTH "  BLE Devices");
  lv_obj_set_style_text_color(ble_title, COL_HDR_BLE_TEXT, 0);
  lv_obj_set_style_text_font(ble_title, &lv_font_montserrat_14, 0);
  lv_obj_align(ble_title, LV_ALIGN_LEFT_MID, 4, 0);

  lbl_ble_status = lv_label_create(ble_hdr);
  lv_label_set_text(lbl_ble_status, "");
  lv_obj_set_style_text_color(lbl_ble_status, lv_color_hex(0xAABBCC), 0);
  lv_obj_set_style_text_font(lbl_ble_status, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl_ble_status, LV_ALIGN_RIGHT_MID, -4, 0);

  list_ble = lv_list_create(scr_ble);
  lv_obj_set_size(list_ble, SCREEN_W, SCREEN_H - 46 - 44 - 18);
  lv_obj_align(list_ble, LV_ALIGN_TOP_LEFT, 0, 64);
  lv_obj_set_style_bg_color(list_ble, bg_dark, 0);
  lv_obj_set_style_border_width(list_ble, 0, 0);
  lv_obj_set_style_pad_row(list_ble, 2, 0);

  ble_bar = lv_obj_create(scr_ble);
  lv_obj_set_size(ble_bar, SCREEN_W, 44);
  lv_obj_align(ble_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(ble_bar, COL_HDR_BLE_BG, 0);
  lv_obj_set_style_border_width(ble_bar, 0, 0);
  lv_obj_set_style_pad_all(ble_bar, 4, 0);
  lv_obj_set_flex_flow(ble_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ble_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  btn_ble_home = make_button(ble_bar, LV_SYMBOL_HOME "  Home",
    COL_BACK_BTN_BG, cb_back_home);
  lv_obj_set_size(btn_ble_home, 100, 34);

  btn_ble_rescan = make_button(ble_bar, LV_SYMBOL_REFRESH "  Rescan",
    COL_BLE_BTN, cb_rescan_ble);
  lv_obj_set_size(btn_ble_rescan, 100, 34);

  // ── WIFI DETAIL SCREEN ─────────────────────────────────────────────────────
  scr_wifi_detail = lv_obj_create(NULL);
  lbl_status_wifi_detail = create_status_bar(scr_wifi_detail);

  lv_obj_set_style_bg_color(scr_wifi_detail, bg_dark, 0);
  lv_obj_set_style_bg_opa(scr_wifi_detail, LV_OPA_COVER, 0);

  wd_hdr = lv_obj_create(scr_wifi_detail);
  lv_obj_set_size(wd_hdr, SCREEN_W, 46);
  lv_obj_set_style_bg_color(wd_hdr, COL_HDR_WIFI_BG, 0);
  lv_obj_set_style_border_width(wd_hdr, 0, 0);
  lv_obj_align(wd_hdr, LV_ALIGN_TOP_LEFT, 0, 18);
  lv_obj_set_style_pad_all(wd_hdr, 4, 0);

  wd_title = lv_label_create(wd_hdr);
  lv_label_set_text(wd_title, "WiFi Details");
  lv_obj_set_style_text_color(wd_title, COL_HDR_WIFI_TEXT, 0);
  lv_obj_set_style_text_font(wd_title, &lv_font_montserrat_14, 0);
  lv_obj_align(wd_title, LV_ALIGN_LEFT_MID, 4, 0);

  wd_back = make_button(wd_hdr, LV_SYMBOL_LEFT " Back", COL_BACK_BTN_BG, cb_noop);
  lv_obj_add_event_cb(wd_back, cb_detail_back, LV_EVENT_CLICKED, (void*)"wifi");
  lv_obj_set_size(wd_back, 80, 30);
  lv_obj_align(wd_back, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_user_data(wd_back, (void*)"wifi");

  wd_box = lv_obj_create(scr_wifi_detail);
  lv_obj_set_size(wd_box, SCREEN_W - 16, SCREEN_H - 46 - 16 - 18);
  lv_obj_align(wd_box, LV_ALIGN_TOP_MID, 0, 72);
  lv_obj_set_style_bg_color(wd_box, COL_PANEL_BG, 0);
  lv_obj_set_style_border_color(wd_box, COL_BORDER, 0);
  lv_obj_set_style_border_width(wd_box, 1, 0);
  lv_obj_set_style_radius(wd_box, 10, 0);
  lv_obj_set_style_pad_all(wd_box, 10, 0);

  lbl_wifi_detail = lv_label_create(wd_box);
  lv_label_set_text(lbl_wifi_detail, "");
  lv_obj_set_style_text_color(lbl_wifi_detail, COL_PANEL_TEXT, 0);
  lv_obj_set_style_text_font(lbl_wifi_detail, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(lbl_wifi_detail, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_wifi_detail, SCREEN_W - 36);
  lv_obj_align(lbl_wifi_detail, LV_ALIGN_TOP_LEFT, 0, 0);

  // ── BLE DETAIL SCREEN ──────────────────────────────────────────────────────
  scr_ble_detail = lv_obj_create(NULL);
  lbl_status_ble_detail = create_status_bar(scr_ble_detail);

  lv_obj_set_style_bg_color(scr_ble_detail, bg_dark, 0);
  lv_obj_set_style_bg_opa(scr_ble_detail, LV_OPA_COVER, 0);

  bd_hdr = lv_obj_create(scr_ble_detail);
  lv_obj_set_size(bd_hdr, SCREEN_W, 46);
  lv_obj_set_style_bg_color(bd_hdr, COL_HDR_BLE_BG, 0);
  lv_obj_set_style_border_width(bd_hdr, 0, 0);
  lv_obj_align(bd_hdr, LV_ALIGN_TOP_LEFT, 0, 18);
  lv_obj_set_style_pad_all(bd_hdr, 4, 0);

  bd_title = lv_label_create(bd_hdr);
  lv_label_set_text(bd_title, "BLE Details");
  lv_obj_set_style_text_color(bd_title, COL_HDR_BLE_TEXT, 0);
  lv_obj_set_style_text_font(bd_title, &lv_font_montserrat_14, 0);
  lv_obj_align(bd_title, LV_ALIGN_LEFT_MID, 4, 0);

  bd_back = make_button(bd_hdr, LV_SYMBOL_LEFT " Back", COL_BACK_BTN_BG, cb_noop);
  lv_obj_add_event_cb(bd_back, cb_detail_back, LV_EVENT_CLICKED, (void*)"ble");
  lv_obj_set_size(bd_back, 80, 30);
  lv_obj_align(bd_back, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_user_data(bd_back, (void*)"ble");

  bd_box = lv_obj_create(scr_ble_detail);
  lv_obj_set_size(bd_box, SCREEN_W - 16, SCREEN_H - 46 - 16 - 18);
  lv_obj_align(bd_box, LV_ALIGN_TOP_MID, 0, 72);
  lv_obj_set_style_bg_color(bd_box, COL_PANEL_BG, 0);
  lv_obj_set_style_border_color(bd_box, lv_color_hex(0x1E90FF), 0);
  lv_obj_set_style_border_width(bd_box, 1, 0);
  lv_obj_set_style_radius(bd_box, 10, 0);
  lv_obj_set_style_pad_all(bd_box, 10, 0);

  lbl_ble_detail = lv_label_create(bd_box);
  lv_label_set_text(lbl_ble_detail, "");
  lv_obj_set_style_text_color(lbl_ble_detail, COL_PANEL_TEXT, 0);
  lv_obj_set_style_text_font(lbl_ble_detail, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(lbl_ble_detail, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_ble_detail, SCREEN_W - 36);
  lv_obj_align(lbl_ble_detail, LV_ALIGN_TOP_LEFT, 0, 0);

  // ── SETTINGS SCREEN ────────────────────────────────────────────────────────
  scr_settings = lv_obj_create(NULL);
  lbl_status_settings = create_status_bar(scr_settings);

  lv_obj_set_style_bg_color(scr_settings, bg_dark, 0);
  lv_obj_set_style_bg_opa(scr_settings, LV_OPA_COVER, 0);

  // Header
  st_hdr = lv_obj_create(scr_settings);
lv_obj_set_size(st_hdr, SCREEN_W, 46);
  lv_obj_set_style_bg_color(st_hdr, COL_HDR_SETTINGS_BG, 0);
  lv_obj_set_style_border_width(st_hdr, 0, 0);
  lv_obj_align(st_hdr, LV_ALIGN_TOP_LEFT, 0, 18);
  lv_obj_set_style_pad_all(st_hdr, 4, 0);

  lv_obj_t *st_title = lv_label_create(st_hdr);
  lbl_settings_title = st_title;
lv_label_set_text(st_title, "Settings");
  lv_obj_set_style_text_color(st_title, COL_HDR_SETTINGS_TEXT, 0);
  lv_obj_set_style_text_font(st_title, &lv_font_montserrat_14, 0);
  lv_obj_align(st_title, LV_ALIGN_LEFT_MID, 4, 0);

  st_back = make_button(st_hdr, LV_SYMBOL_LEFT " Back", COL_BACK_BTN_BG, cb_settings_back);
  lv_obj_set_size(st_back, 80, 30);
  lv_obj_align(st_back, LV_ALIGN_RIGHT_MID, -4, 0);

  // Body container
  st_body = lv_obj_create(scr_settings);
  lv_obj_t *st = st_body;
  lv_obj_set_size(st, SCREEN_W - 16, SCREEN_H - 46 - 16 - 18);
  lv_obj_align(st, LV_ALIGN_TOP_MID, 0, 72);
  lv_obj_set_style_bg_color(st, COL_PANEL_BG, 0);
  lv_obj_set_style_border_color(st, COL_BORDER, 0);
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
    lv_obj_set_style_text_color(_row##_lab, COL_PANEL_TEXT, 0); \
    lv_obj_set_style_text_font(_row##_lab, &lv_font_montserrat_14, 0);

  // BLE scan seconds
  {
    MAKE_ROW("BLE Scan (sec)", row1);
    dd_ble_secs = lv_dropdown_create(row1);
    settings_style_dropdown(dd_ble_secs);
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
    settings_style_dropdown(dd_wifi_secs);
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
    settings_style_dropdown(dd_wifi_max);
    lv_dropdown_set_options(dd_wifi_max, "10\n20");
    lv_dropdown_set_selected(dd_wifi_max, (g_wifi_max_results <= 10) ? 0 : 1);
    lv_obj_set_width(dd_wifi_max, 70);
    lv_obj_add_event_cb(dd_wifi_max, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // BLE max results
  {
    MAKE_ROW("BLE Max", row3);
    dd_ble_max = lv_dropdown_create(row3);
    settings_style_dropdown(dd_ble_max);
    lv_dropdown_set_options(dd_ble_max, "10\n20");
    lv_dropdown_set_selected(dd_ble_max, (g_ble_max_results <= 10) ? 0 : 1);
    lv_obj_set_width(dd_ble_max, 70);
    lv_obj_add_event_cb(dd_ble_max, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // WiFi show hidden
  {
    MAKE_ROW("WiFi Hidden", row4);
    sw_hidden = lv_switch_create(row4);
    settings_style_switch(sw_hidden);
    if (g_wifi_show_hidden) lv_obj_add_state(sw_hidden, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_hidden, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // BLE active scan
  {
    MAKE_ROW("BLE Active", row5);
    sw_active = lv_switch_create(row5);
    settings_style_switch(sw_active);
    if (g_ble_active_scan) lv_obj_add_state(sw_active, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_active, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }


  // RSSI format (absolute vs dBm)
  {
    MAKE_ROW("RSSI Abs", row5b);
    sw_rssi_abs = lv_switch_create(row5b);
    settings_style_switch(sw_rssi_abs);
    if (g_rssi_absolute) lv_obj_add_state(sw_rssi_abs, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_rssi_abs, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // Brightness
  {
    MAKE_ROW("Brightness", row6);
    slider_bright = lv_slider_create(row6);
    settings_style_slider(slider_bright);
    lv_slider_set_range(slider_bright, 5, 100);
    lv_slider_set_value(slider_bright, g_brightness_pct, LV_ANIM_OFF);
    lv_obj_set_width(slider_bright, 120);
    lv_obj_add_event_cb(slider_bright, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }


  // Theme (dropdown)
  {
    MAKE_ROW("Theme", row_theme);
    dd_theme = lv_dropdown_create(row_theme);
    settings_style_dropdown(dd_theme);
    lv_dropdown_set_options(dd_theme, "Jungle\nCyber\nMidnight\nType-R");
    lv_dropdown_set_selected(dd_theme, g_theme_idx);
    lv_obj_set_width(dd_theme, 120);
    lv_obj_add_event_cb(dd_theme, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  // Sleep (presets)
  {
    MAKE_ROW("Sleep", row7);
    dd_sleep = lv_dropdown_create(row7);
    settings_style_dropdown(dd_sleep);
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

  // Reset All (confirmation)
  {
    MAKE_ROW("Reset", row_reset);
    btn_reset_all = make_button(row_reset, "Reset All", COL_BLE_BTN, cb_reset_all);
    lv_obj_set_size(btn_reset_all, 120, 34);
    lv_obj_t *lbl = lv_obj_get_child(btn_reset_all, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, COL_BTN_TEXT, 0);
  }

  // RGB LED enable
  {
    MAKE_ROW("RGB LED", row8);
    sw_rgb = lv_switch_create(row8);
    settings_style_switch(sw_rgb);
    if (g_rgb_enabled) lv_obj_add_state(sw_rgb, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_rgb, cb_settings_apply, LV_EVENT_VALUE_CHANGED, nullptr);
  }

  #undef MAKE_ROW


  // ── RESET CONFIRM SCREEN ───────────────────────────────────────────────────
  scr_reset = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_reset, COL_SCREEN_BG, 0);
  lv_obj_set_style_bg_opa(scr_reset, LV_OPA_COVER, 0);
  create_status_bar(scr_reset);

  reset_hdr = lv_obj_create(scr_reset);
  lv_obj_set_size(reset_hdr, SCREEN_W, 46);
  lv_obj_set_style_bg_color(reset_hdr, COL_HDR_SETTINGS_BG, 0);
  lv_obj_set_style_border_width(reset_hdr, 0, 0);
  lv_obj_align(reset_hdr, LV_ALIGN_TOP_LEFT, 0, 18);
  lv_obj_set_style_pad_all(reset_hdr, 4, 0);

  lv_obj_t *rt = lv_label_create(reset_hdr);
  lv_label_set_text(rt, "Reset All");
  lv_obj_set_style_text_color(rt, COL_HDR_SETTINGS_TEXT, 0);
  lv_obj_set_style_text_font(rt, &lv_font_montserrat_14, 0);
  lv_obj_align(rt, LV_ALIGN_LEFT_MID, 4, 0);

  btn_reset_cancel = make_button(reset_hdr, LV_SYMBOL_LEFT " Back", COL_BACK_BTN_BG, cb_reset_cancel);
  lv_obj_set_size(btn_reset_cancel, 80, 32);
  lv_obj_align(btn_reset_cancel, LV_ALIGN_RIGHT_MID, -4, 0);

  reset_box = lv_obj_create(scr_reset);
  lv_obj_set_size(reset_box, SCREEN_W - 16, SCREEN_H - 46 - 16 - 18);
  lv_obj_set_style_bg_color(reset_box, COL_PANEL_BG, 0);
  lv_obj_set_style_border_color(reset_box, COL_BORDER, 0);
  lv_obj_set_style_border_width(reset_box, 1, 0);
  lv_obj_set_style_radius(reset_box, 10, 0);
  lv_obj_align(reset_box, LV_ALIGN_TOP_MID, 0, 72);
  lv_obj_set_style_pad_all(reset_box, 10, 0);

  lbl_reset_body = lv_label_create(reset_box);
  lv_label_set_text(lbl_reset_body,
    "Reset All will clear saved settings\n"
    "and restore compile-time defaults.\n\n"
    "The device will reboot after reset.");
  lv_label_set_long_mode(lbl_reset_body, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_reset_body, 200);   // adjust to fit inside reset_box
  lv_obj_set_style_text_color(lbl_reset_body, COL_PANEL_TEXT, 0);
  lv_obj_set_style_text_font(lbl_reset_body, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl_reset_body, LV_ALIGN_TOP_LEFT, 0, 0);

  btn_reset_ok = make_button(reset_box, "OK - Reset", COL_WIFI_BTN, cb_reset_ok);
  lv_obj_set_size(btn_reset_ok, 96, 34);
  lv_obj_align(btn_reset_ok, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  lv_obj_t *btn_cancel2 = make_button(reset_box, "Cancel", COL_BACK_BTN_BG, cb_reset_cancel);
  lv_obj_set_size(btn_cancel2, 96, 34);
  lv_obj_align(btn_cancel2, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

}

// ─────────────────────────────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(50);


  settings_load_prefs();
  rgb_init();
  rgb_boot_test();
  rgb_set(0, 0, 0);

  Serial.println("=== ESP32 Scanner — WiFi + BLE ===");
  g_boot_ms = millis();

  lv_init();

  // Theme colors
  theme_apply_palette();


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
  theme_apply_to_ui();
  lv_screen_load(scr_home);

  // Status bar timer (updates system info)
  lv_timer_create(status_timer_cb, 1000, nullptr);
  status_timer_cb(nullptr);

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

  // Run requested scans outside LVGL event callbacks
  if (g_scan_req == REQ_WIFI && !g_wifi_scanning && !g_ble_scanning) {
    g_scan_req = REQ_NONE;
    run_wifi_scan();
  } else if (g_scan_req == REQ_BLE && !g_wifi_scanning && !g_ble_scanning) {
    g_scan_req = REQ_NONE;
    run_ble_scan();
  }

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
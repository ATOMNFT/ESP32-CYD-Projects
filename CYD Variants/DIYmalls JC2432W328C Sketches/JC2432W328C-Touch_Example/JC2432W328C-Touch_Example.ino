/*
  JC2432W328C (ESP32) — LVGL v9.0.0 + TFT_eSPI + CST820 Capacitive Touch + Onboard RGB LED (PWM)
  ======================================================================================

  Created by: ATOMNFT
  GitHub: https://github.com/ATOMNFT

  What this sketch is:
  --------------------
  A known-good, beginner-friendly example for the ESP32 JC2432W328C / “CYD-style” 240x320
  touchscreen boards that use:
    - TFT_eSPI for the display driver (DMA push via pushImageDMA)
    - CST820 capacitive touch controller over I2C (0x15 on many boards)
    - LVGL v9.0.0 for UI rendering + input handling
    - 3-pin onboard RGB LED driven with ESP32 LEDC PWM (separate R/G/B pins)

  What you’ll see:
  ---------------
  - A centered LVGL button labeled "TEST"
  - Touching the button changes it to green and shows "TOUCHED"
  - Releasing returns it to blue and "TEST"
  - RGB LED turns GREEN on press, OFF on release
  - Serial prints show RAW touch coordinates and the mapped coordinates used by LVGL

  Why touch sometimes “doesn’t work” on these boards:
  --------------------------------------------------
  Many JC2432W328C boards have an INT pin that is stuck HIGH, inverted, or not routed.
  If you gate touch reads using INT (GATE_READS_BY_INT = 1), LVGL may never poll the
  touch controller. This sketch defaults to polling the CST820 directly:

      #define GATE_READS_BY_INT 0

  This is the most reliable setting across board variants.

  Quick setup notes:
  ------------------
  1) TFT_eSPI setup:
     - Make sure your TFT_eSPI User_Setup matches your panel + pins. I've included a User_setup.h file
     - This example uses rotation 0:
         tft.setRotation(0);

  2) Touch I2C pins:
     - Default here:
         SDA = GPIO33
         SCL = GPIO32
         RST = GPIO25
         INT = GPIO21

  3) Backlight / power enable:
     - Many CYD-style boards use GPIO27 as the backlight/power enable.

  4) RGB LED:
     - Pins used:
         R = GPIO4, G = GPIO16, B = GPIO17
     - If your LED behaves “inverted” (common-anode), set:
         #define RGB_INVERTED 1
       If your colors act normal (0=off, 255=on), set RGB_INVERTED to 0.

  5) Touch coordinate mapping:
     - If the touch is mirrored or rotated, adjust the mapping section in
       my_touchpad_read() by uncommenting ONE mapping option at a time.

  Debug toggles:
  --------------
  PRINT_TOUCH_RELEASE : Prints release state (spammy)
  DEBUG_RAW_I2C_DUMP  : Dumps raw CST820 I2C frames (spammy)
  GATE_READS_BY_INT   : Gate touch reads using INT (less reliable on many boards)
  INT_MODE_INPUT_PULLUP: Changes INT pin mode (some boards need INPUT vs INPUT_PULLUP)
  DO_TOUCH_RESET_PULSE: Sends a reset pulse at boot (helps some CST820 modules)

  License / Use:
  --------------
  Use freely in your projects. Credit appreciated. If you improve it, PRs welcome.

  ======================================================================================
*/

#include <lvgl.h>        // LVGL v9.0.0
#include <TFT_eSPI.h>
#include "CST820.h"
#include <Wire.h>

// =====================
// Onboard RGB LED (3-pin) - PWM
// =====================
#define LED_R_PIN 4
#define LED_G_PIN 16
#define LED_B_PIN 17

// Set to 1 if colors are inverted (common-anode LED)
#define RGB_INVERTED 1

// PWM settings
static const int LED_PWM_FREQ = 5000;
static const int LED_PWM_BITS = 8;     // 0-255
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

static void rgb_init()
{
  // ESP32 LEDC PWM
  ledcSetup(LEDC_CH_R, LED_PWM_FREQ, LED_PWM_BITS);
  ledcSetup(LEDC_CH_G, LED_PWM_FREQ, LED_PWM_BITS);
  ledcSetup(LEDC_CH_B, LED_PWM_FREQ, LED_PWM_BITS);

  ledcAttachPin(LED_R_PIN, LEDC_CH_R);
  ledcAttachPin(LED_G_PIN, LEDC_CH_G);
  ledcAttachPin(LED_B_PIN, LEDC_CH_B);

  // Start OFF
  ledcWrite(LEDC_CH_R, rgb_fix(0));
  ledcWrite(LEDC_CH_G, rgb_fix(0));
  ledcWrite(LEDC_CH_B, rgb_fix(0));
}

// r,g,b = 0..255
static void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
  ledcWrite(LEDC_CH_R, rgb_fix(r));
  ledcWrite(LEDC_CH_G, rgb_fix(g));
  ledcWrite(LEDC_CH_B, rgb_fix(b));
}

// =====================
// Config / Toggles
// =====================

// Print whenever LVGL polls and touch is RELEASED (can be spammy)
#define PRINT_TOUCH_RELEASE 0

// Extra raw I2C dump even when not touched (useful for debugging, spammy)
#define DEBUG_RAW_I2C_DUMP 0

// Only poll touch when INT indicates a touch (usually INT goes LOW on touch)
#define GATE_READS_BY_INT 0

// Some boards need INPUT (no pullup). Some need INPUT_PULLUP.
// Try switching this if INT always reads 1.
#define INT_MODE_INPUT_PULLUP 1

// If your CST820 needs a manual reset pulse at boot, enable this.
#define DO_TOUCH_RESET_PULSE 1

// I2C speed (400k is fine)
#define I2C_CLOCK_HZ 400000

// =====================
// Screen / Pins
// =====================
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 320;

// Touch pins (your factory sample)
#define I2C_SDA 33
#define I2C_SCL 32
#define TP_RST  25
#define TP_INT  21

TFT_eSPI tft = TFT_eSPI();
CST820 touch(I2C_SDA, I2C_SCL, TP_RST, TP_INT);

// =====================
// LVGL Draw Buffers (v9)
// =====================
static lv_draw_buf_t draw_buf1;
static lv_draw_buf_t draw_buf2;
static lv_color_t *buf_mem1;
static lv_color_t *buf_mem2;

// Partial buffer height in lines
static const uint32_t buf_lines = 40;

// =====================
// UI objects
// =====================
static lv_obj_t *btn = nullptr;
static lv_obj_t *label = nullptr;

// =====================
// Optional: raw I2C read helper (bypasses CST820 library)
// =====================
static bool cst820_read_bytes(uint8_t reg, uint8_t *out, size_t len)
{
  const uint8_t addr = 0x15; // you scanned 0x15

  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false; // repeated start

  // Cast len to uint8_t to avoid overload weirdness
  uint8_t got = Wire.requestFrom((uint8_t)addr, (uint8_t)len);
  if (got != len) return false;

  for (size_t i = 0; i < len; i++)
    out[i] = Wire.read();

  return true;
}

static void dump_cst820_frame(const char *tag)
{
  uint8_t b[8] = {0};
  bool ok = cst820_read_bytes(0x00, b, 7);
  if (!ok) {
    Serial.printf("[%s] CST820 read failed\n", tag);
    return;
  }

  // Prints 7 bytes from reg 0x00
  Serial.printf("[%s] CST820 raw:", tag);
  for (int i = 0; i < 7; i++) Serial.printf(" %02X", b[i]);
  Serial.println();
}

// =====================
// LVGL display flush (v9)
// =====================
static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
  uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);

  tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)px_map);

  lv_display_flush_ready(disp);
}

// =====================
// Touch read callback (LVGL v9)
// =====================
static void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data)
{
  (void)indev;

  // Optional gating: if INT never changes, disable GATE_READS_BY_INT
#if GATE_READS_BY_INT
  int intLevel = digitalRead(TP_INT);
  // Common behavior: INT goes LOW when touched
  if (intLevel == HIGH) {
    data->state = LV_INDEV_STATE_RELEASED;
#if PRINT_TOUCH_RELEASE
    Serial.printf("RELEASE (INT=1)\n");
#endif
#if DEBUG_RAW_I2C_DUMP
    dump_cst820_frame("INT=1");
#endif
    return;
  }
#endif

  bool touched;
  uint8_t gesture = 0;
  uint16_t x = 0, y = 0;

  touched = touch.getTouch(&x, &y, &gesture);

  if (!touched) {
    data->state = LV_INDEV_STATE_RELEASED;
#if PRINT_TOUCH_RELEASE
    Serial.printf("RELEASE (getTouch=false) x=%u y=%u g=%u\n", x, y, gesture);
#endif
#if DEBUG_RAW_I2C_DUMP
    dump_cst820_frame("getTouch=false");
#endif
    return;
  }

  // Raw touch values from the library
  Serial.printf("RAW x=%u y=%u gesture=%u INT=%d\n", x, y, gesture, digitalRead(TP_INT));

  // ---- Coordinate mapping (match TFT rotation) ----
  uint16_t tx = x;
  uint16_t ty = y;

  // If touch is rotated or mirrored, uncomment ONE at a time:

  // 1) Swap axes (very common)
  // tx = y; ty = x;

  // 2) Swap + mirror X
  // tx = screenWidth - 1 - y; ty = x;

  // 3) Swap + mirror Y
  // tx = y; ty = screenHeight - 1 - x;

  // 4) Swap + mirror both
  // tx = screenWidth - 1 - y; ty = screenHeight - 1 - x;

  // Clamp
  if (tx >= screenWidth)  tx = screenWidth - 1;
  if (ty >= screenHeight) ty = screenHeight - 1;

  Serial.printf("MAP x=%u y=%u\n", tx, ty);

  data->state = LV_INDEV_STATE_PRESSED;
  data->point.x = tx;
  data->point.y = ty;
}

// =====================
// Button events
// =====================
static void btn_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);

  if (code == LV_EVENT_PRESSED) {
    lv_obj_set_style_bg_color(obj, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_label_set_text(label, "TOUCHED");

    // LED = green
    rgb_set(0, 255, 0);
  }
  else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    lv_obj_set_style_bg_color(obj, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_label_set_text(label, "TEST");

    // LED = blue (or turn off if you prefer)
    rgb_set(0, 0, 0);
    // rgb_set(0, 0, 0); // <- use this instead for OFF on release
  }
}

static void build_ui()
{
  btn = lv_button_create(lv_screen_active());
  lv_obj_set_size(btn, 160, 80);
  lv_obj_center(btn);

  lv_obj_set_style_bg_opa(btn, LV_OPA_100, 0);
  lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_set_style_radius(btn, 12, 0);

  label = lv_label_create(btn);
  lv_label_set_text(label, "TEST");
  lv_obj_center(label);

  lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, nullptr);
}

void setup()
{
  Serial.begin(115200);
  delay(50);

  rgb_init();
  rgb_set(0, 0, 0); // off

  Serial.println("=== BOOT: LVGL + TFT_eSPI + CST820 touch ===");

  lv_init();

  // On many CYD-style boards, GPIO27 is used as backlight/power enable
  pinMode(27, OUTPUT);
  digitalWrite(27, LOW);

  // ---- TFT init ----
  tft.begin();
  tft.setRotation(0);
  tft.initDMA();

  // ---- I2C init for touch ----
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_CLOCK_HZ);

#if INT_MODE_INPUT_PULLUP
  pinMode(TP_INT, INPUT_PULLUP);
#else
  pinMode(TP_INT, INPUT);
#endif

#if DO_TOUCH_RESET_PULSE
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);
  delay(10);
  digitalWrite(TP_RST, HIGH);
  delay(50);
#endif

  // Start touch driver
  touch.begin();
  delay(20);

  // If you want to see what the chip is returning even before touching:
#if DEBUG_RAW_I2C_DUMP
  dump_cst820_frame("boot");
#endif

  digitalWrite(27, HIGH); // enable backlight/power

  // ---- LVGL draw buffers (DMA-capable) ----
  const uint32_t buf_pixels = (uint32_t)screenWidth * buf_lines;
  const size_t buf_bytes = buf_pixels * sizeof(lv_color_t);

  buf_mem1 = (lv_color_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  buf_mem2 = (lv_color_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  if(!buf_mem1 || !buf_mem2) {
    Serial.println("ERROR: Failed to allocate LVGL draw buffers.");
    while(1) delay(1000);
  }

  lv_draw_buf_init(&draw_buf1, screenWidth, buf_lines, LV_COLOR_FORMAT_RGB565, 0, buf_mem1, buf_bytes);
  lv_draw_buf_init(&draw_buf2, screenWidth, buf_lines, LV_COLOR_FORMAT_RGB565, 0, buf_mem2, buf_bytes);

  lv_display_t *disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_draw_buffers(disp, &draw_buf1, &draw_buf2);

  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  tft.startWrite();

  build_ui();
  Serial.println("UI ready. Touch should print RAW/MAP and the button should change.");

  // Quick INT sanity:
  Serial.printf("INT idle level = %d (try touching)\n", digitalRead(TP_INT));
}

void loop()
{
  // ---- LVGL needs time to move forward (ticks) ----
  static uint32_t last_ms = 0;
  uint32_t now = millis();
  uint32_t elapsed = now - last_ms;
  last_ms = now;

  // Feed LVGL elapsed time (ms)
  lv_tick_inc(elapsed);

  // Run LVGL tasks (this is where it will call your touch read callback)
  lv_timer_handler();

  delay(5);
}
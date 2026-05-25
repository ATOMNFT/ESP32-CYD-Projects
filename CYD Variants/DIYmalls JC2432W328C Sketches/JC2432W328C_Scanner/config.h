#ifndef JC2432W328C_SCANNER_CONFIG_H
#define JC2432W328C_SCANNER_CONFIG_H

// Default settings for JC2432W328C Scanner.
// These are compile-time defaults. The Settings screen updates runtime values.

#define CFG_BLE_SCAN_SECS     5      // seconds
#define CFG_WIFI_SCAN_SECS    5      // seconds
#define CFG_WIFI_MAX_RESULTS  20     // must be <= MAX_APS
#define CFG_BLE_MAX_RESULTS   20     // must be <= MAX_BLES

#define CFG_WIFI_SHOW_HIDDEN  true
#define CFG_BLE_ACTIVE_SCAN   true

#define CFG_BRIGHTNESS_PCT    100    // 0-100
#define CFG_SLEEP_TIMEOUT_S   0      // 0 = disabled
#define CFG_SLEEP_DIM_PCT     5      // 1-100 (brightness % while dimmed)

#define CFG_RGB_ENABLED       true
#define CFG_RGB_BOOT_TEST     true   // flash R/G/B on boot
#define CFG_RGB_BOOT_TEST_MS  320    // per color

// Home screen title (shown next to the WiFi icon)
#define CFG_HOME_TITLE        "ESP32 Scanner"

// ─────────────────────────────────────────────────────────────────────────────
// Themes (0..3)
// ─────────────────────────────────────────────────────────────────────────────
#define CFG_THEME_DEFAULT       0   // 0=Jungle, 1=Cyber, 2=Midnight, 3=Type-R

// Theme 0 (Jungle)
#define T0_STATUS_BG           0x102040
#define T0_SCREEN_BG           0x0D0D1A
#define T0_BORDER              0x00FF66
#define T0_WIFI_BTN            0x0F7A4A
#define T0_BLE_BTN             0x1E3A8A
#define T0_HDR_WIFI_BG         0x001040
#define T0_HDR_BLE_BG          0x180030
#define T0_HDR_SETTINGS_BG     0x101020
#define T0_LIST_WIFI_EVEN      0x1A1A2E
#define T0_LIST_WIFI_ODD       0x16213E
#define T0_LIST_BLE_EVEN       0x1A0A2E
#define T0_LIST_BLE_ODD        0x16082A

// Theme 1 (Cyber)
#define T1_STATUS_BG           0x200018
#define T1_SCREEN_BG           0x0B0712
#define T1_BORDER              0x07FFFF
#define T1_WIFI_BTN            0x3A7CFF
#define T1_BLE_BTN             0x7A2EFF
#define T1_HDR_WIFI_BG         0x001428
#define T1_HDR_BLE_BG          0x2A0048
#define T1_HDR_SETTINGS_BG     0x1A0A2E
#define T1_LIST_WIFI_EVEN      0x141427
#define T1_LIST_WIFI_ODD       0x101022
#define T1_LIST_BLE_EVEN       0x170A26
#define T1_LIST_BLE_ODD        0x130820

// Theme 2 (Midnight)
#define T2_STATUS_BG           0x001018
#define T2_SCREEN_BG           0x050A12
#define T2_BORDER              0x39E7FF
#define T2_WIFI_BTN            0x0E7490
#define T2_BLE_BTN             0x334155
#define T2_HDR_WIFI_BG         0x001827
#define T2_HDR_BLE_BG          0x0B1220
#define T2_HDR_SETTINGS_BG     0x0B1220
#define T2_LIST_WIFI_EVEN      0x0B1220
#define T2_LIST_WIFI_ODD       0x08101C
#define T2_LIST_BLE_EVEN       0x0B1220
#define T2_LIST_BLE_ODD        0x08101C


// Theme 3 (Type-R)
#define T3_STATUS_BG           0x2A0000
#define T3_SCREEN_BG           0x0A0A0A
#define T3_BORDER              0xFF1A1A
#define T3_WIFI_BTN            0xB30000
#define T3_BLE_BTN             0x1A1A1A
#define T3_HDR_WIFI_BG         0x1A0000
#define T3_HDR_BLE_BG          0x121212
#define T3_HDR_SETTINGS_BG     0x101010
#define T3_LIST_WIFI_EVEN      0x121212
#define T3_LIST_WIFI_ODD       0x0E0E0E
#define T3_LIST_BLE_EVEN       0x121212
#define T3_LIST_BLE_ODD        0x0E0E0E

// Button text colors
#define T0_BTN_TEXT           0xFFFFFF
#define T1_BTN_TEXT           0xFFFFFF
#define T2_BTN_TEXT           0xE6F0FF
#define T3_BTN_TEXT           0xFFFFFF

// Text colors
#define T0_TITLE_TEXT         0x07FFFF
#define T0_SUBTITLE_TEXT      0x8888AA
#define T0_HDR_WIFI_TEXT      0x07FFFF
#define T0_HDR_BLE_TEXT       0xCC88FF
#define T0_HDR_SETTINGS_TEXT  0xAABBCC
#define T0_LIST_TEXT          0xE6F0FF

#define T1_TITLE_TEXT         0x07FFFF
#define T1_SUBTITLE_TEXT      0x9AA0C8
#define T1_HDR_WIFI_TEXT      0x07FFFF
#define T1_HDR_BLE_TEXT       0xE0B3FF
#define T1_HDR_SETTINGS_TEXT  0xAABBCC
#define T1_LIST_TEXT          0xF2F7FF

#define T2_TITLE_TEXT         0x7DD3FC
#define T2_SUBTITLE_TEXT      0xA3B4C8
#define T2_HDR_WIFI_TEXT      0x7DD3FC
#define T2_HDR_BLE_TEXT       0xE2E8F0
#define T2_HDR_SETTINGS_TEXT  0xCBD5E1
#define T2_LIST_TEXT          0xE2E8F0

#define T3_TITLE_TEXT         0xFFFFFF
#define T3_SUBTITLE_TEXT      0xCFCFCF
#define T3_HDR_WIFI_TEXT      0xFFFFFF
#define T3_HDR_BLE_TEXT       0xFFFFFF
#define T3_HDR_SETTINGS_TEXT  0xFFFFFF
#define T3_LIST_TEXT          0xFFFFFF

// Panel / detail styling
#define T0_PANEL_BG           0x0D0D1A
#define T0_PANEL_TEXT         0xFFFFFF
#define T0_BACK_BTN_BG        0x222244

#define T1_PANEL_BG           0x0B0712
#define T1_PANEL_TEXT         0xFFFFFF
#define T1_BACK_BTN_BG        0x2A0048

#define T2_PANEL_BG           0x050A12
#define T2_PANEL_TEXT         0xE2E8F0
#define T2_BACK_BTN_BG        0x0B1220

#define T3_PANEL_BG           0x0A0A0A
#define T3_PANEL_TEXT         0xFFFFFF
#define T3_BACK_BTN_BG        0x1A1A1A

// Selection highlight
#define T0_SEL_BG             0x0F7A4A
#define T1_SEL_BG             0x7A2EFF
#define T2_SEL_BG             0x0E7490
#define T3_SEL_BG             0xB30000

#endif

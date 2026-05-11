#ifndef JC2432W328C_SCANNER_CONFIG_H
#define JC2432W328C_SCANNER_CONFIG_H

// Default settings for JC2432W328C Scanner
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

#endif

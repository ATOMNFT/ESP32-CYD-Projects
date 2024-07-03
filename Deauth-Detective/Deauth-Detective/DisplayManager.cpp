// ####################
//     Created by                                                   
// ╔═╗╔╦╗╔═╗╔╦╗╔╗╔╔═╗╔╦╗  
// ╠═╣ ║ ║ ║║║║║║║╠╣  ║   
// ╩ ╩ ╩ ╚═╝╩ ╩╝╚╝╚   ╩   
// ####################                                            

/* FLASH SETTINGS
Board: LOLIN D32
Flash Frequency: 80MHz
Partition Scheme: Minimal SPIFFS
*/

#include "DisplayManager.h"
#include "Config.h"
#include "ScanFaces20pt7b.h"

// Define TFT_eSPI settings
TFT_eSPI tft = TFT_eSPI();

// Custom font variables
#define CUSTOM_FONT_FACES &ScanFaces20pt7b  // Example custom font for faces

// Alternate characters
const char face1 = 's';
const char face2 = 'S';  // Alternate character for 's'

void DisplayManager::initDisplay() {
  tft.begin();
  
  #ifdef DISPLAY_TYPE_TTGO_T
    tft.setRotation(1);  // Adjust rotation for TTGO T-Display
  #elif defined(DISPLAY_TYPE_LOLIN_D32)
    tft.setRotation(1);  // Adjust rotation for LOLIN D32
  #endif
  
  tft.fillScreen(TFT_BLACK);
}

void DisplayManager::displayDeauthMessage(const char* attacker_mac) {
  tft.fillScreen(TFT_BLACK);

  // Display detected message with default font
  tft.setTextSize(2);
  tft.setCursor(5, 5);
  tft.setTextColor(TFT_RED);
  tft.print("Deauth Attack Detected!");

  // Switch to custom font for faces
  tft.setFreeFont(CUSTOM_FONT_FACES);
  tft.setTextSize(2);
  tft.setCursor(10, 100);
  tft.setTextColor(TFT_RED);
  tft.print("d");  // deauth-detected face character

  // Switch back to default font for details
  tft.setFreeFont(NULL);  // Set back to default font
  tft.setTextSize(2);
  tft.setCursor(5, 120);
  tft.setTextColor(TFT_RED);
  tft.println("Attacker");
  tft.setCursor(5, 140);
  tft.println("MAC:");
  tft.setTextSize(2);
  tft.setCursor(5, 158);
  tft.setTextColor(TFT_WHITE);
  tft.println(attacker_mac);
}

void DisplayManager::displayScanningChannel(uint8_t channel, int step) {
  tft.fillScreen(TFT_BLACK);  // Clear screen before updating
  tft.setFreeFont(NULL);  // Use default font for "Scanning Channel:"
  tft.setTextSize(2);
  tft.setCursor(5, 5);
  tft.setTextColor(TFT_GREEN);
  tft.print("Scanning Channel:");
  tft.print(channel);  // Display current channel number
  tft.setFreeFont(CUSTOM_FONT_FACES);  // Switch to custom font for character
  tft.setTextSize(2);
  tft.setTextColor(TFT_VIOLET);
  tft.setCursor(10, 100);

  switch (step) {
    case 0:
      tft.print(face1);  // Display 's' character
      break;
    case 1:
      tft.print(face2);  // Display 'S' character
      break;
    case 2:
      tft.print(face1);  // Display 's' character
      break;
    default:
      break;
  }
}

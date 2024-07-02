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

#include "deauth_detector.h"

// Define TFT_eSPI settings
TFT_eSPI tft = TFT_eSPI();

// Variables for deauth attack detection
uint8_t channel = 1;  // Initial channel for scanning
bool deauthDetected = false;
char attacker_mac[18];  // Buffer to store MAC address

// Alternate characters
const char face1 = 's';
const char face2 = 'S';  // Alternate character for 's'

// Function to process captured packets
void IRAM_ATTR sniffer_packet_handler(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) {
    return;  // Only interested in management packets
  }

  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*) buf;
  uint8_t* payload = pkt->payload;
  uint16_t length = pkt->rx_ctrl.sig_len;

  // Check for deauth packet
  uint8_t subtype = payload[0] & 0xF0;
  if (subtype == 0xC0) {
    // This is a deauthentication packet
    Serial.println("Deauth packet detected!");
    snprintf(attacker_mac, sizeof(attacker_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             payload[10], payload[11], payload[12],
             payload[13], payload[14], payload[15]);
    deauthDetected = true;

    // Turn the NeoPixel red
    digitalWrite(R_PIN, 0);
    digitalWrite(G_PIN, 255);
    digitalWrite(B_PIN, 255);
  }
}

// Display deauth attack message
void displayDeauthMessage() {
  tft.fillScreen(TFT_BLACK);

  // Display main message with default font
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

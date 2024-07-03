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

#include "DeauthDetector.h"
#include "DisplayManager.h"

// Variables for deauth attack detection
uint8_t DeauthDetector::channel = 1;  // Initial channel for scanning
bool DeauthDetector::deauthDetected = false;
char DeauthDetector::attacker_mac[18];  // Buffer to store MAC address

// Define the pins for each color channel
#define R_PIN 4
#define G_PIN 16
#define B_PIN 17

void DeauthDetector::initNeoPixelPins() {
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);

  // Start the NeoPixel green
  digitalWrite(R_PIN, HIGH);
  digitalWrite(G_PIN, LOW);
  digitalWrite(B_PIN, HIGH);
}

void DeauthDetector::scanChannels() {
  // Scan channels 1-13 for deauth attacks
  for (channel = 1; channel <= 13; channel++) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    Serial.print("Scanning Channel: ");
    Serial.println(channel);
    delay(100);  // Adjust delay as needed

    // Check if deauth attack detected
    if (deauthDetected) {
      DisplayManager::displayDeauthMessage(attacker_mac);
      delay(1000);  // Display message for 5 seconds
      deauthDetected = false;  // Reset flag

      // Turn NeoPixel back to green
      digitalWrite(R_PIN, HIGH);
      digitalWrite(G_PIN, LOW);
      digitalWrite(B_PIN, HIGH);
      
      break;  // Exit loop after detecting deauth attack
    }

    // Display current scanning channel with alternating 's' characters
    for (int i = 0; i < 3; ++i) {
      DisplayManager::displayScanningChannel(channel, i);
      delay(800);  // Adjust delay as needed
    }
  }
}

// Function to process captured packets
void IRAM_ATTR DeauthDetector::sniffer_packet_handler(void* buf, wifi_promiscuous_pkt_type_t type) {
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

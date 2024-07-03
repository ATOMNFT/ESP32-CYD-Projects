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

#include <WiFi.h>
#include <esp_wifi.h>
#include "DisplayManager.h"
#include "DeauthDetector.h"
#include "Config.h"

void setup() {
  Serial.begin(115200);

  // Initialize display
  DisplayManager::initDisplay();

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(DeauthDetector::sniffer_packet_handler);
  Serial.println("Promiscuous mode enabled");

  // Initialize the NeoPixel pins
  DeauthDetector::initNeoPixelPins();

  Serial.println("Setup done");
}

void loop() {
  DeauthDetector::scanChannels();
}

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

#ifndef DEAUTHDETECTOR_H
#define DEAUTHDETECTOR_H

#include <WiFi.h>
#include <esp_wifi.h>

class DeauthDetector {
public:
  static void initNeoPixelPins();
  static void scanChannels();
  static void IRAM_ATTR sniffer_packet_handler(void* buf, wifi_promiscuous_pkt_type_t type);

private:
  static uint8_t channel;
  static bool deauthDetected;
  static char attacker_mac[18];
};

#endif

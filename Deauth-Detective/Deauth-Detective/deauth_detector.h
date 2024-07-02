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

#ifndef DEAUTH_DETECTOR_H
#define DEAUTH_DETECTOR_H

#include <WiFi.h>
#include <esp_wifi.h>
#include <TFT_eSPI.h>
#include "ScanFaces20pt7b.h"  // Include your custom font header file

// Define TFT_eSPI settings
extern TFT_eSPI tft;

// Variables for deauth attack detection
extern uint8_t channel;
extern bool deauthDetected;
extern char attacker_mac[18];

// Custom font variables
#define CUSTOM_FONT_FACES  &ScanFaces20pt7b  // Example custom font for faces

// Alternate characters
extern const char face1;
extern const char face2;

// Define the pins for each color channel
#define R_PIN 4
#define G_PIN 16
#define B_PIN 17

// Function prototypes
void IRAM_ATTR sniffer_packet_handler(void* buf, wifi_promiscuous_pkt_type_t type);
void displayDeauthMessage();
delay(800);

#endif // DEAUTH_DETECTOR_H

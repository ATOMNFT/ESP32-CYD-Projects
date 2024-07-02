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

void setup() {
  Serial.begin(115200);

  // Initialize TFT display
  tft.begin();
  tft.setRotation(1);  // Adjust rotation as needed
  tft.fillScreen(TFT_BLACK);

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(sniffer_packet_handler);
  Serial.println("Promiscuous mode enabled");

  // Initialize the NeoPixel pins
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);

  // Start the NeoPixel green
  digitalWrite(R_PIN, HIGH);
  digitalWrite(G_PIN, LOW);
  digitalWrite(B_PIN, HIGH);

  Serial.println("Setup done");
}

void loop() {
  // Scan channels 1-13 for deauth attacks
  for (channel = 1; channel <= 13; channel++) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    Serial.print("Scanning Channel: ");
    Serial.println(channel);
    delay(100);  // Adjust delay as needed

    // Check if deauth attack detected
    if (deauthDetected) {
      displayDeauthMessage();
      delay(5000);  // Display message for 5 seconds
      deauthDetected = false;  // Reset flag

      // Turn NeoPixel back to green
      digitalWrite(R_PIN, HIGH);
      digitalWrite(G_PIN, LOW);
      digitalWrite(B_PIN, HIGH);
      
      break;  // Exit loop after detecting deauth attack
    }

    // Display current scanning channel with alternating 's' characters
    for (int i = 0; i < 3; ++i) {
      tft.fillScreen(TFT_BLACK);  // Clear screen before updating
      tft.setFreeFont(NULL);  // Use default font for "Scanning Channel:"
      tft.setTextSize(2);
      tft.setCursor(5, 5);
      tft.setTextColor(TFT_GREEN);
      tft.print("Scanning Channel: ");
      tft.setTextColor(TFT_ORANGE);
      tft.print(channel);  // Display current channel number
      tft.setFreeFont(CUSTOM_FONT_FACES);  // Switch to custom font for character
      tft.setTextSize(2);
      tft.setTextColor(TFT_VIOLET);
      tft.setCursor(10, 100);

      switch (i) {
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

      delay(800);  // Adjust delay as needed
    }
  }
}

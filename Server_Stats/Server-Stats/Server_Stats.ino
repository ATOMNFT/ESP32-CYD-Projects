#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

const char* ssid = "YOURSSID";  // Set to your wifi network name (Non 5G)
const char* password = "YOURPASSWORD";  // Set to your wifi password
const char* server_url = "YOURIPADDRESS:5000/stats";  // Set to the ip of your server followed by the port number. Port must match entry in system-stats.py

TFT_eSPI tft = TFT_eSPI();

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(server_url);
        int httpResponseCode = http.GET();
        
        if (httpResponseCode > 0) {
            String payload = http.getString();
            Serial.println(payload);
            
            StaticJsonDocument<500> doc;
            deserializeJson(doc, payload);
            
            float cpu = doc["cpu"];
            float memory = doc["memory"];
            float disk = doc["disk"];
            String uptime = doc["uptime"];  // Now uptime is a string like "7:01:43"
            String cpu_temp = doc["cpu_temp"];  // Fetch CPU temperature as a string

            // Clear the display
            tft.fillScreen(TFT_BLACK);
            
            // Display the title
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setTextSize(3);  // Larger text for the title
            tft.setCursor(10, 10);
            tft.print("Server Stats");

            // Draw horizontal line below the title
            tft.drawLine(10, 40, 280, 40, TFT_RED);  // x1, y1, x2, y2, color

            // Set text size back to normal for stats
            tft.setTextSize(2);

            // Display the stats
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);  // Set color for CPU stats
            tft.setCursor(10, 50);  // Start below the line
            tft.printf("CPU: %.1f%%\n", cpu);
            tft.setTextColor(TFT_BLUE, TFT_BLACK);  // Set color for memory stats
            tft.setCursor(10, 80);
            tft.printf("Mem: %.1f%%\n", memory);
            tft.setTextColor(TFT_VIOLET, TFT_BLACK);  // Set color for disk stats
            tft.setCursor(10, 110);
            tft.printf("Disk: %.1f%%\n", disk);
            tft.setTextColor(TFT_MAGENTA, TFT_BLACK);  // Set color for uptime stats
            tft.setCursor(10, 140);
            tft.printf("Uptime: %s\n", uptime.c_str());  // Display uptime string directly
            tft.setTextColor(TFT_CYAN, TFT_BLACK);  // Set color for CPU temperature
            tft.setCursor(10, 170);
            tft.printf("CPU Temp: %s\n", cpu_temp.c_str());  // Display CPU temperature
        }
        http.end();
    }
    delay(5000);  // Refresh every 5 seconds
}

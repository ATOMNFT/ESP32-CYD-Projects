#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>

// TFT display
TFT_eSPI tft = TFT_eSPI();

// RGB LED GPIO pins
#define R_PIN 4
#define G_PIN 16
#define B_PIN 17

// WiFi credentials
const char *ssid = "YOU";
const char *password = "YOU";

// GitHub API endpoint and repository info
const char *githubApiUrl = "YOU";
const char *githubToken = "YOU"; // Optional: Use a personal access token for better rate limits

// Buffer size for HTTP response
const int bufferSize = JSON_OBJECT_SIZE(6) + 210;

// Touch button dimensions and colors
const int buttonWidth = 120;
const int buttonHeight = 40;
const int buttonColor = TFT_BLUE;
const int textColor = TFT_WHITE;

// Timer variables
unsigned long previousMillis = 0;
const long interval = 60000; // 1 minute interval for refreshing

void setup() {
  Serial.begin(115200);
  tft.begin(); // Initialize TFT display

  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);

  connectToWiFi();

  // Draw initial screen
  drawScreen();

  // Fetch GitHub stats initially
  fetchGitHubStats();
}

void loop() {
  unsigned long currentMillis = millis();

  // Check if it's time to refresh GitHub stats
  if (currentMillis - previousMillis >= interval) {
    fetchGitHubStats(); // Refresh GitHub stats
    previousMillis = currentMillis; // Save the last time the refresh occurred
  }

  // Handle touch input
  touchHandler();
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
}

void fetchGitHubStats() {
  Serial.println("Fetching GitHub stats");

  // Set up HTTP client
  HTTPClient http;
  http.begin(githubApiUrl);
  if (strlen(githubToken) > 0) {
    http.addHeader("Authorization", "token " + String(githubToken));
  }

  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    if (httpResponseCode == HTTP_CODE_OK) {
      String response = http.getString();
      Serial.println("Response:");
      Serial.println(response);

      // Parse JSON response
      DynamicJsonDocument doc(bufferSize);
      deserializeJson(doc, response);

      // Extract GitHub stats
      String repoName = doc["full_name"];
      int stars = doc["stargazers_count"];
      int forks = doc["forks_count"];
      int issues = doc["open_issues_count"];
      String lastCommit = doc["pushed_at"];

      // Display GitHub stats on TFT screen
      displayStats(repoName, stars, forks, issues, lastCommit);

      // Turn off LED after updating the stats
      digitalWrite(R_PIN, HIGH); // Red off
      digitalWrite(G_PIN, HIGH); // Green off
      digitalWrite(B_PIN, HIGH); // Blue off
    }
  } else {
    Serial.printf("HTTP Error: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end(); // Close connection
}

void displayStats(String repoName, int stars, int forks, int issues, String lastCommit) {
  tft.fillScreen(TFT_BLACK); // Clear screen
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("GitHub Stats:");

  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.print("Repository: ");
  tft.println(repoName);
  tft.print("Stars: ");
  tft.println(stars);
  tft.print("Forks: ");
  tft.println(forks);
  tft.print("Open Issues: ");
  tft.println(issues);
  tft.print("Last Commit: ");
  tft.println(lastCommit);

  // Calculate x-coordinate for centered button
  int buttonX = (tft.width() - buttonWidth) / 2;
  drawButton("Refresh", textColor, buttonColor, buttonX, 180);
}

void drawScreen() {
  tft.fillScreen(TFT_BLACK); // Clear screen
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("GitHub Stats:");
}

void drawButton(String label, uint32_t textColor, uint32_t buttonColor, int x, int y) {
  tft.fillRect(x, y, buttonWidth, buttonHeight, buttonColor);
  tft.setTextColor(textColor);
  tft.setTextSize(1);
  tft.setCursor(x + 10, y + 10);
  tft.println(label);
}

void touchHandler() {
  uint16_t x, y;
  uint16_t threshold = 30; // Touch sensitivity
  if (tft.getTouch(&x, &y, threshold)) {
    // Calculate button bounds
    int buttonX = (tft.width() - buttonWidth) / 2;
    int buttonY = 180;
    int buttonEndX = buttonX + buttonWidth;
    int buttonEndY = buttonY + buttonHeight;

    if (x >= buttonX && x <= buttonEndX && y >= buttonY && y <= buttonEndY) {
      // Refresh button pressed, make LED blue
      digitalWrite(R_PIN, HIGH); // Red off
      digitalWrite(G_PIN, HIGH); // Green off
      digitalWrite(B_PIN, LOW);  // Blue on

      // Fetch GitHub stats again
      fetchGitHubStats();
    }
  }
}

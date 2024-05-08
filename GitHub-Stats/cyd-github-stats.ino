#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <time.h>

// TFT display
TFT_eSPI tft = TFT_eSPI();

// RGB LED GPIO pins
#define R_PIN 4
#define G_PIN 16
#define B_PIN 17

// WiFi credentials
const char* ssid = "YOU";
const char* password = "YOU;

// GitHub API endpoint and repository info
const char* githubApiUrl = "YOU";
const char* githubUserUrl = "YOU"; // User API for follower count
const char* githubToken = "YOU"; // Use a personal access token for better rate limits

// Buffer size for HTTP response
const int bufferSize = JSON_OBJECT_SIZE(10) + 400;

// Touch button dimensions and colors
const int buttonWidth = 120;
const int buttonHeight = 40;
const int buttonColor = TFT_BLUE;
const int textColor = TFT_WHITE;

// Timer variables
unsigned long previousMillis = 0;
const long interval = 60000; // 1 minute interval for refreshing

// Time of the last API check
time_t lastRefreshTime = 0;

void setup() {
  Serial.begin(115200);
  tft.begin(); // Initialize TFT display

  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);

  // Initially turn off all LEDs
  digitalWrite(R_PIN, HIGH);
  digitalWrite(G_PIN, HIGH);
  digitalWrite(B_PIN, HIGH);

  connectToWiFi();

  // Initialize time with NTP
  configTime(-6 * 3600, 3600, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
  tzset();

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

  HTTPClient http;
  http.begin(githubApiUrl);
  if (strlen(githubToken) > 0) {
    http.addHeader("Authorization", "token " + String(githubToken));
  }

  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    if (httpResponseCode == HTTP_CODE_OK) {
      lastRefreshTime = time(nullptr); // Update the last refresh time on successful fetch
      String response = http.getString();
      Serial.println("Response:");
      Serial.println(response);

      DynamicJsonDocument docRepo(bufferSize);
      deserializeJson(docRepo, response);

      String repoName = docRepo["full_name"];
      int stars = docRepo["stargazers_count"];
      int forks = docRepo["forks_count"];
      int issues = docRepo["open_issues_count"];
      String lastCommit = docRepo["pushed_at"];

      // Also fetch and display follower count
      HTTPClient httpUser;
      httpUser.begin(githubUserUrl);
      if (strlen(githubToken) > 0) {
        httpUser.addHeader("Authorization", "token " + String(githubToken));
      }
      httpResponseCode = httpUser.GET();
      if (httpResponseCode == HTTP_CODE_OK) {
        String userResponse = httpUser.getString();
        DynamicJsonDocument docUser(bufferSize);
        deserializeJson(docUser, userResponse);
        int followers = docUser["followers"];

        displayStats(repoName, stars, forks, issues, lastCommit, followers);

        digitalWrite(R_PIN, HIGH); // Red off
        digitalWrite(G_PIN, HIGH); // Green off
        digitalWrite(B_PIN, HIGH); // Blue off
      
      } else {
        Serial.printf("HTTP Error getting user data: %s\n", httpUser.errorToString(httpResponseCode).c_str());
      }
      httpUser.end();
    }
  } else {
    Serial.printf("HTTP Error getting repo data: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end(); // Close connection

  // Turn off LED after updating the stats
  digitalWrite(R_PIN, HIGH); // Red off
  digitalWrite(G_PIN, HIGH); // Green off
  digitalWrite(B_PIN, HIGH); // Blue off
}

void displayStats(String repoName, int stars, int forks, int issues, String lastCommit, int followers) {
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
  tft.print("Followers: ");
  tft.println(followers);

  struct tm *tm_info = localtime(&lastRefreshTime);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
  tft.setCursor(10, 200);  // Adjust position as needed
  tft.print("Last checked: ");
  tft.println(buffer);

  // Draw refresh button
  int buttonX = (tft.width() - buttonWidth) / 2;
  drawButton("Refresh", textColor, buttonColor, buttonX, 260); // Adjust button position if needed
}

void drawScreen() {
    tft.fillScreen(TFT_BLACK); // Clear the screen
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("GitHub Stats:");

    // Redraw button in case it moved or was not initialized
    drawButton("Refresh", textColor, buttonColor, (tft.width() - buttonWidth) / 2, tft.height() - buttonHeight - 10);
}

void drawButton(String label, uint32_t textColor, uint32_t buttonColor, int x, int y) {
  tft.fillRect(x, y, buttonWidth, buttonHeight, buttonColor);
  tft.setTextColor(textColor);
  tft.setTextSize(1);
  int textWidth = tft.textWidth(label); // Calculate the width of the label text
  int textX = x + (buttonWidth - textWidth) / 2; // Calculate x-coordinate for centering
  int textY = y + (buttonHeight - tft.fontHeight()) / 2; // Calculate y-coordinate for centering vertically
  tft.setCursor(textX, textY);
  tft.println(label);
}

void touchHandler() {
    uint16_t x, y;
    uint16_t threshold = 30; // Touch sensitivity
    if (tft.getTouch(&x, &y, threshold)) {
        Serial.print("Touch at: (");
        Serial.print(x);
        Serial.print(", ");
        Serial.print(y);
        Serial.println(")");

        // Button coordinates as per the visual position
        int buttonX = 37;
        int buttonY = 142;
        int buttonEndX = buttonX + buttonWidth;
        int buttonEndY = buttonY + buttonHeight;

        if (x >= buttonX && x <= buttonEndX && y >= buttonY && y <= buttonEndY) {
            // Refresh button pressed, make LED blue
            digitalWrite(R_PIN, HIGH); // Red off
            digitalWrite(G_PIN, HIGH); // Green off
            digitalWrite(B_PIN, LOW);  // Blue on

            // Fetch GitHub stats again
            fetchGitHubStats();

            // Turn off the LED after some time or in fetchGitHubStats
            digitalWrite(B_PIN, HIGH); // Blue off
        }
    }
}
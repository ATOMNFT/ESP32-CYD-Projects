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
const char* password = "YOU";

// GitHub API endpoint and repository info
const char* githubApiUrl = "https://api.github.com/repos/YOU/REPO"; // Repo you want to monitor
const char* githubUserUrl = "https://api.github.com/users/YOURUSERNAME"; // User API for follower count
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

      // Fetch notification count
      HTTPClient httpNotifications;
      httpNotifications.begin("https://api.github.com/notifications");
      if (strlen(githubToken) > 0) {
        httpNotifications.addHeader("Authorization", "token " + String(githubToken));
      }
      int notificationsCount = 0;
      httpResponseCode = httpNotifications.GET();
      if (httpResponseCode == HTTP_CODE_OK) {
        String notificationsResponse = httpNotifications.getString();
        DynamicJsonDocument docNotifications(2000); // Adjust size based on expected notifications
        deserializeJson(docNotifications, notificationsResponse);
        notificationsCount = docNotifications.size(); // Count notifications
      } else {
        Serial.printf("HTTP Error getting notifications: %s\n", httpNotifications.errorToString(httpResponseCode).c_str());
      }
      httpNotifications.end();

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

        displayStats(repoName, stars, forks, issues, lastCommit, followers, notificationsCount);

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
}

void displayStats(String repoName, int stars, int forks, int issues, String lastCommit, int followers, int notificationsCount) {
  tft.fillScreen(TFT_BLACK); // Clear screen
  tft.setTextColor(TFT_WHITE); // Edit for heading color
  tft.setTextSize(2);
  tft.setCursor(45, 10);
  tft.println("GitHub Stats:");

  tft.setTextColor(TFT_YELLOW); // Edit for stats color
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.print("Repository: ");
  tft.println(repoName);
  tft.setCursor(10, 50);
  tft.print("Stars: ");
  tft.println(stars);
  tft.setCursor(10, 60);
  tft.print("Forks: ");
  tft.println(forks);
  tft.setCursor(10, 70);
  tft.print("Open Issues: ");
  tft.println(issues);
  tft.setCursor(10, 80);
  tft.print("Last Commit: ");
  tft.println(lastCommit);
  tft.setCursor(10, 90);
  tft.print("Followers: ");
  tft.println(followers);
  tft.setCursor(10, 100);
  tft.print("Notifications: ");
  tft.println(notificationsCount);
  tft.setCursor(10, 110);

  struct tm *tm_info = localtime(&lastRefreshTime);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
  tft.setCursor(28, 245);  // Adjust position as needed
  tft.print("Refreshed: ");
  tft.println(buffer);
 

  // Draw refresh button
  int buttonX = (tft.width() - buttonWidth) / 2;
  drawButton("Refresh", textColor, buttonColor, buttonX, 265); // Adjust button position if needed
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
        int buttonX = 19;
        int buttonY = 133;
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

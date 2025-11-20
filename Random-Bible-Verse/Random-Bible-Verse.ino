/*
  ╔══════════════════════════════════════════════════════════════╗
  ║                     Bible Verse Display                      ║
  ║                    Created by: ATOMNFT                       ║
  ╚══════════════════════════════════════════════════════════════╝

  The ESP32 displays rotating Bible verses fetched online.
  On first boot, connect to the WiFi access point from mobile phone or computer:

        ▶  Bible_Verses_AP

  Then open a browser and visit:

        ▶  http://192.168.4.1

  In the setup portal you can:
     • Enter Wi-Fi credentials (SSID & Password)
     • Choose refresh interval (3 min, 5 min, or custom seconds)

  Device remembers your settings permanently.

  — Designed + Built by ATOMNFT
*/

#include <WiFi.h>
#include <TFT_eSPI.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <Preferences.h>

#include "splash_screen.h"   // contains: const uint16_t splashImg[240][320]

TFT_eSPI tft = TFT_eSPI();
String apiEndpoint = "https://bible-api.com/data/kjv/random";

String verse = "";
String reference = "";

WiFiManager wifiManager;
Preferences prefs;

int margin = 10;

/* ============================================================
   Custom HTML Block (Refresh Interval UI + Pastel Theme + Branding)
   ============================================================ */
const char refreshHTML[] PROGMEM = R"HTML(
<style>
  body { font-family:-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto; }
  .card {
    background: #f8faff;
    border: 1px solid #d0d7e2;
    padding: 18px;
    border-radius: 14px;
    margin-top: 20px;
  }
  .card h3 { margin-top:0; color:#4a5c78; }
  label { color:#4a5c78; font-size:14px; }
  input[type=number] {
    padding:6px; border-radius:10px; border:1px solid #c5ccd8;
    width:150px; margin-top:6px;
  }
  footer {
    margin-top:25px; font-size:14px; text-align:center;
    color:#9aa3b5;
  }
</style>

<div class='card'>
  <h3>Refresh Interval</h3>
  <p>Select how often new Bible verses are loaded:</p>

  <input type='radio' name='rsel' value='180' id='r180' checked>
  <label for='r180'>Every 3 minutes</label><br><br>

  <input type='radio' name='rsel' value='300' id='r300'>
  <label for='r300'>Every 5 minutes</label><br><br>

  <input type='radio' name='rsel' value='custom' id='rcustom'>
  <label for='rcustom'>Custom:</label><br>
  <input type='number' id='custom_inp' placeholder='seconds'><br><br>

  <input type='hidden' id='refresh_final' name='refresh_final' value='180'>

  <script>
    function updateFinal() {
      var r3 = document.getElementById('r180').checked;
      var r5 = document.getElementById('r300').checked;
      var rc = document.getElementById('rcustom').checked;
      var cv = document.getElementById('custom_inp').value;

      if (r3) document.getElementById('refresh_final').value = '180';
      else if (r5) document.getElementById('refresh_final').value = '300';
      else if (rc && cv.length > 0) document.getElementById('refresh_final').value = cv;
    }

    document.querySelectorAll("input").forEach(el=>{
      el.addEventListener("input",updateFinal);
      el.addEventListener("change",updateFinal);
    });
  </script>
</div>

<footer>Made with ❤️ by ATOMNFT</footer>
)HTML";

/* Hidden field that stores final chosen refresh time */
WiFiManagerParameter refreshBlock(refreshHTML);
// type='hidden' so it doesn’t show as a text box in the portal
WiFiManagerParameter refreshHidden("refresh_final", "", "", 10, "type='hidden'");

/* ================================
   Forward Declarations
   ================================ */
void fetchBibleVerse();
void displayVerse();
int measureWrappedHeight(const String&, int, int);
void drawWrappedText(const String&, int, int, int, int, uint16_t, int);
void showSplash();

/* ================================
   SETUP
   ================================ */
void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);          // landscape: 320x240
  tft.fillScreen(TFT_BLACK);

  // --- Show splash screen with slide-up animation ---
  showSplash();

  // --- Wi-Fi setup instructions screen ---
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(10,10); 
  tft.print("Wi-Fi Setup:");

  tft.setTextSize(1);
  tft.setCursor(10,40); 
  tft.print("1. Open Wi-Fi settings on mobile.");
  tft.setCursor(10,55); 
  tft.print("2. Connect to:");
  tft.setCursor(10,70); 
  tft.setTextColor(TFT_GREEN);
  tft.print("   Bible_Verses_AP");
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(10,90); 
  tft.print("3. Open browser:");
  tft.setCursor(10,105); 
  tft.setTextColor(TFT_GREEN);
  tft.print("   192.168.4.1");
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(10,125);
  tft.print("Set Wi-Fi + verse interval in portal.");

  // Add custom UI to WiFiManager
  wifiManager.addParameter(&refreshBlock);
  wifiManager.addParameter(&refreshHidden);

  wifiManager.setCaptivePortalEnable(true);
  wifiManager.setConfigPortalBlocking(true);
  wifiManager.autoConnect("Bible_Verses_AP");

  // Read interval from hidden field
  int refreshTime = 180; // default 3 minutes
  if (strlen(refreshHidden.getValue()) > 0) {
    int r = atoi(refreshHidden.getValue());
    if (r > 0) refreshTime = r;
  }

  prefs.begin("settings", false);
  prefs.putInt("refresh", refreshTime);
  prefs.end();

  Serial.print("Using refresh interval: ");
  Serial.println(refreshTime);

  // Connected screen
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10,10);
  tft.print("Connected!");

  delay(1500);

  fetchBibleVerse();
  displayVerse();
}

/* ================================
   LOOP
   ================================ */
void loop() {
  prefs.begin("settings", true);
  int refreshSec = prefs.getInt("refresh", 180);  // default again 3 min
  prefs.end();

  fetchBibleVerse();
  displayVerse();

  delay(refreshSec * 1000);
}

/* ================================
   Splash Screen (slide-up animation, ~3s)
   ================================ */
void showSplash() {
  int screenW = tft.width();   // 320
  int screenH = tft.height();  // 240

  const int imgW = 320;
  const int imgH = 240;

  const int step = 8;          // reveal in 8px chunks
  const int totalSteps = imgH / step;  // 240/8 = 30
  const int totalDuration = 3000;      // 3 seconds
  const int delayPerStep = totalDuration / totalSteps; // ~66ms

  tft.fillScreen(TFT_BLACK);
  tft.startWrite();

  for (int h = step; h <= imgH; h += step) {
    int srcY = imgH - h;   // start row in image
    int dstY = screenH - h; // draw so bottom stays aligned

    // draw h rows of the image, aligned at bottom
    tft.pushImage(0, dstY, imgW, h, (uint16_t*)&splashImg[srcY][0]);
    tft.dmaWait(); // just in case (no-op if DMA not used)
    tft.endWrite();
    delay(delayPerStep);
    tft.startWrite();
  }

  tft.endWrite();
  delay(300); // small pause at fully revealed state
}

/* ================================
   Fetch Verse
   ================================ */
void fetchBibleVerse() {
  HTTPClient http;
  http.begin(apiEndpoint);

  int code = http.GET();
  Serial.print("HTTP Status: "); 
  Serial.println(code);

  if (code == 200) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, payload);

    verse = doc["random_verse"]["text"].as<String>();
    reference =
      doc["random_verse"]["book"].as<String>() + " " +
      String(doc["random_verse"]["chapter"].as<int>()) + ":" +
      String(doc["random_verse"]["verse"].as<int>());
  }
  http.end();
}

/* ================================
   Word Wrap Helpers
   ================================ */
int measureWrappedHeight(const String& text, int maxWidth, int textSize) {
  tft.setTextSize(textSize);
  int lh = 8 * textSize + 2;

  String line="", word="";
  int lines=0;

  auto flush=[&](){ if(line.length()){lines++; line="";} };

  for(char c: text) {
    if(c==' '||c=='\n'){
      String test=line+(line.length()?" ":"")+word;
      if(tft.textWidth(test) > maxWidth){ flush(); line=word; }
      else line=test;
      word="";
      if(c=='\n') flush();
    } else word+=c;
  }

  // tail
  if(word.length()){
    String test=line+(line.length()?" ":"")+word;
    if(tft.textWidth(test) > maxWidth){ flush(); line=word; }
    else line=test;
  }
  if(line.length()) lines++;

  return lines * lh;
}

void drawWrappedText(const String& text, int x, int y, int maxWidth,
                     int textSize, uint16_t color, int maxBottom) {
  tft.setTextSize(textSize);
  tft.setTextColor(color);

  int lh = 8 * textSize + 2;
  String line="", word="";

  auto draw=[&](String L){
    tft.setCursor(x,y); 
    tft.print(L); 
    y+=lh;
  };

  auto dots=[&](String L){
    while(tft.textWidth(L+"...")>maxWidth && L.length()) L.remove(L.length()-1);
    tft.setCursor(x,y); 
    tft.print(L+"...");
  };

  for(char c: text){
    if(c==' '||c=='\n'){
      String test=line+(line.length()?" ":"")+word;
      if(tft.textWidth(test)>maxWidth){
        if(maxBottom!=-1 && y+lh>maxBottom){ dots(line); return; }
        draw(line); 
        line=word;
      }
      else line=test;

      word="";
      if(c=='\n'){
        if(maxBottom!=-1 && y+lh>maxBottom){ dots(line); return; }
        draw(line); 
        line="";
      }
    }
    else word+=c;
  }

  // tail
  if(word.length()){
    String test=line+(line.length()?" ":"")+word;
    if(tft.textWidth(test)>maxWidth){
      if(maxBottom!=-1 && y+lh>maxBottom){ dots(line); return; }
      draw(line); 
      line=word;
    }
    else line=test;
  }
  if(line.length()){
    if(maxBottom!=-1 && y+lh>maxBottom){ dots(line); return; }
    draw(line);
  }
}

/* ================================
   Display Verse
   ================================ */
void displayVerse() {
  tft.fillScreen(TFT_BLACK);

  int SW = tft.width();
  int SH = tft.height();

  const uint16_t VC = TFT_CYAN;
  const uint16_t RC = TFT_YELLOW;

  int vs = 2;
  int rs = 1;

  int refHeight = measureWrappedHeight(reference, SW - 2*margin, rs);
  int refTop = SH - margin - refHeight;

  // reference at bottom
  drawWrappedText(reference, margin, refTop, SW - 2*margin, rs, RC, -1);

  // verse in remaining space above
  drawWrappedText(verse, margin, margin, SW - 2*margin, vs, VC, refTop - margin);
}

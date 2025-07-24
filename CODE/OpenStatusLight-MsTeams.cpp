// Copyright (C) 2025 Pouria Müller
//
// This file is part of OpenStatusLight.
//
// OpenStatusLight is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or 
// any later version.
//
// OpenStatusLight is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <TaskScheduler.h>
// -----------------------------------------------------------------------------------------------------------------------------------------
// START OF USER CONFIG 

//LED CONFIG
//DATA Pin of ESP32 -- Needs to support PWM - int do not use ["]
#define LED_PIN 21

//Amount of LEDs in Series - int do not use ["]
#define LED_COUNT 24

  
// WEBHOOK CONFIG 
// Discord Example Value: "https://discord.com/api/webhooks/#############/#############"
// Teams only Supports it in Channels (not chats) and is actively pushing to abandon it - USE AT OWN RISK
#define WEBHOOK_URL "ChangeMe"

  
//WIFI CONFIG
//!!!CASE SENSITIVE!!!
//WIFI Name
const char* ssid = "ChangeMe";
//WIFI Password (Supports Spaces if needed)
const char* password = "ChangeMe";


// Microsoft Graph API
// You will need a APP-Registeration ( --NO-- Special Account or Azure Subscription Required) in Azure
// If your Organisation does not allow it write me a pm and/or ask your IT DEPT

// client_id: Application (Client) ID
// Azure / Home / App Registrations
const char* client_id = "ChangeMe";

// client_secret: Secret Value of the Client Key
// Azure / Home / App Registrations -> Manage - Certificates & Secrets (Create a New one) 
const char* client_secret = "ChangeMe";

// tenant_id: Directory (tenant) ID
//  Azure / Home / App Registrations
const char* tenant_id = "ChangeMe";

//USER IDs OF Teams Accounts
// Azure (Entra) / Users -- OR -- Use the API Sandbox https://developer.microsoft.com/en-us/graph/graph-explorer -- LOGIN and use Funktion (GET) user by Email 
// You can enter as many as u want to track (If you share a Room) or just one id -- Seperated by ","
String users[] = {"ChangeMe"}; // {"ChangeMe", "ChangeMe", "ChangeMe"}

// END OF USER CONFIG 
// -----------------------------------------------------------------------------------------------------------------------------------------









String refresh_token = "";
String access_token = "";
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Scheduler runner;  
int numUsers = sizeof(users) / sizeof(users[0]);


// Status Check Task every 15 seconds 
void checkAndSetStatus();
Task statusCheckTask(10000, TASK_FOREVER, checkAndSetStatus);  // YOU CAN MODIFY IT BUT I WOULD NOT RECCOMEND GOING BELOW 5000 (5 sec)


// --- LED Control ---
void setLeds(int status) {
  for (int i = 0; i < LED_COUNT; i++) {
    if (status == 1) strip.setPixelColor(i, strip.Color(255, 0, 0));     // Red (Busy)
    else if (status == 2) strip.setPixelColor(i, strip.Color(0, 255, 0)); // Green (Available)
    else if (status == 3) strip.setPixelColor(i, strip.Color(136, 0, 255)); // Purple (WLAN)
    else if (status == 4) strip.setPixelColor(i, strip.Color(0, 0, 255)); // Blue (Error)
    else if (status == 5) strip.setPixelColor(i, strip.Color(255, 50, 0)); // Orange (AFK)
    else strip.setPixelColor(i, strip.Color(0, 0, 0));  // Off
  }
  strip.show();
}



// --- Presence API Check ---
int checkPresence(String& activityOut) {
  HTTPClient http;
  int highestPriority = 0;
  activityOut = "";

  for (int i = 0; i < numUsers; i++) {
    String url = "https://graph.microsoft.com/v1.0/users/" + users[i] + "/presence";
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + access_token);
    int code = http.GET();
    String res = http.getString();

    if (code == 401) {
      refreshAccessToken();
      http.end();
      return checkPresence(activityOut);
    }

    if (code == 200) {
      DynamicJsonDocument doc(1024);
      DeserializationError err = deserializeJson(doc, res);
      if (!err) {
        String presence = doc["availability"].as<String>();
        String activity = doc["activity"].as<String>();
        if (activity == "InACall" || activity == "InAConferenceCall" || activity == "InAMeeting") activityOut = activity;
        int p = (presence == "Busy") ? 3 : (presence == "Available") ? 2 : (presence == "Away" || presence == "BeRightBack") ? 1 : 0;
        if (p > highestPriority) highestPriority = p;
      }
    }
    http.end();
  }
  return highestPriority;
}

// --- Token-Handling ---
void startDeviceAuth() {
  HTTPClient http;
  http.begin("https://login.microsoftonline.com/" + String(tenant_id) + "/oauth2/v2.0/devicecode");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "client_id=" + String(client_id) + "&scope=offline_access%20Presence.Read.All";
  int httpResponseCode = http.POST(body);

  if (httpResponseCode == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);

    String device_code = doc["device_code"].as<String>();
    String user_code = doc["user_code"].as<String>();
    String verification_uri = doc["verification_uri"].as<String>();

    sendToWebsocket(verification_uri, user_code);
    Serial.println(verification_uri +"  "+ user_code);

    delay(15000);
    pollForToken(device_code);
  } else {
    setLeds(4);
  }
  http.end();
}

void pollForToken(String device_code) {
  HTTPClient http;
  int attempts = 0;
  while (attempts < 30) {
    http.begin("https://login.microsoftonline.com/" + String(tenant_id) + "/oauth2/v2.0/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = "client_id=" + String(client_id) + "&client_secret=" + String(client_secret) +
                  "&grant_type=urn:ietf:params:oauth:grant-type:device_code&device_code=" + device_code;

    int httpResponseCode = http.POST(body);

    if (httpResponseCode == 200) {
      String response = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);

      access_token = doc["access_token"].as<String>();
      refresh_token = doc["refresh_token"].as<String>();
      http.end();
      return;
    } else {
      String response = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      String error = doc["error"].as<String>();
      if (error != "authorization_pending") {
        setLeds(4);
        break;
      }
    }
    delay(5000);
    attempts++;
    http.end();
  }
}

void refreshAccessToken() {
  HTTPClient http;
  http.begin("https://login.microsoftonline.com/" + String(tenant_id) + "/oauth2/v2.0/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "client_id=" + String(client_id) +
                "&refresh_token=" + String(refresh_token) + "&grant_type=refresh_token";

  int httpResponseCode = http.POST(body);

  if (httpResponseCode == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    access_token = doc["access_token"].as<String>();
    if (doc.containsKey("refresh_token")) refresh_token = doc["refresh_token"].as<String>();
  } else {
    setLeds(4);
    sendToWebsocket("ERROR", "TOKEN REFRESH FAILED");
    while (true) delay(60000);
  }
  http.end();
}

// blinking & Task-Handling 
bool blinking = false;
Task blinkTask(100, TASK_FOREVER, []() {
  static int brightness = 0;
  static int direction = 5;  // Fade Steps

  brightness += direction;
  if (brightness >= 255) {
    brightness = 255;
    direction = -5;
  } else if (brightness <= 30) {  // Bottom Threshhold
    brightness = 30;
    direction = 5;
  }

  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(brightness, 0, 0));  // Red with adapting Brightness
  }
  strip.show();
});

// --- Status Check Logic ---
void checkAndSetStatus() {
  if (WiFi.status() != WL_CONNECTED) return;

  String activity = "";
  int prio = checkPresence(activity);

  if (activity == "InACall" || activity == "InAConferenceCall" || activity == "InAMeeting") {
    if (!blinkTask.isEnabled()) blinkTask.enable();
  } else {
    if (blinkTask.isEnabled()) blinkTask.disable();
    setLeds(prio == 3 ? 1 : prio == 2 ? 2 : prio == 1 ? 5 : 0);
  }
}

// --- websocket Send Message ---
void sendToWebsocket(String verification_uri, String user_code) {
  HTTPClient http;
  http.begin(WEBHOOK_URL);
  String msg = "{\"content\": \"Please open this URL: " + verification_uri + " and enter ```" + user_code + "``` as the code.\"}";
  http.addHeader("Content-Type", "application/json");
  http.POST(msg);
  http.end();
}

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.show();

  setLeds(3);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n wifi Connected!");
  setLeds(0);

  if (refresh_token == "") {
    startDeviceAuth();
  } else {
    refreshAccessToken();
  }

  runner.init(); 
  runner.addTask(statusCheckTask);
  runner.addTask(blinkTask);
  statusCheckTask.enable();
}

void loop() {
  runner.execute();  
}

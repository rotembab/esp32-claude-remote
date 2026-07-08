/*
 * Claude Remote — ESP32-WROOM-32 smoke test (no screen/parts needed).
 *
 * Turns the Arduino Serial Monitor into a live Claude terminal:
 *   ESP32 --WiFi/WebSocket--> bridge.py --> Claude, streamed back to Serial.
 *
 * Type in the Serial Monitor (set line ending to "Newline", 115200 baud):
 *   <any text>   send it to Claude as a prompt
 *   /mode        cycle permission mode (default -> acceptEdits -> plan)  [Shift+Tab]
 *   /new         start a new session
 *   /stop        interrupt Claude mid-response
 *   /y  /n       approve / deny a pending tool-permission request
 *
 * Libraries (install via Arduino IDE Library Manager):
 *   - "WebSockets" by Markus Sattler
 *   - "ArduinoJson" by Benoit Blanchon (v7)
 * Board: "ESP32 Dev Module" (esp32 package by Espressif).
 */

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "config.h"

WebSocketsClient ws;

String serialLine;          // accumulates typed input until newline
String pendingPermId;       // id of a permission request awaiting /y or /n
bool   wsConnected = false;

// ---- sending helpers --------------------------------------------------------
void sendJson(const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  ws.sendTXT(out);
}

void sendPrompt(const String& text) {
  JsonDocument doc;
  doc["type"] = "prompt";
  doc["text"] = text;
  sendJson(doc);
  Serial.println("\n> " + text);
}

void sendSimple(const char* type) {
  JsonDocument doc;
  doc["type"] = type;
  sendJson(doc);
}

void sendPermission(bool allow) {
  if (pendingPermId.isEmpty()) {
    Serial.println("[i] no pending permission request");
    return;
  }
  JsonDocument doc;
  doc["type"]  = "permission";
  doc["id"]    = pendingPermId;
  doc["allow"] = allow;
  sendJson(doc);
  Serial.printf("[permission] %s -> %s\n", pendingPermId.c_str(), allow ? "ALLOW" : "DENY");
  pendingPermId = "";
}

// ---- incoming bridge -> device messages ------------------------------------
void handleMessage(uint8_t* payload, size_t length) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;

  const char* type = doc["type"] | "";

  if (!strcmp(type, "status")) {
    Serial.printf("\n[status] %s  (mode: %s)\n",
                  (const char*)(doc["state"] | "?"),
                  (const char*)(doc["mode"]  | "?"));
  } else if (!strcmp(type, "text")) {
    Serial.print((const char*)(doc["delta"] | ""));      // stream inline
  } else if (!strcmp(type, "tool_use")) {
    Serial.printf("\n[tool] %s\n", (const char*)(doc["name"] | "?"));
  } else if (!strcmp(type, "permission_request")) {
    pendingPermId = String((const char*)(doc["id"] | ""));
    Serial.printf("\n[PERMISSION] tool: %s  ->  reply /y (allow) or /n (deny)\n",
                  (const char*)(doc["tool"] | "?"));
  } else if (!strcmp(type, "result")) {
    Serial.println("\n[done]");
  } else if (!strcmp(type, "transcript")) {
    Serial.printf("\n[voice] %s\n", (const char*)(doc["text"] | ""));
  } else if (!strcmp(type, "error")) {
    Serial.printf("\n[error] %s\n", (const char*)(doc["message"] | ""));
  }
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsConnected = true;
      Serial.printf("\n[ws] connected to ws://%s:%d\n", BRIDGE_HOST, BRIDGE_PORT);
      Serial.println("Type a prompt and press Enter. Commands: /mode /new /stop /y /n");
      break;
    case WStype_DISCONNECTED:
      wsConnected = false;
      Serial.println("\n[ws] disconnected (will retry)…");
      break;
    case WStype_TEXT:
      handleMessage(payload, length);
      break;
    case WStype_ERROR:
      Serial.println("\n[ws] error");
      break;
    default:
      break;
  }
}

// ---- serial input (your keyboard = the device's buttons for now) -----------
void processLine(String line) {
  line.trim();
  if (line.isEmpty()) return;

  if (line == "/mode")      { sendSimple("cycle_mode"); }
  else if (line == "/new")  { sendSimple("new_session"); Serial.println("[i] new session"); }
  else if (line == "/stop") { sendSimple("interrupt");   Serial.println("[i] stop"); }
  else if (line == "/y")    { sendPermission(true);  }
  else if (line == "/n")    { sendPermission(false); }
  else                      { sendPrompt(line); }
}

void pollSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialLine.length()) { processLine(serialLine); serialLine = ""; }
    } else {
      serialLine += c;
      if (serialLine.length() > 512) serialLine = "";  // guard
    }
  }
}

// ---- setup / loop ----------------------------------------------------------
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    uint8_t reason = info.wifi_sta_disconnected.reason;
    // Common: 201 NO_AP_FOUND (wrong SSID / 5GHz / out of range),
    //         15 4WAY_HANDSHAKE_TIMEOUT & 2 AUTH_EXPIRE (wrong password).
    Serial.printf("\n[wifi] disconnected, reason=%u %s\n", reason,
                  reason == 201 ? "(NO_AP_FOUND: wrong SSID or 5GHz network)" :
                  (reason == 15 || reason == 2) ? "(likely wrong password)" : "");
  }
}

void scanWiFi() {
  Serial.println("[wifi] scanning for 2.4GHz networks the ESP32 can see…");
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  if (n <= 0) { Serial.println("[wifi]   (none found)"); return; }
  for (int i = 0; i < n; i++) {
    Serial.printf("[wifi]   %2d) \"%s\"  (rssi %d, ch %d, %s)\n", i + 1,
                  WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured");
  }
  WiFi.scanDelete();
}

void connectWiFi() {
  Serial.printf("[wifi] connecting to \"%s\"", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(400);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[wifi] connected, IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[wifi] FAILED — check WIFI_SSID/WIFI_PASS in config.h");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Claude Remote — ESP32 smoke test ===");
  WiFi.onEvent(onWiFiEvent);
  scanWiFi();
  connectWiFi();
  ws.begin(BRIDGE_HOST, BRIDGE_PORT, "/");
  ws.onEvent(webSocketEvent);
  ws.setReconnectInterval(3000);
}

void loop() {
  ws.loop();
  pollSerial();
  static uint32_t lastTry = 0;
  if (WiFi.status() != WL_CONNECTED && millis() - lastTry > 10000) {
    lastTry = millis();
    connectWiFi();                        // retry dropped WiFi, backed off
  }
}

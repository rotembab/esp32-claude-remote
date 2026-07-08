/*
 * Claude Remote — ESP32-WROOM-32 firmware.
 *
 *   ESP32 --WiFi/WebSocket--> bridge.py --> Claude, streamed back.
 * Claude's output renders on a 2.4" ILI9341 (320x240) screen, and you can
 * still drive it from the Serial Monitor until buttons/mic are wired.
 *
 * Serial input (115200 baud, "Newline"):
 *   <any text>   send it to Claude as a prompt
 *   /mode        cycle permission mode (default -> acceptEdits -> plan)  [Shift+Tab]
 *   /new         start a new session
 *   /stop        interrupt Claude mid-response
 *   /y  /n       approve / deny a pending tool-permission request
 *
 * Libraries (arduino-cli lib install / Library Manager):
 *   - "WebSockets" by Markus Sattler
 *   - "ArduinoJson" by Benoit Blanchon (v7)
 *   - "Adafruit ILI9341" + "Adafruit GFX Library"
 * Board: "ESP32 Dev Module". Display wiring: see firmware/PINMAP.md.
 */

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "config.h"

// ---- display (ILI9341 on VSPI) — pins from firmware/PINMAP.md --------------
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  17
#define TFT_BL   4     // backlight
#define TFT_SCLK 18
#define TFT_MISO 19
#define TFT_MOSI 23

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

WebSocketsClient ws;

String serialLine;          // accumulates typed input until newline
String pendingPermId;       // id of a permission request awaiting /y or /n
bool   wsConnected = false;

// ---- on-screen state -------------------------------------------------------
String g_body  = "";        // accumulated assistant text for the current turn
String g_state = "boot";    // status state
String g_mode  = "default"; // permission mode
String g_perm  = "";        // pending-permission tool name ("" = none)
int    g_bootY = 30;        // y-cursor for boot messages

// Header bar: color-coded status + mode, or a permission banner.
void drawHeader() {
  uint16_t c;
  if (g_perm.length())                          c = ILI9341_ORANGE;
  else if (g_state == "ready" || g_state == "done") c = ILI9341_DARKGREEN;
  else if (g_state == "thinking")               c = ILI9341_OLIVE;
  else if (g_state == "running_tool")           c = ILI9341_ORANGE;
  else if (g_state == "error" || g_state == "offline") c = ILI9341_MAROON;
  else                                          c = ILI9341_NAVY;
  tft.fillRect(0, 0, 320, 22, c);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(4, 3);
  if (g_perm.length()) { tft.print("Allow "); tft.print(g_perm); tft.print("? y/n"); }
  else                 { tft.print(g_state); tft.print(" ["); tft.print(g_mode); tft.print("]"); }
}

// Body: word-wrap g_body and show the last lines that fit (auto-scroll).
void drawBody() {
  const int bodyY = 26, lineH = 18, cols = 26, rows = 11;
  tft.fillRect(0, 24, 320, 216, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);

  String lines[rows];
  int count = 0;
  auto push = [&](const String &s) {
    if (count < rows) lines[count++] = s;
    else { for (int i = 1; i < rows; i++) lines[i - 1] = lines[i]; lines[rows - 1] = s; }
  };

  String cur = "", word = "";
  auto flush = [&]() { push(cur); cur = ""; };
  auto commitWord = [&]() {
    if (word.length() == 0) return;
    while ((int)word.length() > cols) {          // hard-split overly long words
      if (cur.length()) flush();
      push(word.substring(0, cols));
      word = word.substring(cols);
    }
    if (cur.length() == 0)                                cur = word;
    else if ((int)(cur.length() + 1 + word.length()) <= cols) cur += " " + word;
    else { flush(); cur = word; }
    word = "";
  };

  for (unsigned int i = 0; i < g_body.length(); i++) {
    char ch = g_body[i];
    if (ch == '\n')      { commitWord(); flush(); }
    else if (ch == ' ')  { commitWord(); }
    else if (ch == '\r') { /* ignore */ }
    else                 { word += ch; }
  }
  commitWord();
  if (cur.length()) flush();

  for (int i = 0; i < count; i++) {
    tft.setCursor(4, bodyY + i * lineH);
    tft.print(lines[i]);
  }
}

void displayBegin() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
  tft.begin();
  tft.setRotation(1);                 // landscape, 320x240
  tft.fillScreen(ILI9341_BLACK);
  drawHeader();
}

// Boot progress lines while WiFi/WS come up.
void displayBoot(const String &msg) {
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(4, g_bootY);
  tft.print(msg);
  g_bootY += 18;
  if (g_bootY > 220) g_bootY = 30;
}

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
  g_body = "> " + text + "\n";   // start a fresh turn on screen
  drawBody();
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
    g_state = String((const char*)(doc["state"] | "?"));
    g_mode  = String((const char*)(doc["mode"]  | g_mode.c_str()));
    Serial.printf("\n[status] %s  (mode: %s)\n", g_state.c_str(), g_mode.c_str());
    drawHeader();
  } else if (!strcmp(type, "text")) {
    const char* d = doc["delta"] | "";
    Serial.print(d);                                     // stream inline
    g_body += d;
    drawBody();
  } else if (!strcmp(type, "tool_use")) {
    const char* name = doc["name"] | "?";
    Serial.printf("\n[tool] %s\n", name);
    g_body += "\n[tool: "; g_body += name; g_body += "]\n";
    drawBody();
  } else if (!strcmp(type, "permission_request")) {
    pendingPermId = String((const char*)(doc["id"] | ""));
    g_perm = String((const char*)(doc["tool"] | "?"));
    Serial.printf("\n[PERMISSION] tool: %s  ->  reply /y (allow) or /n (deny)\n", g_perm.c_str());
    drawHeader();
  } else if (!strcmp(type, "result")) {
    Serial.println("\n[done]");
    g_perm = "";
    drawHeader();
  } else if (!strcmp(type, "transcript")) {
    Serial.printf("\n[voice] %s\n", (const char*)(doc["text"] | ""));
  } else if (!strcmp(type, "error")) {
    const char* m = doc["message"] | "";
    Serial.printf("\n[error] %s\n", m);
    g_body += "\n[error] "; g_body += m;
    drawBody();
  }
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsConnected = true;
      Serial.printf("\n[ws] connected to ws://%s:%d\n", BRIDGE_HOST, BRIDGE_PORT);
      Serial.println("Type a prompt and press Enter. Commands: /mode /new /stop /y /n");
      g_state = "ready";
      g_body  = "Connected.\nType a prompt on Serial\n(buttons + mic coming soon).";
      drawHeader();
      drawBody();
      break;
    case WStype_DISCONNECTED:
      wsConnected = false;
      Serial.println("\n[ws] disconnected (will retry)…");
      g_state = "offline";
      drawHeader();
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
    displayBoot("WiFi ok " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[wifi] FAILED — check WIFI_SSID/WIFI_PASS in config.h");
    displayBoot("WiFi FAILED - check config.h");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Claude Remote — ESP32 ===");
  displayBegin();
  displayBoot("Claude Remote");
  displayBoot("WiFi: " WIFI_SSID);
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

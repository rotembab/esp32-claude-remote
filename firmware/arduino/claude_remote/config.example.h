#pragma once
// Copy this file to `config.h` (same folder) and fill in your values.
// config.h is gitignored so your WiFi password never gets pushed.

#define WIFI_SSID   "YOUR_WIFI_NAME"
#define WIFI_PASS   "YOUR_WIFI_PASSWORD"

// The PC running bridge.py. Detected on this machine:
//   192.168.1.100  (Ethernet)
//   192.168.1.112  (Wi-Fi)
// Either works since the ESP32 joins the same 192.168.1.x network.
#define BRIDGE_HOST "192.168.1.100"
#define BRIDGE_PORT 8765

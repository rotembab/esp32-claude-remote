# Firmware — Arduino IDE smoke test

Get the ESP32 talking to a live Claude session over Serial — **no screen or other
parts required**. Your board is on **COM3** (USB-SERIAL CH340).

## 1. One-time Arduino IDE setup

1. Install the **Arduino IDE** (2.x) from arduino.cc.
2. Add the ESP32 boards:
   - **File → Preferences → Additional Boards Manager URLs**, add:
     `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - **Tools → Board → Boards Manager**, search **esp32**, install *"esp32 by Espressif Systems"*.
3. Install two libraries — **Tools → Manage Libraries**:
   - **WebSockets** by Markus Sattler
   - **ArduinoJson** by Benoit Blanchon

## 2. Configure

1. Open `firmware/arduino/claude_remote/claude_remote.ino` in the Arduino IDE.
2. Edit **`config.h`** (in the same folder) — set `WIFI_SSID` and `WIFI_PASS`.
   `BRIDGE_HOST` is already set to this PC's IP (`192.168.1.100`).

## 3. Start the bridge (on this PC)

```powershell
cd "C:\Users\rotem\Documents\My Projects\claude-remote\bridge"
python -m venv .venv ; .\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
copy .env.example .env      # then edit .env, add ANTHROPIC_API_KEY
python bridge.py            # should print: bridge listening on ws://0.0.0.0:8765
```

## 4. Flash the board

- **Tools → Board** → *ESP32 Dev Module*
- **Tools → Port** → *COM3*
- Click **Upload** (→). If it stalls at "Connecting….", hold the board's **BOOT**
  button until upload starts.

## 5. Use it

Open **Tools → Serial Monitor**, set **115200 baud** and line ending **Newline**.
You should see WiFi connect, then `[ws] connected`. Now type:

| You type | Effect |
|----------|--------|
| any text | send as a prompt to Claude (reply streams back) |
| `/mode`  | cycle permission mode (default → acceptEdits → plan) |
| `/new`   | new session |
| `/stop`  | interrupt the current response |
| `/y` `/n`| approve / deny a tool-permission request |

That's the entire pipeline working. Everything after this — the TFT screen, buttons,
and push-to-talk mic — just replaces "type in Serial Monitor" with real hardware I/O.

## Troubleshooting

- **Upload fails / "Connecting…":** hold **BOOT** during upload; try a different USB cable (must be data).
- **WiFi FAILED:** re-check `config.h`; the ESP32 only does 2.4 GHz WiFi (not 5 GHz).
- **WS won't connect:** make sure `bridge.py` is running and Windows Firewall allows
  Python on port 8765 (accept the prompt the first time, or allow it for Private networks).

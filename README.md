# ESP32 Claude Remote

A pocket handheld that **remotely controls a real Claude Code session** running on
your computer. A tiny screen shows Claude's streamed output; physical buttons drive
the session (send, stop, new, and a **Mode** button that cycles permission modes just
like <kbd>Shift</kbd>+<kbd>Tab</kbd> in Claude Code); and **push-to-talk** lets you
dictate prompts with your voice.

```
   ESP32 handheld                    Your PC/Mac (bridge)                 Cloud
┌──────────────────┐   WiFi/WS    ┌────────────────────────────┐      ┌──────────┐
│ screen: streamed │ ◄──────────► │ bridge.py:                 │ ───► │ Claude   │
│ Claude output    │  JSON + PCM  │  • Claude Agent SDK        │      │ API      │
│ buttons + mode   │              │  • WebSocket server        │      └──────────┘
│ mic: push-to-talk│  audio ─────►│  • Whisper speech-to-text  │
└──────────────────┘              └────────────────────────────┘
```

The ESP32 is a **thin client** — it can't run Claude itself. All the intelligence
lives in `bridge/bridge.py`, which the ESP32 talks to over your local WiFi.

## Repo layout

| Path | What |
|------|------|
| [`bridge/`](bridge/) | Python bridge server (run this on your PC) |
| [`bridge/bridge.py`](bridge/bridge.py) | The WebSocket ↔ Claude Agent SDK server |
| [`bridge/test_client.html`](bridge/test_client.html) | Browser client to drive the bridge before hardware exists |
| [`hardware/BOM.md`](hardware/BOM.md) | Full AliExpress parts list |
| [`firmware/`](firmware/) | ESP32 firmware (coming next) |

## Quick start — bridge (no hardware needed yet)

```bash
cd bridge
python -m venv .venv && . .venv/Scripts/activate   # Windows
pip install -r requirements.txt
cp .env.example .env        # then edit .env and add ANTHROPIC_API_KEY
python bridge.py
```

Then open `bridge/test_client.html` in a browser, connect to `ws://localhost:8765`,
and start talking to a real Claude session. Use the **⇧⇥ Mode** button to cycle
`default → acceptEdits → plan`.

## Permission modes (the Mode button)

| Mode | Behavior |
|------|----------|
| **default** | Every tool use asks the remote for approval (a permission prompt on the screen). |
| **acceptEdits** | File edits + reads auto-approved; other tools still prompt. |
| **plan** | Read-only tools allowed; Claude plans without making changes. |

These are enforced in the bridge's `can_use_tool` callback, so they work identically
from the browser client and the ESP32.

## Status / roadmap

- [x] Bridge: WebSocket server + Claude Agent SDK session (streaming, interrupt, modes)
- [x] Browser test client + wire protocol
- [x] Push-to-talk speech-to-text path (Whisper) on the bridge
- [x] Hardware BOM
- [ ] ESP32 firmware: WiFi + WebSocket + display rendering
- [ ] ESP32 firmware: buttons + mode cycling
- [ ] ESP32 firmware: I2S mic capture + audio streaming
- [ ] Enclosure

## Notes

- Requires an `ANTHROPIC_API_KEY` (the Agent SDK bundles the Claude Code CLI — no
  Node.js install needed).
- The bridge binds `0.0.0.0` so the ESP32 can reach it over your LAN; point the
  firmware at your PC's local IP (e.g. `ws://192.168.1.50:8765`).

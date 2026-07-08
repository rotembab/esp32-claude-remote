# Bill of Materials — ESP32 Claude Remote

A complete AliExpress shopping list to build the handheld. It's tiered:

- **Tier 1 — Core (must-have):** the minimum to get text + buttons working.
- **Tier 2 — Push-to-talk:** the mic for voice input.
- **Tier 3 — Portable/power:** battery so it's not tethered to USB.
- **Tier 4 — Enclosure & polish:** case, knob, nice buttons.
- **Tier 5 — Optional/future:** speaker for Claude talking back, haptics, etc.

Prices are rough AliExpress single-unit USD (June 2026); buying multipacks is cheaper.
You already have the **ESP32 board**, so that line is marked *(have)*.

---

## Tier 1 — Core

| # | Part | Qty | AliExpress search term | ~$ | Notes |
|---|------|-----|------------------------|----|-------|
| 1 | ESP32-S3 dev board *(have)* | 1 | `ESP32-S3 DevKitC N16R8` | 7 | S3 recommended for audio + PSRAM. Any ESP32 works for text-only. |
| 2 | **Color TFT display — ST7789 1.9" 170×320 IPS SPI** | 1 | `ST7789 1.9 inch IPS 170x320 SPI` | 6 | Best readability. Alternatives below. |
| 3 | Tactile push buttons 6×6×6mm (100 pc bag) | 1 | `6x6x6 tactile push button through hole` | 2 | We need ~7; buy a bag. |
| 4 | Breadboard 830 points | 1 | `830 point breadboard MB-102` | 3 | For prototyping before soldering. |
| 5 | Dupont jumper wires (M-M, M-F, F-F, 40 each) | 1 set | `dupont jumper wire 120pcs` | 3 | |
| 6 | Male header pins 2.54mm (40-pin strips, 10 pc) | 1 | `2.54mm male header strip` | 2 | To solder onto the display/modules. |
| 7 | USB data cable for your board (USB-C or micro) | 1 | `USB-C data cable` | 2 | Must be **data**, not charge-only, for flashing. |

**Display alternatives for line 2 (pick one):**
- *Cheapest / mono:* `SSD1306 0.96 OLED I2C` (~$2) — only 128×64, ~4 short lines of text. Uses just 2 pins (I2C).
- *Bigger color:* `ST7789 2.0 inch IPS 240x320 SPI` (~$8) — more text on screen.
- *All-in-one board* (if you didn't have a board): `LilyGO T-Display-S3` (~$15) bundles ESP32-S3 + 1.9" ST7789 + 2 buttons + battery circuit.

---

## Tier 2 — Push-to-talk (voice input)

| # | Part | Qty | AliExpress search term | ~$ | Notes |
|---|------|-----|------------------------|----|-------|
| 8 | **INMP441 I2S MEMS microphone module** | 1 | `INMP441 I2S microphone module` | 3 | Digital I2S mic — clean audio, 3 signal pins. Recommended. |
| 9 | (Alt) MAX9814 analog mic w/ AGC | 1 | `MAX9814 microphone module` | 3 | Only if you can't use I2S; needs an ADC pin. INMP441 is better. |

---

## Tier 3 — Portable / power

| # | Part | Qty | AliExpress search term | ~$ | Notes |
|---|------|-----|------------------------|----|-------|
| 10 | LiPo battery 3.7V ~1200mAh w/ JST-PH | 1 | `3.7V 1200mAh lipo battery JST` | 6 | Batteries sometimes ship slowly/restricted — order early. 103450 size fits nicely. |
| 11 | TP4056 USB-C charging module **with protection** | 1 (5-pk) | `TP4056 USB-C charging module protection` | 2 | Get the version with the extra protection IC (DW01). |
| 12 | Slide switch SPDT (on/off), 10 pc | 1 | `SS12D00 slide switch` | 1.5 | Cuts battery power. |
| 13 | (Optional) MT3608 boost converter | 1 | `MT3608 boost converter` | 1 | Only if a module needs a stable 5V from the LiPo. Usually not needed. |

> **Power note:** Most ESP32 dev boards have an onboard regulator. Feed the LiPo (3.7–4.2V) into the board's **5V/VIN** pin only if its regulator accepts down to ~3.5V; otherwise wire the LiPo through the TP4056 and into 3V3 **only if** your board lets you bypass the regulator. When in doubt, tell me your exact board and I'll give the exact wiring.

---

## Tier 4 — Enclosure & controls polish

| # | Part | Qty | AliExpress search term | ~$ | Notes |
|---|------|-----|------------------------|----|-------|
| 14 | EC11 rotary encoder + knob | 1 | `EC11 rotary encoder push knob` | 1.5 | Great for scrolling long output + click-to-select. Replaces the Up/Down/OK buttons (saves pins). |
| 15 | 12mm tactile buttons **with colored caps** | 1 set | `12mm tactile button with cap` | 3 | Nicer feel for the fixed-function buttons (Send / Mode / New / Stop / PTT). |
| 16 | Project box / enclosure ~100×60×25mm | 1 | `plastic project box 100x60x25` | 4 | Or 3D-print one (I can generate an STL later). |
| 17 | Perfboard / protoboard set | 1 | `prototype PCB board double sided set` | 3 | For the soldered permanent build. |
| 18 | Resistor assortment kit | 1 | `1/4W resistor kit 600pcs` | 3 | 10k for optional pull-ups; also enables the "analog button ladder" trick to put many buttons on one pin. |

---

## Tier 5 — Optional / future

| # | Part | Qty | AliExpress search term | ~$ | Notes |
|---|------|-----|------------------------|----|-------|
| 19 | MAX98357A I2S amplifier | 1 | `MAX98357A I2S amplifier module` | 3 | If you later want Claude to **speak** replies (TTS on the bridge → audio to speaker). |
| 20 | Small speaker 8Ω 1–2W | 1 | `8 ohm 2W speaker 40mm` | 2 | Pairs with #19. |
| 21 | Coin vibration motor + driver | 1 | `coin vibration motor 1027` | 1.5 | Haptic buzz on "response ready". Needs a transistor (2N2222) to drive. |
| 22 | WS2812 single RGB LED | 1 | `WS2812B module` | 1 | Status light (thinking / ready / error). |
| 23 | Soldering iron + solder *(if you don't own one)* | 1 | `soldering iron kit 60W` | 12 | Needed to attach headers to modules. |

---

## Button layout (7 controls) — matches the firmware/protocol

| Control | Purpose | In protocol |
|---------|---------|-------------|
| **Push-to-Talk** (hold) | Record mic → transcribe → fill prompt | `audio_start` / audio / `audio_end` |
| **Send / OK** | Confirm prompt, or approve a permission | `prompt` / `permission allow` |
| **Mode** (Shift+Tab) | Cycle permission mode: *default → auto-accept edits → plan* | `set_mode` |
| **New** | Start a fresh session | `new_session` |
| **Stop** | Interrupt Claude mid-response | `interrupt` |
| **Scroll ↑** / **Scroll ↓** | Page through long output | client-side |

> Using the **EC11 rotary encoder (#14)** replaces Scroll ↑ / ↓ / OK with one part
> (rotate = scroll, press = OK). That drops you from 7 buttons to 4 buttons + 1 encoder,
> and frees GPIO pins.

---

## Pin budget sanity check (bare ESP32-S3)

| Block | Pins |
|-------|------|
| ST7789 TFT (SPI): SCLK, MOSI, CS, DC, RST, BLK | 6 |
| INMP441 mic (I2S): BCLK, WS, SD | 3 |
| Buttons: 5 fixed + encoder (A/B/SW) | ~8 |
| **Total** | **~17** |

An ESP32-S3 has plenty of GPIO for this. If you go with a pin-limited board or add
more buttons, the **analog button ladder** (all buttons → one ADC pin via resistors from
kit #18) or a **PCF8574 I2C I/O expander** (`PCF8574 I2C module`, ~$1) collapses many
buttons onto few pins. Tell me your board and I'll finalize the exact pin map.

---

## Quick-buy summary (recommended full build)

Core + voice + battery + polish, assuming you have the board and a soldering iron:

2, 3, 5, 6, 7 (core) · 8 (mic) · 10, 11, 12 (power) · 14, 15, 16, 17, 18 (polish)

Rough total: **~$45–55** shipped, most of it the battery + enclosure + display.

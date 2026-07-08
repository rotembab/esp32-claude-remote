# Pin map — ESP32-WROOM-32 (38-pin DevKit, USB-C)

Wiring for the recommended build: **ST7789 color TFT + INMP441 I2S mic + rotary
encoder + 4 buttons**. Chosen to avoid the WROOM-32 gotchas:

- GPIO **6–11** are wired to the flash — never use them.
- GPIO **34, 35, 36, 39** are **input-only** and have **no internal pull-up**.
- Strapping pins (0, 2, 5, 12, 15) are used here only as **outputs** or left alone.
- ADC2 pins can't do analog reads while WiFi is on — we don't use analog buttons, so fine.

## Display — SHCHV 2.4" RPi LCD, ILI9341 320×240 (SPI / VSPI)

This is an SPI ILI9341 panel (Waveshare 2.4" RPi LCD (A) clone) with XPT2046 resistive
touch, 3.3 V logic. It plugs onto a Pi 40-pin header; for the ESP32 you run jumpers from
the relevant **header positions** to ESP32 GPIOs. The standard mapping (⚠ **confirm
against your board's silkscreen** — clones vary):

| Signal | RPi header pin (BCM) | ESP32 GPIO |
|--------|----------------------|-----------|
| SCLK   | 23 (GPIO11) | 18 |
| MOSI (SDI) | 19 (GPIO10) | 23 |
| LCD CS | 24 (CE0/GPIO8) | 5 |
| LCD DC / RS | 18 (GPIO24) | 16 |
| RESET  | 22 (GPIO25) | 17 |
| Backlight (LED/BL) | 12 (GPIO18) | 4 *(or tie to 3V3 for always-on)* |
| 3V3    | 1 | 3V3 |
| GND    | 6 | GND |

TFT_eSPI driver: **ILI9341_DRIVER**, 320×240.

### Touchscreen (optional — wire later if you want on-screen input)

| Signal | RPi header pin (BCM) | ESP32 GPIO |
|--------|----------------------|-----------|
| MISO (shared SPI) | 21 (GPIO9) | 19 *(frees only if PTT moved)* |
| Touch CS (T_CS)   | 26 (CE1/GPIO7) | 15 |
| Touch IRQ (T_IRQ) | 11 (GPIO17) | 2 |

> Touch shares SCLK/MOSI with the display and adds MISO. Note MISO on **19** would
> collide with the Push-to-talk button below — if you enable touch, I'll move PTT to a
> free pin. The first firmware is **display-only** (no MISO/touch needed), so there's no
> conflict yet.

### Alternative: ST7789 1.9" TFT

If you use an ST7789 instead: same SPI pins (SCLK 18, MOSI 23, CS 5, DC 16, RST 17,
BLK 4), driver `ST7789_DRIVER`, and no touch.

## Microphone — INMP441 (I2S0)

| INMP441 pin | ESP32 GPIO |
|-------------|-----------|
| SCK (BCLK)  | 26 |
| WS (LRCLK)  | 25 |
| SD (data)   | 33 |
| L/R         | GND (left channel) |
| VDD | 3V3 |
| GND | GND |

## Controls (all active-low, `INPUT_PULLUP`)

| Control | ESP32 GPIO | Notes |
|---------|-----------|-------|
| Rotary encoder A | 21 | scroll |
| Rotary encoder B | 22 | scroll |
| Encoder push (SW) = **Send / OK / Approve** | 32 | 32 supports internal pull-up |
| **Mode** (Shift+Tab: default→acceptEdits→plan) | 27 | |
| **New session** | 14 | |
| **Stop** (interrupt) | 13 | |
| **Push-to-talk** (hold) | 19 | |

> This scheme uses the encoder click as Send/OK, so you only need **4 buttons + 1
> encoder** = 7 GPIOs. If you'd rather have discrete Up/Down/Send buttons instead of
> the encoder, say so and I'll remap.

## Power (battery)

The WROOM-32 DevKit's onboard AMS1117 regulator needs ≥ ~4.7 V in, so a 3.7 V LiPo
must be **boosted to 5 V** first:

```
LiPo 3.7V ──> TP4056 (USB-C charge/protect) ──> slide switch ──> MT3608 (boost to 5V) ──> board 5V pin
```

Set the MT3608 output to 5.0 V with its trimpot **before** connecting the board.
Do **not** power from battery and USB at the same time.

## GPIO usage summary

Used: 4, 5, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 — 16 pins.
Free for expansion: 0, 2, 12, 15 (strapping — outputs only), 34/35/36/39 (input-only),
plus 3V3/5V/GND/EN.

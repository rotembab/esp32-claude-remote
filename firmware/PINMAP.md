# Pin map — ESP32-WROOM-32 (38-pin DevKit, USB-C)

Wiring for the recommended build: **ST7789 color TFT + INMP441 I2S mic + rotary
encoder + 4 buttons**. Chosen to avoid the WROOM-32 gotchas:

- GPIO **6–11** are wired to the flash — never use them.
- GPIO **34, 35, 36, 39** are **input-only** and have **no internal pull-up**.
- Strapping pins (0, 2, 5, 12, 15) are used here only as **outputs** or left alone.
- ADC2 pins can't do analog reads while WiFi is on — we don't use analog buttons, so fine.

## Display — ST7789 TFT (SPI / VSPI)

| TFT pin | ESP32 GPIO |
|---------|-----------|
| SCLK / SCK  | 18 |
| MOSI / SDA  | 23 |
| CS          | 5  |
| DC / RS     | 16 |
| RST         | 17 |
| BLK (backlight) | 4 |
| VCC | 3V3 |
| GND | GND |

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

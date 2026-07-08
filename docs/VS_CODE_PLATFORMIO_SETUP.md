# VS Code / PlatformIO Setup for DOBETTERLED

## 1. Install tools

1. Install Visual Studio Code.
2. In VS Code, open Extensions.
3. Install **PlatformIO IDE**.
4. Restart VS Code if prompted.

## 2. Open the project

Open the folder that contains `platformio.ini`:

```text
DOBETTERLED_PlatformIO/
```

Do not open only the `src` folder. PlatformIO needs to see `platformio.ini` at the project root.

## 3. Connect the Dig2Go

Use the same USB-C data cable. The board should appear as CH340 on COM6, based on prior tests.

## 4. Build

Click the PlatformIO checkmark icon, or use:

```bash
pio run
```

## 5. Upload

Click the PlatformIO upload arrow, or use:

```bash
pio run --target upload
```

## 6. Serial Monitor

Open PlatformIO Serial Monitor, or use:

```bash
pio device monitor --baud 115200
```

## Current hardware config

Confirmed by direct FastLED test:

- LED data: GPIO16
- Relay power: GPIO12 HIGH
- LED count: 15
- Chipset: WS2812B
- Color order: GRB

## Important debugging note

This project is still the v1.0.2 diagnostic build. The current `LEDDriver::begin()` is intentionally stripped down to reproduce the working direct FastLED test before normal architecture behavior is restored.

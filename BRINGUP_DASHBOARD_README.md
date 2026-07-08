# DOBETTERLED Bring-Up Dashboard

This is a diagnostic firmware build for the Dig2Go hardware.

It intentionally bypasses the full DOBETTERLED SystemManager architecture and gives you a Serial Monitor command menu.

## Hardware confirmed

- LED data: GPIO16
- Relay power: GPIO12, HIGH = on
- Button: GPIO0, active LOW
- LEDs: 15 WS2812B, GRB

## PlatformIO steps

1. Open this folder in VS Code.
2. Click PlatformIO Build (checkmark).
3. Click PlatformIO Upload (right arrow).
4. Click PlatformIO Monitor (plug icon).
5. Set baud to 115200 if needed.
6. Type commands into the Serial Monitor.

## Commands

- `1` = Solid red
- `2` = Solid green
- `3` = Solid blue
- `4` = Solid white
- `5` = Moving rainbow
- `6` = Chase test
- `7` = Relay off test
- `8` = Button test mode
- `9` = LEDs off, relay on
- `+` = Brightness up
- `-` = Brightness down
- `m` = Print menu again

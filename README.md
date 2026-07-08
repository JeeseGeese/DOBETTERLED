# DOBETTERLED

A modular, multi-product firmware platform for ESP32-based addressable LED
controllers, built on the Arduino framework, FastLED, and PlatformIO/VS Code. The
**QuinLED Dig2Go** is the first supported hardware target — the platform
is designed so future boards can be added by supplying a new
`ProductConfig.h`, without touching manager or effect code.

> **Status: v1.0.0 — hardware-confirmed on a real Dig2Go.** A standalone
> Arduino+FastLED isolation test confirmed the actual wiring (15 WS2812B
> LEDs, GPIO16, GRB, relay power on GPIO12), now patched into
> `ProductConfig.h`. The framework (Config, ProductConfig, Types, HAL,
> ButtonManager, LEDDriver, SettingsManager, PaletteManager,
> EffectRegistry, AnimationManager, SystemManager) plus one effect
> (`SolidEffect`) are wired end-to-end against this confirmed config.
> **No new features, effects, or polish are being added until the full
> button/brightness/factory-reset checklist is confirmed on hardware.**
> See [`CHANGELOG.md`](CHANGELOG.md) for the exact expected behavior
> checklist for this milestone, and
> [`docs/PRODUCT_SPEC.md`](docs/PRODUCT_SPEC.md) for the full behavioral
> spec beyond it.

---

## Why this exists

Most single-board Arduino LED sketches hard-code pins, timing, and button
logic together into one file, which makes them a dead end the moment you
want a second product or a serious feature. This project is built the
other way around: hardware facts, generic firmware behavior, hardware
access, and application coordination are deliberately kept in separate
layers so the same firmware base can grow into a family of products.

## Architecture

```
                 SystemManager
           (owns managers, boot/shutdown
            sequencing, button-event ->
                 action mapping)
                       |
   +-------------------+-------------------+
   |          |            |               |
ButtonManager LEDDriver  SettingsManager  SoundManager   <- Managers
   |          |            |               |             (own behavior)
   +-------------------+-------------------+
                       |
                      HAL                                <- Hal.h / Hal.cpp
           (pure GPIO / I2S access only,
            no application logic)
                       |
                   Hardware                               <- ProductConfig.h
                                                              (pin facts)
```

Config is split the same way behavior is split:

| File | Contains |
|---|---|
| `ProductConfig.h` | **Board-specific facts only**: pin numbers, LED count, relay presence, mic bus. The Dig2Go is one target defined here — not the firmware's identity. |
| `Config.h` | **Firmware-generic behavior**: timing constants, defaults, limits. Anything a *different* product would plausibly reuse (or re-tune, never re-pin) lives here. |
| `Types.h` | Shared enums and small value types (`ButtonEvent`, `AnimationId`, `PaletteId`, `AudioMode`, `RgbColor`, `EffectSettings`) used across modules without creating circular includes. |

Key rules enforced throughout:
- **Only the HAL touches GPIO/I2S directly** (one documented exception:
  `LEDDriver.cpp` will reference `ProductConfig.h`'s LED data pin directly,
  because FastLED requires it as a compile-time template argument, not a
  runtime value).
- **`Types.h` is FastLED-free.** A plain `RgbColor` struct carries color
  data everywhere outside `LEDDriver`, so no other file is structurally
  coupled to FastLED's `CRGB` type.
- **Managers own behavior; the HAL never does.** `ButtonManager` contains
  all debounce/click-count/long-press/factory-reset logic and consumes
  only `Hal::isButtonPressed()` — it has no GPIO or `ProductConfig`
  reference anywhere in it.
- **Enum values that get persisted to NVS are append-only.** `AnimationId`
  and `PaletteId` values must never be reordered once shipped, or saved
  user preferences on real devices will silently point to the wrong thing.

## Repository structure

```
DOBETTERLED_PlatformIO/
├── platformio.ini                VS Code / PlatformIO build configuration
├── README.md                     (this file)
├── CHANGELOG.md
├── include/
│   ├── Config.h                  Firmware-generic timing/defaults/limits
│   ├── ProductConfig.h           Dig2Go-specific pin map & hardware facts
│   └── Types.h                   Shared enums & value types
├── docs/
│   ├── PRODUCT_SPEC.md           Milestone 1 — behavioral source of truth
│   ├── DESIGN_LOG.md             Rationale trail for every architecture decision
│   └── ARCHITECTURE_CONTRACT.md  Ownership boundaries for every module
└── src/                          PlatformIO compiles everything here
    ├── main.cpp                   Top-level entry point — only calls begin()/update()
    ├── SystemManager.h / .cpp     Application coordinator
    ├── AnimationBase.h            Animation interface + AnimationContext
    ├── HAL/
    │   └── Hal.h / .cpp            Hardware abstraction layer
    ├── Managers/
    │   ├── LEDDriver.h / .cpp       FastLED encapsulation, pixel buffer, brightness
    │   ├── ButtonManager.h / .cpp    Button gesture state machine
    │   ├── SettingsManager.h / .cpp   NVS persistence, dirty-flag debounced commits
    │   ├── PaletteManager.h / .cpp     Built-in palettes, color interpolation
    │   ├── EffectRegistry.h / .cpp      Stateless AnimationId -> effect lookup
    │   ├── AnimationManager.h / .cpp     Effect selection & per-frame invocation
    │   └── SoundManager.h                PLACEHOLDER stub (see file header)
    ├── Effects/
    │   └── SolidEffect.h / .cpp           The one effect implemented so far
    └── Utilities/                          (empty — Milestone 4)
```

This version is converted for VS Code + PlatformIO. `src/` contains all compiled source files, while `include/` contains shared project headers. The old Arduino `.ino` wrapper has been replaced by `src/main.cpp`.

Not yet present (upcoming milestones): the real `SoundManager` (I2S
sampling, calibration, AGC, beat detection), every effect beyond
`SolidEffect`, and any fade/transition polish.

## Hardware target: QuinLED Dig2Go

- ESP32, single hardware revision (no board variants to track)
- Level-shifted WS2812B-compatible LED data output
- Built-in ICS-43434 digital I2S MEMS microphone (not analog)
- LED power relay for true off-state power cutting
- Full pinout: see `ProductConfig.h`

## Development approach

This firmware is being built incrementally, one milestone and one file at
a time, with each design decision reviewed and locked before moving on.
`docs/DESIGN_LOG.md` captures the reasoning behind each significant
decision as a running record — useful both for continuing development
and for onboarding a future contributor (or future product's
`ProductConfig`) without re-litigating settled questions.

## License

Not yet decided — add a `LICENSE` file before publishing publicly if
you intend this to be an open-source project.

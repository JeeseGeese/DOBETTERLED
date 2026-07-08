# DOBETTERLED

A modular, multi-product firmware platform for ESP32-based addressable LED
controllers, built on the Arduino framework, FastLED, and PlatformIO/VS Code. The
**QuinLED Dig2Go** is the first supported hardware target — the platform
is designed so future boards can be added by supplying a new
`ProductConfig.h`, without touching manager or effect code.

> **Status: this repo currently contains two unreconciled code paths.
> Read this before touching anything.**
>
> 1. **Active build, awaiting hardware re-verification: the
>    Engineering Console, v0.2.0-alpha, Milestone 3.** `main.cpp`
>    instantiates `EngineeringConsole`. Diagnostic access (`s` for
>    status, `m` for menu) is Serial-only and always available -- no
>    button gesture is reserved for it. `ButtonGestureEngine`
>    (Milestone 2) detects the finalized button gesture map (BRoadmap
>    v1.3, see `docs/PRODUCT_SPEC.md` Section 3) and now also returns a
>    `Gesture` value per call. `EffectEngine` (Milestone 3) wires
>    **1/2/3/4-press gestures to real Next/Previous Effect (scoped to a
>    Static/Motion/Reactive Mode category), Next Palette, and Next
>    Mode** across a 6-effect starter set (Solid, Rainbow, Confetti,
>    Sparkle, Chase, Fire) and 4 built-in palettes. Double Press + Hold
>    toggles a tracked-only Audio Reactive Overlay flag (no microphone
>    input implemented). Long Hold, 6-press, and 10-press remain
>    detection/report-only. This build compiles cleanly
>    (`pio run` — see `CHANGELOG.md`) but has **not yet been flashed to
>    or confirmed on real hardware**; `BringUpDashboard.h`/`.cpp` remain
>    unmodified as the one-line rollback target.
> 2. **Dormant full architecture tree (not currently running, not
>    hardware-confirmed):** `SystemManager`, `Hal`, `LEDDriver`,
>    `ButtonManager`, `SettingsManager`, `PaletteManager`,
>    `EffectRegistry`, `AnimationManager`, `SolidEffect`. This tree is
>    architecturally complete and matches
>    `docs/ARCHITECTURE_CONTRACT.md`, but its own hardware bring-up
>    stalled mid-diagnostic (see `CHANGELOG.md`'s v1.0.1/v1.0.2 entries)
>    and was never confirmed working before the project pivoted to the
>    Bring-Up Dashboard instead. It is **not included, linked, or
>    executed** by the current build. Do not assume it works. Its
>    button/audio/effect design has been revised on paper three times
>    since (`docs/PRODUCT_SPEC.md` Sections 3 and 10, BRoadmap v1.1 →
>    v1.2 → v1.3) but none of that is implemented against the dormant
>    tree itself — `ButtonGestureEngine` and `EffectEngine` (path 1
>    above) are separate, standalone implementations, explicitly built
>    to be extractable into `ButtonManager`/`AnimationManager` later,
>    not a merge into them now.
> 3. **Required later milestone:** reconcile these two paths -- either
>    by getting the dormant tree confirmed working on hardware and
>    retiring the standalone console, or by deliberately folding
>    console functionality into `SystemManager` once that's proven.
>    Until that milestone happens, treat path 2 as unverified and do
>    not build new features on top of it.
>
> Prior status text (kept for history, no longer current): "v1.0.0 —
> hardware-confirmed on a real Dig2Go" referred to path 2 above, before
> its bring-up stalled and the project pivoted to path 1.

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
    ├── main.cpp                   Top-level entry point — instantiates EngineeringConsole
    ├── EngineeringConsole.h / .cpp  ACTIVE, hardware-verified — Engineering Console + Developer Mode (v0.2.0-alpha)
    ├── ButtonGestureEngine.h / .cpp  ACTIVE, awaiting hardware re-verification — Milestone 2 gesture detection (hardware-confirmed) + Milestone 3 Gesture-enum return, standalone (not part of dormant ButtonManager)
    ├── EffectEngine.h / .cpp        ACTIVE, awaiting hardware re-verification — Milestone 3 effect/palette rendering, standalone (not part of dormant AnimationManager/EffectRegistry/PaletteManager)
    ├── BringUpDashboard.h / .cpp    Unmodified, unreferenced — one-line rollback target (see CHANGELOG Milestone 1)
    ├── SystemManager.h / .cpp     DORMANT — application coordinator, not hardware-confirmed (see status note above)
    ├── AnimationBase.h            DORMANT — animation interface + AnimationContext
    ├── HAL/
    │   └── Hal.h / .cpp            DORMANT — hardware abstraction layer
    ├── Managers/                   (all DORMANT — see status note above)
    │   ├── LEDDriver.h / .cpp       FastLED encapsulation, pixel buffer, brightness
    │   ├── ButtonManager.h / .cpp    Button gesture state machine
    │   ├── SettingsManager.h / .cpp   NVS persistence, dirty-flag debounced commits
    │   ├── PaletteManager.h / .cpp     Built-in palettes, color interpolation
    │   ├── EffectRegistry.h / .cpp      Stateless AnimationId -> effect lookup
    │   ├── AnimationManager.h / .cpp     Effect selection & per-frame invocation
    │   └── SoundManager.h                PLACEHOLDER stub (see file header)
    ├── Effects/                     (DORMANT)
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

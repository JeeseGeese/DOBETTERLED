# Changelog

All notable progress on this firmware platform, organized by development
milestone (see `docs/PRODUCT_SPEC.md` for the full milestone plan).

## [1.0.2] — Maximally-Inlined Diagnostic (v1.0.1 diagnostic still didn't light LEDs)

**Status: diagnostic build, not a real feature release.** The v1.0.1
pre-settings diagnostic (forcing relay/brightness/fill/show through
LEDDriver/Hal before any settings ran) still did not light the LEDs,
even though it was logically equivalent to the working isolation test.
This build removes every remaining layer of indirection so
`LEDDriver::begin()` matches the working standalone isolation sketch as
literally as possible:

- Raw `pinMode(12, OUTPUT)` / `digitalWrite(12, HIGH)` inlined directly
  in `LEDDriver.cpp` (bypassing `Hal::setLedPower()` entirely for this
  test).
- Literal pin/count values (`16`, `15`) in the `FastLED.addLeds<>()`
  call instead of `Product::` constants, to eliminate any doubt about
  constexpr template-argument resolution.
- `fill_solid(g_leds, 15, CRGB::Red)` directly, bypassing the
  `RgbColor`→`CRGB` conversion path entirely.
- `SystemManager::begin()` now calls only `Hal::begin()` and
  `m_ledDriver.begin()`, then returns — no `SettingsManager`, no
  `AnimationManager`, no `applyPowerState()`, no clear/black-frame logic
  of any kind can run afterward.
- `SystemManager::update()` is an empty no-op, matching the isolation
  sketch's empty `loop()` exactly.

If this build **still** doesn't light the strip, the remaining
explanations are no longer inside DOBETTERLED's source: a stale Arduino
IDE build cache, a different board/partition selection than the one
used for the original isolation test, incomplete file replacement when
extracting this zip over a previous one, or a physical change in the
wiring/power/strip itself since the original successful test. Re-running
the original `HWTest1_GPIO16_GRB` sketch again as a control is the next
step if so.

**Restore markers are left in both patched files** (`LEDDriver.cpp`,
`SystemManager.cpp`) documenting the exact code to bring back once a
working configuration is found.

## [1.0.1] — Temporary Boot Diagnostic (debugging LEDs not showing)

**Status: diagnostic build, not a real feature release.** Full
DOBETTERLED compiled and uploaded successfully, but LEDs did not show
solid white as expected, despite the standalone isolation test (v1.0.0
prerequisite) confirming the hardware itself works with this exact
config. Added a temporary two-part diagnostic in `SystemManager::begin()`
to isolate whether the problem is in LEDDriver/HAL or in stale
persisted settings from earlier test/flash cycles:

- **Part 1** (before `SettingsManager`/`AnimationManager` run at all):
  prints every static hardware fact (`HasPowerRelay`, `LedRelay` pin,
  `LedData` pin, `Count`), then forces relay on, brightness 80, fills
  all 15 LEDs white, calls `show()`, and holds for 2 seconds. If this
  doesn't light the strip, the bug is in `LEDDriver`/`Hal`, not settings.
- **Part 2** (immediately after `SettingsManager::begin()`): prints the
  actual loaded `PowerState`, `Brightness`, and `AnimationId` — the one
  thing static code review can't answer, since it depends on whatever
  is physically sitting in this board's NVS from prior flashes, not the
  source code itself.

Includes `ProductConfig.h` directly in `SystemManager.cpp` as a
deliberate, temporary exception to the normal Hal.cpp/LEDDriver.cpp-only
rule, solely for the diagnostic prints. **Remove this entire block (and
the temporary include) once the root cause is found and solid-white
boot is confirmed working**, then re-verify against the v1.0.0
checklist below.

## [1.0.0] — Confirmed on Real Dig2Go Hardware

**Status: hardware-confirmed baseline.** A standalone Arduino+FastLED-only
isolation test (bypassing all DOBETTERLED code) confirmed the Dig2Go's
actual wiring: 15 WS2812B LEDs, GPIO16 data, GRB color order, relay
power on GPIO12. All 15 LEDs lit solid red at brightness 80. These
confirmed values are now patched into `ProductConfig.h`, and
`LEDDriver.cpp` asserts against them at compile time so they can't
silently drift. `SystemManager::begin()` now explicitly energizes the
relay before any `FastLED.show()` call can happen, including
`LEDDriver::begin()`'s own internal clear — previously true only by
incidental call ordering, now guaranteed by construction.

### Confirmed hardware config (`ProductConfig.h`)
- LED data pin: GPIO16
- LED count: 15 (was a placeholder 60)
- Chipset: WS2812B
- Color order: GRB (now an explicit named fact, `Led::Order`, asserted
  against in `LEDDriver.cpp` the same way `Led::Chipset` already was)
- LED power relay: enabled, GPIO12

### Scope (unchanged from the prior frozen milestone)
- `Hal`, `LEDDriver`, `ButtonManager`, `SettingsManager`,
  `PaletteManager`, `EffectRegistry`, `AnimationManager`, `SystemManager`
- One effect: `SolidEffect`
- One placeholder: `SoundManager` (stub only — see its header comment)
- Top-level `DOBETTERLED_Firmware.ino`

### Expected behavior on hardware
- [ ] Dig2Go boots
- [ ] LEDs show solid white (default `EffectSettings::primaryColor`)
- [ ] Single / double / triple / quad clicks may update internal state
      (next/previous animation, next palette, cycle audio mode) even if
      not visually obvious yet — only one effect exists, so animation
      changes won't be visible; palette/audio mode changes have no
      visible effect on Solid by design
- [ ] Holding the button smoothly adjusts brightness
- [ ] A short long-press (released before the ramp threshold) toggles
      power off/on
- [ ] A very long hold (5–10s) triggers factory-reset behavior; the
      strip returns to default white at default brightness afterward
- [ ] Releasing mid-ramp does **not** also toggle power (see
      `docs/DESIGN_LOG.md` Section 6)

### Not yet implemented (unchanged from before this milestone)
- Fades, transitions, or any visual feedback beyond raw effect output
- Any effect other than Solid
- Real `SoundManager` (I2S sampling, calibration, AGC, beat detection)
- Everything else already listed as pending in earlier entries below

## [Unreleased] — Milestone 2: Framework (in progress)

### Added
- `docs/PRODUCT_SPEC.md` — Milestone 1 product specification, locked as
  the behavioral source of truth.
- `docs/DESIGN_LOG.md` — running record of architecture decisions and
  their rationale.
- `ProductConfig.h` — Dig2Go hardware identity (pin map, LED count,
  relay presence, mic bus assignment), isolated from generic firmware
  config so future hardware targets can be added without touching
  behavior code.
- `Config.h` — firmware-generic timing constants, defaults, and limits.
  Includes `FirmwareInfo` (platform name + semantic version) and
  `Timing::StartupFadeMs` / `ShutdownFadeMs` / `EffectTransitionMs` added
  ahead of Milestone 5 since the spec treats fades as required behavior.
- `Types.h` — shared enums (`ButtonEvent`, `PowerState`, `AudioMode`,
  `AnimationId`, `PaletteId`) and value types (`RgbColor`,
  `EffectSettings`), deliberately independent of FastLED.
- `Hal.h` / `Hal.cpp` — hardware abstraction layer. Implements button
  pin read, LED power relay control, and I2S microphone init/read for
  the Dig2Go's ICS-43434 digital MEMS mic. Pure hardware access only;
  no application logic.
- `ButtonManager.h` / `ButtonManager.cpp` — full button gesture state
  machine: debounce, single/double/triple/quad click counting,
  long-press phase tracking (`Start`/`Hold`/`Release`), self-contained
  brightness ramp computation, and automatic factory-reset confirmation
  with a cancelable warning window.

### Not yet implemented
- `LEDDriver` (FastLED encapsulation, fade in/out, relay-based power)
- `SettingsManager` (NVS persistence, dirty-flag debounced commits)
- `PaletteManager`
- `SoundManager` (calibration, AGC, envelope, beat detection)
- `AnimationBase` / `EffectRegistry` / `AnimationManager`
- `SystemManager` (button-event → action mapping, boot/shutdown sequencing)
- All individual effects (`Solid`, `Rainbow`, `Fire`, `Twinkle`,
  `Confetti`, `Plasma`, `Chase`, `Sparkle`, `SoundReactive`)
- Top-level `.ino` sketch

## Milestone 1: Product Specification — Complete

- Defined and locked complete button behavior, startup/shutdown
  behavior, brightness logic, palette behavior, 3-state audio mode
  behavior, settings persistence model, factory reset behavior, LED
  transition/feedback behavior, and the full timing constant table.

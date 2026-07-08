# Changelog

All notable progress on this firmware platform, organized by development
milestone (see `docs/PRODUCT_SPEC.md` for the full milestone plan).

## [0.2.0-alpha] — Milestone 3 [BRoadmap v1.3]: Effect Engine, Mode categories, Audio Overlay wiring

**Status: candidate build. Compiles cleanly (`pio run`, verified this
session), not yet flashed to or confirmed on real hardware.**

### Roadmap finalization (`docs/PRODUCT_SPEC.md`, `docs/DESIGN_LOG.md`)
- Button Roadmap bumped to **v1.3**. Added **4 Presses = Next Mode**, a
  category filter over the effect list: **Static** (Solid), **Motion**
  (Rainbow, Chase), **Reactive** (Confetti, Sparkle, Fire). 1/2-Press
  now cycles within the current category only; 4 Presses switches
  category and selects that category's first effect.
- Explicit disambiguation added: the "Reactive" **Mode category** name
  is unrelated to the **Audio Reactive Overlay** toggle -- two
  different axes that happen to share a word.
- Finalized the Audio Reactive Overlay's navigation contract (Section
  10): not a separate effect, never blocks or intercepts 1/2/3/4-press
  navigation, persists as a global on/off flag across every effect/
  palette/Mode change.
- Full rationale in `docs/DESIGN_LOG.md` Section 13.

### Added (`src/EffectEngine.h`/`.cpp`)
- New, standalone effect/palette rendering class. Renders into a
  caller-provided `CRGB` buffer only -- no `FastLED.show()`, no relay,
  no button reads, no dormant-tree (`AnimationManager`/`EffectRegistry`/
  `PaletteManager`) dependency.
- Six starter effects: `Solid` (flat palette color), `Rainbow`
  (palette-independent full-spectrum sweep), `Confetti` (fading random
  palette-colored sparks), `Sparkle` (sharper, darker-background
  flashes), `Chase` (single palette-colored pixel on black), `Fire`
  (Fire2012 heat-diffusion, reading color from the *current palette*
  instead of a fixed heat gradient so palette-cycling reskins it).
- Four built-in FastLED gradient palettes: Rainbow, Party, Ocean, Fire
  (`RainbowColors_p`/`PartyColors_p`/`OceanColors_p`/`HeatColors_p`).
  Applies to Solid/Confetti/Sparkle/Chase/Fire; Rainbow the *effect*
  intentionally ignores palette selection.
- `nextEffect()`/`previousEffect()` cycle within the current Mode
  category only; `nextMode()` switches category and selects its first
  effect; `nextPalette()` cycles all four regardless of category.
- Fire's per-pixel heat state uses a fixed `MAX_LEDS = 64` array, not
  dynamic allocation; `render()` clamps to it with a one-time Serial
  warning if a caller ever passes more (current hardware has 15).

### Changed (`src/ButtonGestureEngine.h`/`.cpp`)
- `update()` now returns a `Gesture` enum classifying what (if
  anything) was detected this call, in addition to its existing Serial
  `BUTTON: ...` prints (unchanged, hardware-verified in Milestone 2).
  Purely additive -- detection logic and every existing print line are
  unchanged.
- 4 completed presses, previously falling into the "unclassified click
  count" default case, are now a first-class `FourPress` gesture
  (`BUTTON: FOUR_PRESS`).

### Changed (`src/EngineeringConsole.h`/`.cpp`)
- New `Mode::EffectEngineMode`, entered only via gestures -- never
  reachable via a serial digit command (`1`-`9` are unchanged).
- Wires **1 Press → Next Effect**, **2 Presses → Previous Effect**,
  **3 Presses → Next Palette**, **4 Presses → Next Mode** to
  `EffectEngine`. Any of these four gestures switches into
  `Mode::EffectEngineMode`, even if a diagnostic command had most
  recently set a different mode.
- **2 Presses + Hold** toggles a new `m_audioReactiveOverlay` bool and
  Serial-reports it. Tracked only -- no microphone sampling, no
  modulation; explicitly labeled as such in the printed line.
- All other detected gestures (`LongHold`, `SixPressPowerCandidate`,
  `TenPressFactoryResetPending`, `Unclassified`) are still detected and
  Serial-reported by `ButtonGestureEngine` but deliberately not acted
  on -- quick settings, power toggle, and factory reset remain out of
  scope for this milestone.
- Serial prints the resulting effect/palette/mode name on every
  Next/Previous Effect, Next Palette, or Next Mode action.
- Additive lines added to `printMenu()` and a new "Effect Engine" /
  updated "Audio" section added to `printEngineeringStatus()`'s status
  block (`s` command). No existing line was altered; commands `1`-`9`,
  `+`, `-`, `m`, `s` are unchanged in behavior.

### Build verification
- `pio run` succeeds (espressif32 platform, esp32dev board). RAM 7.0%,
  Flash 23.6%. No warnings or errors attributable to new/changed
  project files; all warnings present are pre-existing, from the
  FastLED/I2S framework libraries, unrelated to this change.
- **Not yet flashed or hardware-tested.**

### Not touched
- `BringUpDashboard.h`/`.cpp` -- still unmodified, still the one-line
  rollback target.
- `SystemManager`, `Hal`, everything under `Managers/`, `Effects/`,
  `AnimationBase.h`, `Types.h`, `Config.h`, `ProductConfig.h` -- the
  dormant tree. `EffectEngine` and the `ButtonGestureEngine` changes
  are standalone, not edits to the dormant `AnimationManager`/
  `EffectRegistry`/`PaletteManager`/`ButtonManager`.
- Settings persistence: effect/palette/mode selection resets to
  Solid / Rainbow palette / Static mode on every reboot. No NVS writes.
- No microphone input, no `SoundManager` code. No power toggle, no
  factory-reset erase.

## [0.2.0-alpha] — Milestone 2 [BRoadmap v1.2]: Button Gesture Engine

**Status: candidate build, awaiting hardware re-verification.**

### Roadmap finalization (`docs/PRODUCT_SPEC.md`, `docs/DESIGN_LOG.md`)
- Button Roadmap bumped to **v1.2**: finalized the gesture map used by
  this milestone. Continuous-hold Brightness Ramp and Very Long Press
  (5-10s) Factory Reset (from v1.1) are removed. Replaced with:
  - 6 presses → Power On/Off candidate
  - 10 presses → Factory Reset candidate (pending-only; a real
    confirmation-hold mechanism is explicitly deferred to a later
    milestone, and Milestone 2 never erases settings)
  - Long Hold (2-3s) repurposed from "toggle on/off" to Quick Settings
    Menu candidate
  - 1/2/3 presses and 2 Presses + Hold carried over unchanged from v1.1
    (Next/Previous Effect, Next Palette, Audio Reactive Overlay toggle)
- Flagged an explicit open item: brightness adjustment has no gesture
  as of v1.2 (its old trigger, the hold/ramp gesture, no longer exists).
  Not resolved here — left for a future BRoadmap revision.
- Full rationale in `docs/DESIGN_LOG.md` Section 12.

### Added (`src/ButtonGestureEngine.h`/`.cpp`)
- New, standalone gesture-detection class. Does not call `pinMode`/
  `digitalRead` itself -- `EngineeringConsole` passes in the raw button
  reading each loop; the engine is pure debounce/click-count/hold-timing
  logic with zero GPIO, FastLED, or dormant-tree coupling. Explicitly
  designed to be extractable into `ButtonManager` later without a
  rewrite.
- Detects and prints via Serial: `SINGLE_PRESS`, `DOUBLE_PRESS`,
  `TRIPLE_PRESS`, `DOUBLE_PRESS_HOLD`, `LONG_HOLD`,
  `SIX_PRESS_POWER_CANDIDATE`, `TEN_PRESS_FACTORY_RESET_PENDING` (all
  prefixed `BUTTON: `). Any click count or hold pattern outside this
  set (4, 5, 7, 8, 9, 11+ presses; a hold on the 3rd+ press of a
  sequence) is reported on a separate, deliberately differently-worded
  line (`Button: unclassified ...`) so nothing is silently dropped, but
  nothing is confused with a real, defined gesture either.
- **No gesture is wired to any action.** No effect switching, no
  palette switching, no power toggle, no audio overlay, no settings
  erase. Detection and Serial reporting only, per the milestone's goal.

### Changed (`src/EngineeringConsole.h`/`.cpp`)
- Added a `ButtonGestureEngine` member. `begin()` calls
  `m_gestureEngine.begin()` (state reset only, no hardware access).
  `update()` forwards the same raw button reading Button Test mode
  already uses into `m_gestureEngine.update(...)` -- an additional
  read of an already-read pin, not a new GPIO access pattern.
- One informational line added to `printMenu()` and one to
  `printEngineeringStatus()`'s Button section, noting the gesture
  engine is active. No existing line was altered.
- Commands `1`-`9`, `+`, `-`, `m`, `s` are unchanged in behavior.

### Not touched
- `BringUpDashboard.h`/`.cpp` -- still unmodified, still the one-line
  rollback target.
- `SystemManager`, `Hal`, everything under `Managers/`, `Effects/`,
  `AnimationBase.h`, `Types.h`, `Config.h`, `ProductConfig.h` -- the
  dormant tree. `ButtonGestureEngine` is a new, separate class, not an
  edit to the dormant `ButtonManager`.

## [0.2.0-alpha] — Button roadmap revision [BRoadmap v1.1]: Developer Mode is Serial-only; Audio Reactive is an overlay

**Status: documentation + one code simplification, no new hardware test
required beyond re-confirming the removed gate doesn't regress anything
already passing.**

### Roadmap / specification changes (`docs/PRODUCT_SPEC.md`, `docs/DESIGN_LOG.md`)
- Physical button is reserved entirely for product features. Developer/
  diagnostic access is Serial-only, permanently, and consumes no
  physical button gesture -- not even at boot.
- Button gesture table (Section 3) updated: Quad Press and the 3-state
  audio mode cycle are removed. New gesture: Double Press + Hold toggles
  Audio Reactive Mode. Single/Double/Triple Press retarget from
  "animation"/"palette" wording to "effect"/"palette" to match current
  product terminology; Long Press, Brightness Ramp, and Factory Reset
  are unchanged.
- Audio Mode Behavior (Section 10) simplified from a 3-state enum
  (Disabled/Overlay/Dedicated) to a single on/off overlay toggle. The
  "Dedicated Audio Effects" concept and `SoundReactive` effect family
  are removed from the design -- audio always modulates whatever effect
  is currently selected, never replaces it. Graceful degradation (an
  effect with no audio support just keeps running, unmodulated, no
  crash) is now an explicit hard requirement, not an implementation
  detail.
- Both sections flag open implementation items (new `ButtonManager`
  state for the compound gesture; a default-safe audio-modulation hook
  on the effect interface) as work for whichever future milestone
  revives the dormant tree -- not decided or implemented now.
- `docs/DESIGN_LOG.md` Section 11 records the rationale for both
  changes.

### Code change (`src/EngineeringConsole.h`/`.cpp`)
- Removed the button-held-at-boot Developer Mode gate entirely, along
  with the `m_developerMode` member, the `beginHardwareBaseline()`/gate
  split, and the periodic Developer-Mode-only status timer introduced
  in the previous hotfix. `begin()` is back to one straight-line
  sequence, identical in shape to `BringUpDashboard::begin()`, with no
  button read for any gating purpose.
- `s`/`S` is now a plain, always-available, on-demand command --
  exactly like `m` -- with no mode check of any kind.
- Net effect: this firmware no longer has *any* code path that reads
  the button for a purpose other than the Button Test mode (`8`) and
  the existing `renderFrame()` live-state check -- both pre-existing,
  unchanged, hardware-confirmed behavior from `BringUpDashboard`.

### Not touched
- `BringUpDashboard.h`/`.cpp` -- still unmodified, still the one-line
  rollback target.
- `SystemManager`, `Hal`, everything under `Managers/`, `Effects/`,
  `AnimationBase.h`, `Types.h`, `Config.h`, `ProductConfig.h` -- the
  dormant tree. The new button map and audio overlay design apply to
  this tree's *eventual* revival; no enum, struct, or logic in these
  files was changed as part of this update.

## [0.2.0-alpha] — Milestone 1 hotfix: Developer Mode hardware acceptance failure

**Status: candidate fix, awaiting hardware re-verification.** The
Milestone 1 build below failed hardware acceptance testing:

- Normal boot and commands (`1`/`2`/`3` confirmed RED/GREEN/BLUE) worked
  correctly.
- **Developer Mode booted with LEDs dark** (not the expected solid red).
- **Typing `s` caused a Serial Monitor disconnect / PermissionError**,
  recoverable only by a full USB-C unplug/replug.

Per the charter's stop-condition rule ("if LED output ever fails,
STOP"), `main.cpp` was immediately reverted to `BringUpDashboard`.

**Root cause, to the extent it can be determined without a hardware-
connected build environment:** the original `begin()` read the
Developer Mode gate *before* confirming hardware init, and called
`printEngineeringStatus()` (multiple `Serial.println` calls plus
`ESP.getFreeHeap()`) synchronously, inside `begin()`, during the
cold-boot window -- the least stable moment for a fresh USB serial
connection. This is the most likely trigger, though it has not been
independently confirmed by a passing hardware test yet.

**Fix, in `EngineeringConsole.h`/`.cpp`:**
- Hardware init is now isolated in `beginHardwareBaseline()`, which
  contains zero reference to `m_developerMode` and runs to completion,
  identically for both modes, before the gate is read at all.
- The Developer Mode gate (`digitalRead(BUTTON_PIN)`) is now read
  strictly *after* `beginHardwareBaseline()` returns -- so by
  construction, entering Developer Mode cannot change what hardware
  init already did.
- The synchronous status-block print during `begin()` has been removed
  entirely, in both modes. The status block is now only ever produced
  from `update()` -- via the `s`/`S` command or the periodic Developer
  Mode timer -- never during boot.
- `s`/`S` handling is unchanged in scope (Developer Mode only, falls
  through to "Unknown command" in normal boot) but is now guaranteed to
  run only after the loop is stable, not during the cold-boot window.

**`main.cpp`** now points at this patched `EngineeringConsole` again,
labeled explicitly as an unverified candidate. If it fails hardware
acceptance again, revert `main.cpp` to `BringUpDashboard` (unchanged,
one-line-per-call swap, exactly as before).

**Suggested (optional, not applied) secondary investigation:** if the
Serial disconnect recurs, it may be worth checking `platformio.ini`'s
`monitor_dtr`/`monitor_rts` settings -- some ESP32 boards' auto-reset
circuitry can interact oddly with a physical button held on GPIO0 (a
boot-strapping pin) at the exact moment a serial monitor opens and
toggles DTR/RTS. This is a monitor/hardware interaction, not something
fixable from firmware alone, so it's not applied here -- flagging it in
case the hotfix above doesn't fully resolve the symptom.

## [0.2.0-alpha] — Milestone 1: Engineering Console + Developer Mode

**Status: hardware-verified baseline evolves; dormant tree untouched.**
`main.cpp` now boots `EngineeringConsole` instead of `BringUpDashboard`.
This is an additive evolution of the Bring-Up Dashboard's own code, not
a rewrite on top of `SystemManager` -- the dormant architecture tree
(`SystemManager`, `Hal`, `LEDDriver`, `ButtonManager`, `SettingsManager`,
`PaletteManager`, `EffectRegistry`, `AnimationManager`, `SolidEffect`)
is not read, included, linked, or executed by this change.

### Added
- `src/EngineeringConsole.h` / `.cpp` -- evolves the Bring-Up Dashboard
  into a permanent diagnostics interface:
  - **Developer Mode boot gate**: button held at boot (before any
    Serial output) latches Developer Mode for that session only.
  - **Engineering Console status block**: prints hardware identity,
    LED brightness/pixel count/current effect, raw button state,
    uptime, and free heap. Read-only -- no GPIO writes, no FastLED
    buffer writes, no relay calls.
  - **`s` command**: Developer Mode only. Not present in the normal-
    boot command surface at all -- typing `s` during a normal boot
    falls through to the existing "Unknown command" path exactly as
    it would have with no `s` handling.
  - Pixel buffer (`g_engineeringConsoleLeds`) and hardware constants
    are held in an anonymous namespace with a unique buffer name,
    specifically to avoid colliding with `BringUpDashboard.cpp`'s own
    file-scope `leds[]` global, since that file remains compiled but
    unreferenced.

### Unchanged (verified-equivalent, not modified)
- LED data pin, relay pin, button pin, LED count, frame timing.
- Commands `1`–`9`, `+`, `-`, `m` -- identical command surface and
  behavior during a normal (non-Developer-Mode) boot.
- Startup relay/LED sequence: relay energized, FastLED init, default
  solid-red fill -- identical in both boot paths.

### Not touched
- `BringUpDashboard.h` / `.cpp` -- left in the repo, unmodified,
  simply no longer instantiated by `main.cpp`. Kept as the one-line
  rollback target.
- `SystemManager`, `Hal`, every file under `Managers/`, `Effects/`,
  `AnimationBase.h` -- the dormant architecture tree. Not read or
  depended on by this milestone. See `docs/ARCHITECTURE_CONTRACT.md`
  for the current reconciliation status between this hardware-
  verified console and that tree.

### Rollback
Change `main.cpp` to instantiate `BringUpDashboard` instead of
`EngineeringConsole` (two lines: the member declaration and its
`begin()`/`update()` calls). No other file needs to change.

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

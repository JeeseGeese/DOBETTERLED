# Architecture Contract — Ownership Boundaries

This document is the enforceable contract for *who owns what*. Where
`docs/DESIGN_LOG.md` explains why a decision was made and
`docs/PRODUCT_SPEC.md` defines observable behavior, this file defines
**module boundaries**: what each piece is allowed to touch, and — just
as important — what it is explicitly forbidden from touching. When in
doubt during implementation, this is the file to check before adding a
new call or dependency between modules.

If a change would require a module to reach outside its listed
ownership, that's a signal the design needs a conversation, not a quick
workaround.

---

## ⚠️ Current status: this entire tree is DORMANT

As of Milestone 1 (v0.2.0-alpha), **every module documented below —
`SystemManager`, `Hal`, `LEDDriver`, `ButtonManager`, `SettingsManager`,
`PaletteManager`, `EffectRegistry`, `AnimationManager`, `SolidEffect`,
`AnimationBase` — is dormant.** None of it is included, linked, or
executed by the current build. `main.cpp` instantiates
`EngineeringConsole` (`src/EngineeringConsole.h`/`.cpp`), a standalone
evolution of the earlier Bring-Up Dashboard, not this tree.

This tree's own hardware bring-up stalled mid-diagnostic (see
`CHANGELOG.md`'s v1.0.1/v1.0.2 entries — `SystemManager::begin()` and
`LEDDriver::begin()` still contain unresolved diagnostic bypass code)
and was never confirmed to light LEDs on real hardware before the
project pivoted to the Bring-Up Dashboard / Engineering Console
instead. The ownership boundaries below remain the intended design and
should still be honored **if and when** this tree's bring-up is
completed and hardware-confirmed — but until a later milestone does
that work and reconciles the two trees, treat everything below as
unverified design documentation, not a description of running code.

Do not extend, "fix," or route new features into this tree as a side
effect of unrelated work. If a genuine engineering reason exists to
revive it, that should be its own milestone with its own hardware
verification checklist — not an incidental change.

---

## SystemManager

**Owns:**
- Runtime orchestration — the only module that constructs and holds
  references to every other manager.
- Mapping `ButtonEvent` values (and the duration/intent accessors that
  accompany them) to concrete actions — next animation, toggle power,
  apply a brightness delta, cycle audio mode, trigger factory reset, etc.
- Coordinating cross-manager sequences: boot sequencing, shutdown
  sequencing, factory reset (clearing `SettingsManager`, resetting
  `AnimationManager`/`PaletteManager` selection, re-applying defaults to
  `LEDDriver`).
- The authoritative live brightness value during a brightness ramp: it
  reads `ButtonManager::consumeBrightnessRampDelta()`, applies it to the
  value it gets from `SettingsManager`, clamps to
  `LedConfig::MinBrightness`/`MaxBrightness`, and pushes the result to
  both `LEDDriver` and `SettingsManager`.

**Does not own:**
- Hardware access of any kind (goes through managers, never the HAL
  directly).
- Persistent storage format (that's `SettingsManager`'s).
- Pixel-level rendering detail (that's `LEDDriver`'s).
- Gesture classification detail (that's `ButtonManager`'s — SystemManager
  consumes the *result*, never re-implements debounce/click-counting).

---

## SettingsManager

**Owns:**
- Persistent user preferences **only**: brightness, current animation ID,
  current palette ID, power state, audio mode, speed, intensity, primary
  color, secondary color.
- The dirty-flag / debounced-commit model against ESP32 `Preferences`
  (NVS) — deciding *when* a change is idle long enough to actually write
  to flash.
- Defaulting to `Config.h` values on first boot (or after a factory
  reset) and persisting that baseline once.

**Does not own:**
- Hardware control of any kind. `SettingsManager` never calls `Hal::*`
  or `LEDDriver::*` — it is a pure state store. `SystemManager` reads
  values out of it and is the one that applies them to hardware-facing
  managers.
- Interpretation of what a value *means* (e.g., it doesn't know
  `AnimationId::Fire` renders fire — it just stores the number).
- Any temporary/in-progress state that shouldn't be persisted (e.g., the
  live brightness value *during* an active ramp is `SystemManager`'s
  concern until the ramp ends and the final value is written back here).

---

## LEDDriver

**Owns:**
- FastLED entirely — the only module in the codebase permitted to
  include `<FastLED.h>` or reference `CRGB`.
- The pixel buffer, brightness scaling, and pushing frames to the strip
  (`show()`).
- The LED power relay call (via the HAL), for instant on/off.

**Does not own:**
- Animations or transitions. `LEDDriver` has no concept of "which effect
  is running," "fade progress," or "blend between two effects" — it only
  ever receives a buffer of colors to display. (Fades/transitions are
  explicitly deferred to a later milestone, and even then, they'll be
  computed by whatever calls `LEDDriver`, not inside it.)
- Settings persistence (it doesn't know or care what brightness was
  *yesterday* — it just applies whatever value it's given).
- Any pin/GPIO access beyond the one documented, unavoidable exception
  (FastLED's compile-time data-pin template argument, sourced from
  `ProductConfig.h`).

---

## AnimationManager

**Owns:**
- Which effect is currently selected, and next/previous/select-by-ID
  navigation through the effect list (via `EffectRegistry`, once it
  exists).
- Per-frame invocation of the current effect's `update()`, passing it an
  `AnimationContext` built from `PaletteManager`, `SoundManager`, and the
  active `EffectSettings`.
- Effect/palette transition blending, once that milestone is reached.

**Does not own:**
- Persistence (reads/writes the *current selection* through
  `SettingsManager`, doesn't implement storage itself).
- Hardware access (renders through `LEDDriver` only).
- Button interpretation (receives "next animation" as an instruction
  from `SystemManager`, never reads `ButtonManager` directly).

---

## SoundManager

**Owns:**
- All microphone processing and audio analysis: noise floor calibration,
  automatic gain control, envelope/peak detection, beat detection.
- The `getVolume()` / `getBass()` / `isBeat()` accessor surface consumed
  by effects and `AnimationManager`.
- Deciding when to start/stop I2S sampling via `Hal::micBegin()`/
  `Hal::micEnd()`, based on the active `AudioMode` it's told about.

**Does not own:**
- The `AudioMode` value itself (that's persisted state, owned by
  `SettingsManager`; `SystemManager` tells `SoundManager` when the mode
  changes).
- Which effect is showing, or whether "Overlay" vs. "Dedicated" changes
  what's rendered — that's `AnimationManager`'s and `SystemManager`'s
  concern. `SoundManager` only ever answers "what does the audio look
  like right now," never "what should be on screen."

---

## ButtonManager

**Owns:**
- Physical button interpretation **only**: debouncing, click counting,
  long-press phase tracking (`Start`/`Hold`/`Release`), brightness-ramp
  intent/direction (never an absolute value), and factory-reset
  confirmation timing.
- Consumes only `Hal::isButtonPressed()` — no GPIO, no `ProductConfig`.

**Does not own:**
- Any application state. It doesn't know what a "next animation" is,
  doesn't know the current brightness, doesn't know what palette is
  active, and doesn't decide what a given gesture *should do* — it only
  reports what the button physically did. `SystemManager` is the only
  consumer that turns a `ButtonEvent` into an action.

---

## Summary Table

| Module | Owns | Never touches |
|---|---|---|
| SystemManager | Orchestration, event→action mapping, live ramp brightness | Hardware directly, storage format, gesture detail, pixel rendering |
| SettingsManager | Persisted preference values, dirty-flag NVS commits | Hardware, `LEDDriver`, `Hal` |
| LEDDriver | FastLED, pixel buffer, brightness, frame push | Animation/transition logic, persistence |
| AnimationManager | Current effect selection/state, per-frame effect invocation | Persistence storage, hardware, button interpretation |
| SoundManager | Mic sampling, audio analysis, calibration/AGC | Which `AudioMode` is active (told, not decided), what's rendered |
| ButtonManager | Gesture classification, hold timing, ramp intent | Application/settings state, GPIO/`ProductConfig` |

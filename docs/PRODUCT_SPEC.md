# Product Specification — DOBETTERLED
**Hardware Target (v1): QuinLED Dig2Go**
**Status:** Milestone 1 — Source of Truth
**Firmware Version:** 0.1.0-spec (tracks `Config.h::FirmwareInfo`, independent of the version below)
**Button Roadmap Version ("BRoadmap"): v1.3**
*(the "B" stands for Button — this version tracks changes to the physical-button gesture design specifically, i.e. Sections 3 and 10 below. It is not a version number for this entire document.)*

### Button Roadmap Revision History

| Version | Summary | Sections touched |
|---|---|---|
| v1.0 | Initial locked button design — single/double/triple/quad click gestures, long-press family (toggle, brightness ramp, factory reset), 3-state audio mode cycle | 3, 10 |
| v1.1 | Developer Mode is Serial-only (no button gesture, ever); Audio Reactive Mode collapsed from a 3-state cycle to a simple on/off overlay; Quad Press and the "Dedicated" audio state removed; new Double Press + Hold gesture added | 3, 10 |
| v1.2 | Finalized map for Milestone 2 implementation: Long Hold (2-3s) repurposed from "toggle on/off" to Quick Settings Menu candidate; continuous-hold Brightness Ramp and Very-Long-Press Factory Reset removed entirely; replaced with discrete click counts — 6 presses = Power On/Off candidate, 10 presses = Factory Reset candidate (with a "pending" report only, no immediate action — real confirmation mechanism deferred to a later milestone) | 3, 4, 8 |
| v1.3 | Milestone 3 implementation: added 4 Presses = Next Mode, grouping the effect list into three categories (**Static**, **Motion**, **Reactive**) that 1/2-press cycling stays scoped to; explicit disambiguation that the **Reactive** category name is unrelated to the **Audio Reactive Overlay** toggle; finalized the Overlay's navigation contract (never blocks 1/2/3/4-press, persists across effect/palette/Mode changes); 1/2/3/4-press and Double Press + Hold are now wired to real actions in `EffectEngine`/`EngineeringConsole`, not just detected | 3, 10 |

**Convention going forward:** any change to the button gesture table
(Section 3) or audio mode behavior (Section 10) bumps the **Button
Roadmap ("BRoadmap") Version** — minor for additions/clarifications,
major for a breaking redefinition of existing gesture behavior — and
gets a row above naming exactly what changed. Rationale for each bump
lives in `docs/DESIGN_LOG.md`, tagged `[BRoadmap vX.Y]`. Delivered zips
for a given update are named with the matching tag (e.g.
`DOBETTERLED_ButtonRoadmap_v1.1.zip`) so a specific conversation's
output can be traced back to the button design version it implements.
This version number is scoped to the button/audio design only — it
does not track changes elsewhere in this document (startup behavior,
settings persistence, etc.), which remain covered by ordinary
`CHANGELOG.md`/milestone tracking instead.

---

## 0. Scope & Intent

This document defines *behavior*, not implementation. It is the contract
that every later milestone's code must satisfy. If code and this document
ever disagree, either the code has a bug or this document needs a
deliberate, reviewed update — it should never drift silently.

This firmware is a **platform**, not a single-board sketch. The Dig2Go is
the first supported **hardware target**, wired in through a
`ProductConfig` layer (Section 10). Nothing in the behavioral spec below
is Dig2Go-specific unless explicitly called out.

---

## 1. Startup Behavior

1. On power-up, the HAL initializes GPIO, the LED data line, and (if
   `LedConfig::UseRelayForPower`) the relay pin, relay defaulting to
   **off** until settings are loaded.
2. `SettingsManager` loads persisted state from NVS:
   - Brightness
   - Current animation ID
   - Current palette ID
   - Power state (on/off)
   - Audio mode (Disabled / Overlay / Dedicated)
   - Animation speed, intensity
   - Primary color, secondary color
3. If no saved settings exist (first boot, or after factory reset),
   defaults from `Config.h` are used and immediately persisted once.
4. If the restored power state is **On**:
   - Relay (if present) is enabled.
   - LEDs fade in from black to the restored brightness/effect over a
     short, configurable duration (`Timing::StartupFadeMs`) rather than
     snapping instantly to full brightness. This avoids a jarring flash
     and gives an obvious "the device is alive" signal.
5. If restored power state is **Off**:
   - Relay stays off (if present). No LED activity. Button input is
     still processed (a press should be able to turn the device on).
6. Audio hardware (I2S mic) initializes lazily — only when audio mode is
   anything other than **Disabled** — so devices that never use sound
   reactive effects don't pay an init cost or hold the I2S peripheral.
7. Boot must not block on anything with unbounded wait time. Any
   calibration (e.g. noise floor) happens asynchronously, in the
   background, on the first activation of audio mode, not during
   `setup()`.

---

## 2. Shutdown Behavior ("turning LEDs off")

Triggered by Long Press (Section 3) or explicit "off" logic elsewhere.

1. LEDs fade out from current brightness to black over
   `Timing::ShutdownFadeMs`, rather than cutting instantly — mirrors the
   startup fade for a consistent, polished feel.
2. Once the fade reaches black, if `LedConfig::UseRelayForPower` is true,
   the relay is de-energized (true power cut to the strip).
3. Power state is persisted as **Off**.
4. Current animation, palette, brightness, speed, intensity, and colors
   are all **retained** in RAM and will be restored exactly when powered
   back on — turning off is not the same as resetting.
5. Button input continues to be read while off; only LED rendering and
   audio sampling are suspended (audio sampling stops to save power/CPU
   when off, since nothing needs modulating).

---

## 3. Button Behavior (Complete Gesture Table)

**Revision note (BRoadmap v1.3, Milestone 3 implementation):** the
physical button is reserved entirely for product features. Developer/
diagnostic access is Serial-only (via the Engineering Console's `s`/`m`
commands) and **does not consume, gate, or reserve any physical button
gesture** — not even at boot.

Single physical button. All gestures are edge- and duration-based,
software debounced (`Timing::DebounceMs`), and reported as discrete
`ButtonEvent` values (see Types.h) from `ButtonManager`. ButtonManager
never invokes any other manager directly — `SystemManager` is the only
consumer of these events and decides what they do. (In the current
standalone implementation, `ButtonGestureEngine` plays this role for
`EngineeringConsole` — see the Milestone 3 scope note below.)

Effects are grouped into three Mode categories that 1/2-press
navigation stays scoped to:

| Mode Category | Effects |
|---|---|
| **Static** | Solid |
| **Motion** | Rainbow, Chase |
| **Reactive** | Confetti, Sparkle, Fire |

**Disambiguation:** the **Reactive** Mode category above is a
visual-character grouping name (organic/randomized effects) and is
**unrelated** to the **Audio Reactive Overlay** toggle below — they are
two independent axes that happen to share a word. Turning the Overlay
on/off never changes Mode category, effect, or palette; switching Mode
category never touches the Overlay flag.

| Gesture | Definition | Effect | Wired? |
|---|---|---|---|
| 1 Press | 1 click, released before the click window expires with no further click or hold | Next Effect (within current Mode category) | Yes |
| 2 Presses | 2 clicks within the click window, second press released (not held) | Previous Effect (within current Mode category) | Yes |
| 3 Presses | 3 clicks within the click window | Next Palette | Yes |
| 4 Presses | 4 clicks within the click window | Next Mode (Static → Motion → Reactive → wraps) | Yes |
| 2 Presses + Hold | 2 clicks within the click window, where the **second** press is not released but held past the hold-confirm threshold | Toggle Audio Reactive Overlay | Yes (tracked flag only — no microphone input) |
| Long Hold (2–3s) | The **first** press of a sequence held continuously for 2.0–3.0s | Quick Settings Menu candidate | Detect/report only |
| 6 Presses | 6 clicks within the click window (each click extends the window) | Power On/Off candidate | Detect/report only |
| 10 Presses | 10 clicks within the click window | Factory Reset candidate — reports "pending" only; does **not** erase settings (see Section 8) | Detect/report only |

**Milestone 3 scope note:** 1/2/3/4-press and Double Press + Hold are
wired to real actions via two standalone, hardware-decoupled classes:
`ButtonGestureEngine` (detection, returns a `Gesture` per call) and
`EffectEngine` (effect/palette rendering), coordinated by
`EngineeringConsole`. Neither is an edit to the dormant `ButtonManager`/
`AnimationManager`/`EffectRegistry`/`PaletteManager`. Long Hold,
6-press, and 10-press remain detection/report-only — no Quick Settings
Menu, no power toggle, no settings erase. That remaining wiring is
later-milestone work, gated on the dormant tree's own hardware
bring-up being completed first (see `docs/ARCHITECTURE_CONTRACT.md`).

Notes:
- **Continuous-hold Brightness Ramp and Very Long Press (5–10s) Factory
  Reset from v1.1 are removed.** Brightness adjustment has no defined
  gesture as of this revision (see Section 4's open item). Factory
  Reset is now reached via click-count (10 presses), not hold duration.
- **The v1.0 "Quad Press" and the 3-state `AudioMode` cycle it drove are
  still removed** — Audio Reactive Mode remains a simple on/off overlay
  (Section 10). **4 Presses is reintroduced as of v1.3**, but as a
  different gesture with a different meaning (Next Mode category, not
  a 4th audio state) — not a revival of the old quad-click concept.
- Gesture classification for click-count gestures (1/2/3/4/6/10
  presses) only finalizes once the click window elapses with no further
  press and no qualifying hold. 2 Presses vs. 2 Presses + Hold forks on
  whether the second press is released before or after crossing the
  hold-confirm threshold. Any hold occurring on the 3rd+ press of a
  sequence, or any completed click count other than 1/2/3/4/6/10, is
  explicitly out of scope for this table — the implementation reports
  it for visibility but assigns it no product meaning.
- Long Hold (2–3s) only applies to the **first** press of a sequence
  (no prior completed clicks pending) — holding after 2+ prior clicks
  is governed by the 2 Presses + Hold row instead, not this one.
- 10 Presses (Factory Reset) requires a Section-8-defined follow-up
  confirmation (a subsequent hold) before an actual reset occurs — that
  confirmation mechanism is deferred to a later milestone. As of this
  revision, 10 presses only ever reports "pending," never performs a
  reset.
- All button-specific timing constants for this milestone's
  implementation live locally in `ButtonGestureEngine.cpp`, independent
  of `Config.h::Timing` (which still describes the dormant tree's own,
  not-yet-reconciled thresholds).

**Open implementation item (carried from v1.1, still open):** the
dormant `ButtonManager`'s state machine doesn't track "the Nth click in
a sequence turned into a hold" at all. `ButtonGestureEngine` (Milestone
2) implements this fresh, independently — it is not an edit to
`ButtonManager` and is explicitly built to be extractable into it later,
per Milestone 2's implementation goal.

---

## 4. Brightness Logic

**Revision note (BRoadmap v1.2):** the Hold/Ramp gesture this section
depended on no longer exists (Section 3). Brightness adjustment
currently has **no defined gesture** — this is an explicit open item,
not an oversight. A future BRoadmap revision must either assign a new
gesture, move brightness into the Quick Settings Menu (Section 3's
Long Hold candidate), or otherwise redefine how it's reached, before
any milestone implements live brightness control against the finalized
map. The mechanics below (constant-time ramp, direction reversal,
debounced persistence) describe the *behavior once triggered*, not
*how it's triggered* — that trigger is what's now undefined.

- Range: `LedConfig::MinBrightness` (never fully invisible while "on")
  to `LedConfig::MaxBrightness` (255).
- Ramp speed is constant-time, not constant-step: a full 0→255 sweep
  always takes `BrightnessRampFullSweepMs`, regardless of frame rate,
  computed from elapsed time each tick rather than counting fixed ticks.
- Direction reverses automatically at both ends (0 and 255) while
  triggered, so a sustained ramp "breathes" up and down rather than
  clamping and doing nothing.
- Brightness changes take effect on the live LED output immediately
  (no fade lag during ramping) but are **not** written to NVS until the
  dirty-flag debounce timer (`SettingsSaveDebounceMs`) elapses after
  the ramp ends — see Section 7.

---

## 5. Palette Behavior

- A fixed, ordered set of palettes (Rainbow, Ocean, Forest, Lava, Cloud,
  Sunset, Ice, Neon, Pastel, Halloween, Christmas) is owned by
  `PaletteManager`.
- Triple Press advances to the next palette, wrapping from the last back
  to the first.
- Not all effects consume the active palette. Effects that are
  explicitly "primary/secondary color" driven (Section 6) may ignore the
  palette entirely; effects that are gradient/rainbow-style consume it.
  Each effect declares which color source(s) it uses.
- Changing palette does not change or reset the current animation,
  speed, or intensity — it is an independent axis.

---

## 6. Effect Parameter Standard

Every effect is driven by a shared `EffectSettings` structure (delivered
via `AnimationContext`, owned/persisted by `SettingsManager`) with:

- **Speed** (0–255)
- **Intensity** (0–255)
- **Palette reference** (current `PaletteId`, resolved to a `CRGBPalette16` by `PaletteManager`)
- **Primary Color** (`CRGB`) — explicit user-set color, independent of palette
- **Secondary Color** (`CRGB`) — explicit user-set second color (e.g. background/accent)

Not every effect uses every field — e.g. `Rainbow` ignores Primary/Secondary
Color entirely and is palette-driven; `Solid` uses only Primary Color and
ignores palette. Each effect's own header documents which fields it
consumes so the behavior is discoverable without reading its `.cpp`.

v1 does not expose a discrete UI gesture for setting Primary/Secondary
Color directly (the physical single-button interface has no room for
color-picking); they exist as a settings-layer/API surface for now,
defaulted in `Config.h`, so a future product variant with more inputs
(e.g. an app, an encoder, a touch interface) can expose them without
firmware changes to the effects themselves.

---

## 7. Settings Persistence

- Backing store: ESP32 `Preferences` (NVS).
- **Dirty-flag model**: all runtime state lives in RAM; a single dirty
  flag (or small set of per-group flags) is set whenever any persisted
  field changes. A background check in the main loop commits to NVS only
  after `SettingsSaveDebounceMs` has elapsed with no further changes —
  not on every change.
- This means: rapid brightness ramping, quickly cycling through
  animations, etc. produce **zero** flash writes until the user stops
  interacting and the debounce window elapses. This is a hard
  requirement, not an optimization — flash has finite write endurance.
- Persisted fields: brightness, animation ID, palette ID, power state,
  audio mode, speed, intensity, primary color, secondary color.
- On a clean commit, all dirty fields are written together in one NVS
  transaction where practical, rather than field-by-field, to minimize
  write overhead.

---

## 8. Factory Reset Behavior

**Revision note (BRoadmap v1.2):** the trigger mechanism changes from
"hold continuously for 5–10 seconds" to "10 presses" (Section 3),
followed by a separate confirmation step. This section's mechanics
(what a reset actually clears/restores) are unchanged — only how it's
*reached* is different, and the confirmation step below is a **planned
design, not yet implemented**:

1. **10 Presses** reports "Factory Reset Pending" (Section 3) and takes
   no destructive action by itself.
2. **Planned, not yet implemented:** a follow-up continuous hold of
   ~3 seconds after the pending state, acting as the confirmation —
   analogous in spirit to the old countdown-hold, but starting from the
   pending state rather than from press 1. Visual countdown/confirmation
   feedback (Section 9) should accompany this hold once it's built, so
   the user can abort by releasing early, same as the original design's
   intent.
3. Milestone 2 implements step 1 only (Serial-reported "pending"). Step
   2's confirmation hold, and the actual settings-erase logic below, are
   later-milestone work — gated on the dormant `SettingsManager` tree's
   own hardware bring-up being completed first.

**Mechanics once an actual reset is confirmed and fires** (unchanged
from v1.0/v1.1, not yet implemented against the new trigger):

1. All NVS keys for this firmware are cleared.
2. In-RAM state resets to `Config.h` defaults: default brightness,
   default animation, default palette, audio mode Disabled, default
   speed/intensity, default primary/secondary colors.
3. Power state resets to **On** (so the user gets immediate visual
   confirmation the reset succeeded, rather than a dark strip that
   looks like a failure).
4. Defaults are immediately persisted to NVS as the new baseline.

---

## 9. Visual Feedback Requirements

Beyond normal effect rendering, the firmware provides brief visual
confirmation for state changes so a screen-less, single-button device
still feels responsive and legible:

- **Startup fade-in** / **Shutdown fade-out** (Section 1, 2).
- **Effect/palette change**: a short, subtle flash or transition blend
  (not a jarring hard cut) when switching animations or palettes via
  button press, using `Timing::EffectTransitionMs`.
- **Factory reset confirmation**: once the hold exceeds
  `FactoryResetMinMs`, the strip visibly signals "reset pending" (e.g. a
  distinct pulsing color, decoupled from whatever effect was previously
  showing) so the user gets unambiguous feedback that continuing to hold
  will erase settings. Releasing before the pending signal appears
  should never reset anything.
- Visual feedback effects are rendered through the same `LEDDriver`
  interface as regular effects but are **not** registered in
  `EffectRegistry` — they are transient, system-owned overlays driven by
  `SystemManager`, not user-selectable content.

---

## 10. Audio Mode Behavior

**Revision note (post-Milestone-1):** the original 3-state model
(Disabled / Overlay / Dedicated) is replaced with a simpler design.
There is no more "Dedicated Audio Effects" mode and no `SoundReactive`
effect family — Audio Reactive Mode is **always** an overlay/modifier
on whatever effect is currently running, never a separate effect slot.

Audio Reactive Mode is a **simple on/off toggle**, independent from
effect selection:

| State | Behavior |
|---|---|
| **Off** (default) | Microphone is not sampled. No effect is modulated by sound. |
| **On** | Microphone is sampled and analyzed (volume, bass, beat). The **currently selected effect** (whatever it is — Solid, Rainbow, Chase, Fire, Plasma, etc.) continues running exactly as it would otherwise, but has its parameters modulated by sound wherever that effect supports it. |

- Toggled by the Double Press + Hold gesture (Section 3) — there is no
  more multi-state cycling. **As of BRoadmap v1.3, this toggle is wired
  to a real flag** (`EngineeringConsole::m_audioReactiveOverlay`,
  tracked and Serial-reported) — no microphone sampling or modulation
  is implemented yet; see the Milestone 3 scope note in Section 3.
- **Navigation contract (BRoadmap v1.3):** the Overlay is a global
  on/off flag, never a separate effect. It must never block or
  intercept 1/2/3/4-press navigation, and it persists as-is across
  every effect, palette, and Mode change — it is never reset or
  reapplied per-effect. Whichever effect ends up selected renders with
  modulation immediately if it supports audio (once modulation exists),
  or continues rendering normally, flag still on, if it doesn't.
- Turning Audio Reactive Mode on/off never changes which effect is
  selected, and never changes which palette is selected. It only
  changes whether that effect's parameters are being modulated.
- **Graceful degradation is required, not optional:** if the currently
  active effect does not implement audio modulation, turning Audio
  Reactive Mode on must not crash, hang, glitch, or blank the display —
  the effect simply continues rendering normally, unmodulated, until
  either the effect changes to one that does support it, or Audio
  Reactive Mode is turned back off. This must hold for every existing
  and future effect without each one needing bespoke guard code at the
  call site — the mechanism (see below) should make "doesn't support
  audio" the safe default, not a special case effects must remember to
  handle.
- Effects that DO support modulation may use audio-derived signals
  (volume, bass envelope, beat pulses) to affect: intensity,
  brightness, animation speed, pulse/flash amount, color movement
  (e.g., palette index cycling faster on a beat), or other
  effect-specific parameters — whichever are meaningful for that
  effect. Not every effect needs to use every signal.
- Turning Audio Reactive Mode off stops mic sampling (power/CPU saving)
  regardless of which effect is active.
- `SoundManager` performs: noise floor calibration on first activation,
  automatic gain control, peak/envelope detection, and beat detection,
  exposing simple accessors (`getVolume()`, `getBass()`, `isBeat()`) —
  these accessors return static/neutral values while Audio Reactive
  Mode is off, so effects never need to branch on "is the mic even on."

**Open implementation item:** the current `AnimationBase`/effect
interface (dormant tree) doesn't yet have a defined, optional
audio-modulation hook. The graceful-degradation guarantee above implies
something like a default no-op virtual method (or an
`AnimationContext` field effects opt into reading) rather than a
required override — so that an effect written with zero awareness of
audio still compiles and runs safely once Audio Reactive Mode exists.
Designing that interface is work for whichever milestone implements
this, not decided here.

---

## 11. Timing Constants (Summary — authoritative values live in `Config.h`)

| Constant | Purpose |
|---|---|
| `TargetFrameIntervalMs` | Target render cadence (~60 FPS) |
| `ButtonPollIntervalMs` | How often raw button pin is sampled |
| `DebounceMs` | Minimum stable duration to count as a real edge |
| `MultiClickWindowMs` | Max gap between clicks in a multi-click gesture |
| `LongPressMinMs` / `LongPressMaxMs` | On/off toggle press duration band |
| `BrightnessHoldStartMs` | Hold duration before ramp mode begins |
| `BrightnessRampFullSweepMs` | Time for a full 0↔255 brightness sweep |
| `FactoryResetMinMs` / `FactoryResetMaxMs` | Factory reset hold duration band |
| `SettingsSaveDebounceMs` | Idle time before a dirty settings commit to NVS |
| `StartupFadeMs` | Fade-in duration on power-up (new, this milestone) |
| `ShutdownFadeMs` | Fade-out duration on power-down (new, this milestone) |
| `EffectTransitionMs` | Blend duration when switching effects/palettes (new, this milestone) |

`StartupFadeMs`, `ShutdownFadeMs`, and `EffectTransitionMs` are new
additions to `Config.h::Timing` required by Sections 1, 2, and 9 above;
they'll be added when `Config.h` is next touched.

---

## 12. Architecture Notes Carried Into Implementation

These aren't new behavior, but they constrain how Milestone 2 must be
built, so they're recorded here as binding constraints:

- **HAL (Hardware Abstraction Layer)**: the *only* module permitted to
  touch GPIO/I2S/ADC directly. `LEDDriver`, `ButtonManager`,
  `SoundManager` all go through it. This is what allows a second,
  differently-wired product to reuse those managers unmodified.
- **ProductConfig layer**: hardware identity (pin numbers, LED count,
  mic bus assignment, relay presence) is isolated from firmware-generic
  configuration (timing constants, default values, limits). The Dig2Go
  is one `ProductConfig`, not the firmware's identity. A second product
  supplies its own `ProductConfig` and reuses everything else untouched.
- **SystemManager**: the single application coordinator. Owns the
  instances of every manager, owns the button-event → action mapping,
  owns the boot/shutdown sequencing described in Sections 1–2. `main.ino`
  only calls `System::begin()` / `System::update()`.
- **EffectRegistry vs. AnimationManager**: `EffectRegistry` is a pure,
  stateless lookup table (ID → effect instance/factory). `AnimationManager`
  is the stateful layer on top — current selection, next/prev, transition
  blending (Section 9) — and is the only thing that talks to
  `EffectRegistry` directly.
- **AnimationContext**: the read-only bundle passed into every effect's
  `update()` — LED buffer access, resolved palette, `EffectSettings`
  (Section 6), and sound accessors. Effects never reach into any manager
  directly; everything they need arrives through this context.

---

## 13. Open Items Deferred Past v1

Recorded so they aren't silently forgotten, but explicitly out of scope
for the initial milestones:

- No discrete UI path to set Primary/Secondary Color via the physical
  button (Section 6) — single-button hardware doesn't support it
  cleanly; deferred to a future input method.
- No IR remote, OLED, encoder, or BLE config yet — pins reserved in
  `ProductConfig`, logic not implemented.
- No battery monitoring yet — ADC pin reserved, logic not implemented.

---

*End of Milestone 1 specification. Implementation (Milestone 2) should
treat any ambiguity not resolved here as a question to raise before
coding, not an assumption to make silently.*

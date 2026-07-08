# Design Log

A running record of architecture decisions and the reasoning behind them.
Kept separate from `PRODUCT_SPEC.md` (which defines *what the firmware
does*) — this file explains *why it's built the way it is*, so future
work doesn't silently re-litigate settled questions or lose the context
behind a non-obvious choice.

Entries are grouped by topic, roughly in the order decisions were made.

---

## 1. Overall Architecture

**Decision:** Layered as `SystemManager → Managers → HAL → Hardware`,
with configuration split into `ProductConfig.h` (board-specific facts)
and `Config.h` (firmware-generic behavior).

**Why:** The original goal was a firmware *platform* for a family of
products, not a single-board sketch. If pin numbers, timing constants,
and behavior logic all lived in one flat file, adding a second hardware
target would mean forking the whole codebase. Splitting hardware
identity from behavior means a new product supplies only a new
`ProductConfig.h`; every manager, the HAL interface, and every effect is
reused unmodified.

**Rule that falls out of this:** if a future hardware target with a
different pinout would need to change a value, it belongs in
`ProductConfig.h`. If every product would plausibly want the same value
(or the same tuning knob with a different number), it belongs in
`Config.h`.

---

## 2. Hardware Abstraction Layer (HAL)

**Decision:** `Hal.h`/`Hal.cpp` is the *only* module permitted to call
`pinMode`/`digitalRead`/`digitalWrite`/I2S driver functions directly.
`Hal.h` does not include `Types.h` and never will — it has zero
awareness of animations, settings, button gestures, or audio modes.

**Why:** This is what makes "managers own behavior" a real, enforceable
boundary rather than a suggestion. If the HAL returned or accepted
`ButtonEvent` or `AudioMode`, application logic would leak into the one
layer that's supposed to be pure hardware access, and reusing managers
on a different board would require also reusing the HAL's interpretation
of what a gesture means — defeating the point of the split.

**Documented exception:** FastLED's `addLeds<>()` requires its data pin
as a **compile-time template argument**, which cannot be satisfied by a
runtime HAL function call. `LEDDriver.cpp` will include `ProductConfig.h`
directly for this one constant. This is a narrow, load-bearing exception
to "only the HAL touches hardware," made necessary by the library's API
shape — not a crack in the boundary, since it's still a named
`ProductConfig` constant, never a hard-coded magic number.

---

## 3. Types.h is deliberately FastLED-free

**Decision:** Introduced a plain `RgbColor{r,g,b}` struct instead of
using FastLED's `CRGB` in any shared header.

**Why:** The stated rule is "no module except LEDDriver should access
FastLED directly." If `Types.h` pulled in `CRGB` and every effect,
`SettingsManager`, and `AnimationContext` used it, that rule would be
true in name but false in practice — everything would still be
structurally coupled to FastLED's type. With a separate `RgbColor`,
`LEDDriver` is the *only* file that ever converts to/from `CRGB`;
swapping LED libraries later touches one file, not every effect and the
settings layer too.

---

## 4. Enum persistence stability (`AnimationId`, `PaletteId`)

**Decision:** These enums are documented as append-only — new values go
at the end, existing values are never reordered or renumbered once
shipped.

**Why:** `SettingsManager` persists these as raw `uint8_t` values to NVS,
not by name. If `AnimationId::Fire` (value 2) were reordered in a future
firmware update, any device in the field with that value saved would
silently boot into whatever effect *now* occupies value 2 — a data
corruption bug with no error message. The fix costs nothing (just a
convention), so it's enforced from the start rather than retrofitted
after a real device ships with saved preferences.

---

## 5. Audio Mode: 3-state enum, independent of effect selection

**Decision:** `AudioMode` is `Disabled / Overlay / Dedicated`, not a
boolean "sound reactive on/off." Quad Press cycles through all three.
Overlay modulates whatever effect is currently selected; Dedicated shows
the `SoundReactive` effect family without discarding the previously
selected "regular" effect (switching back restores it exactly).

**Why:** An early draft treated sound reactivity as a single toggle tied
to one dedicated effect. That doesn't allow "make my current Fire effect
pulse with the music" — a real, commonly desired mode — without either
losing the current effect selection or requiring a second, separate
control. Splitting into three explicit modes captures both use cases
without adding a second button gesture.

---

## 6. Button gesture model: phases + accessors, not pre-classified events

**Decision:** `ButtonEvent` reports `LongPressStart` / `LongPressHold` /
`LongPressRelease` as raw phases, plus `FactoryResetRequested` as a
one-shot auto-fired event. `ButtonManager` does **not** decide "this
was a toggle" vs. "this was a ramp" — it exposes duration accessors
(`getCurrentHoldDurationMs()`, `getLastHoldDurationMs()`,
`isBrightnessRampActive()`, `isFactoryResetWarningActive()`,
`getBrightnessRampValue()`) and lets `SystemManager` classify the hold
using `Config::Timing` thresholds.

**Why:** `ButtonManager`'s explicit constraint is "no animation or
settings logic." Deciding that a 1.4-second hold means "toggle power"
is an interpretation, not a hardware fact — keeping that decision in
`SystemManager` keeps `ButtonManager` a pure, reusable input state
machine that would work identically on a product with entirely
different button-triggered behaviors.

**Sub-decision — mutual exclusivity of toggle vs. ramp:** if a hold is
released *before* the ramp threshold, it's a simple on/off toggle; if it
crosses into ramp territory, release does **not** also toggle power —
it just stops the ramp in place. Without this rule, every ramp session
would end by also flipping the power state, which is very likely not
the intended behavior, but wasn't explicit in either original prompt —
flagged and confirmed before locking the spec.

**Sub-decision — factory reset fires without requiring release:**
`FactoryResetRequested` fires automatically the instant held duration
reaches `Timing::FactoryResetMaxMs`, rather than waiting for the user to
release the button. The warning/confirmation window starts earlier, at
`Timing::FactoryResetMinMs`, exposed via `isFactoryResetWarningActive()`
so the LED can signal "reset pending" well before the trigger point,
giving a real chance to cancel by releasing early. This mirrors how
most consumer devices (routers, etc.) implement hold-to-reset, and
avoids requiring a precisely-timed release to either trigger or avoid
the reset.

**Sub-decision — mixed click+hold edge case:** if a click sequence is
mid-progress (e.g., one click already counted) and the next press turns
into a long hold instead of a quick click, the pending click count is
silently discarded the moment `LongPressStart` fires. This was flagged
as a judgment call (not an explicit requirement) — an alternative would
preserve and fire the earlier clicks first; the current behavior treats
the long hold as taking over entirely, which seemed like the least
surprising outcome for an ambiguous multi-gesture edge case.

---

## 7. Brightness ramp is self-contained, not delta-based against real brightness

**Decision:** `ButtonManager` doesn't know the actual current LED
brightness. It runs its own independent sweep from
`LedConfig::MinBrightness` to `MaxBrightness`, bouncing direction at
each bound, and exposes the result as an **absolute** suggested value
via `getBrightnessRampValue()`. `SystemManager` applies this directly to
`LEDDriver` while `isBrightnessRampActive()` is true.

**Why:** Computing a *delta* against the real brightness would require
`ButtonManager` to either own brightness state (violating "no settings
logic") or query it from another manager (creating a dependency
`ButtonManager` shouldn't have). A self-contained absolute sweep avoids
both problems entirely, at the cost of every ramp session starting from
the same point (breathing from the bottom) rather than continuing from
wherever brightness currently sits. This felt like the right trade for
keeping `ButtonManager` fully decoupled; worth revisiting if the
"restart from bottom every time" behavior feels wrong once it's
actually running on hardware.

---

## 8. Firmware version metadata

**Decision:** `Config::FirmwareInfo` holds the platform's own name and
semantic version (major/minor/patch), separate from
`Product::Name`/`Product::Revision` in `ProductConfig.h`.

**Why:** The firmware platform's version and a given product's identity
are different axes — the same firmware version should be able to run on
multiple products, and a product's hardware revision shouldn't be
conflated with which firmware release it's running.

---

## 9. Compilation & testing constraints (process note, not a code decision)

This development sandbox has no network egress by default and no
Arduino toolchain installed, so real `arduino-cli compile` checks
against the ESP32 core and FastLED aren't possible from here without the
user enabling network access in their Claude settings (Settings →
Capabilities → Code execution and file creation → Allow network
egress → All domains). Absent that, the working process is: code is
written and reviewed carefully by hand for type/signature correctness,
and the user compiles/flashes/tests on real hardware after each
milestone, reporting back anything that fails so it can be fixed before
proceeding — matching the plan's own requirement to "compile and test
after every milestone," just with the compile/flash step happening on
the user's machine rather than in this sandbox.

---

## 10. Milestone 1 spec decisions carried in from `PRODUCT_SPEC.md`

Recorded here too, for a single place to scan all locked decisions:

- Startup/shutdown fade timing (`StartupFadeMs`, `ShutdownFadeMs`,
  `EffectTransitionMs`) were added to `Config::Timing` proactively,
  since Milestone 5's fade requirements are described as required
  behavior in the spec, not optional later polish.
- Factory reset leaves power **on** afterward (not off), so the reset
  gives visible confirmation rather than looking like a dead device.
- Primary/Secondary Color have no button gesture to set them in v1 — the
  single-button interface has no room for a color picker. They exist as
  a persisted/API surface for effects to consume now, ready for a future
  input method (encoder, app, touch) without firmware changes to the
  effects themselves.

---

## 11. Button roadmap revision: Developer Mode is Serial-only; Audio Reactive is an overlay [BRoadmap v1.1], not a mode

**Decision (post-Milestone-1):** two changes to the button/audio design
in `PRODUCT_SPEC.md` Sections 3 and 10:

1. The physical button owns zero Developer/diagnostic behavior, now and
   permanently. Milestone 1's `EngineeringConsole` originally used a
   button-held-at-boot gesture to enter "Developer Mode." That build
   failed hardware acceptance (Developer Mode booted with LEDs dark;
   `s` caused a Serial disconnect). Independent of whatever the exact
   root cause turns out to be, gating diagnostic access behind a
   physical gesture on GPIO0 — a boot-strapping pin already sampled by
   the ESP32's own ROM bootloader — is a fragile design on this
   hardware regardless. Diagnostic access moves to Serial-only: typing
   commands (`s`, `m`) is available any time, with no boot-time read of
   the button for this purpose at all. This also frees the entire
   gesture table for product features, which was the roadmap's original
   intent — Developer Mode was never meant to compete with product
   gestures for the single physical button.

2. Audio Reactive Mode collapses from a 3-state cycle (Disabled /
   Overlay / Dedicated) to a simple on/off overlay. The "Dedicated"
   state and its `SoundReactive` effect family added a second way to
   select what's on screen (effect selection vs. audio-mode selection),
   which meant every future effect implicitly needed a audio-specific
   sibling to be reachable in that mode. Collapsing to overlay-only
   means exactly one selection axis (which effect is active) and one
   independent toggle (whether it's currently being modulated by
   sound) — simpler to reason about, and it matches how most people
   actually want reactive lighting to behave ("make what I already
   picked react to music," not "switch to a different, audio-only
   thing").

**Why graceful degradation is a hard requirement, not a nice-to-have:**
because the toggle is global and effect-agnostic, turning it on can
happen while *any* effect is active, including ones written before
audio modulation existed or ones that just don't have a meaningful way
to use it. The interface must make "no-op if unsupported" the default
behavior of the mechanism itself — not something each effect author
has to remember to implement defensively, since forgetting it would
mean a future effect could crash or glitch the instant a user happens
to have Audio Reactive Mode on. This constrains the eventual
`AnimationBase`/effect interface design (see open item in
`PRODUCT_SPEC.md` Section 10) but that interface itself is not designed
or implemented as part of this revision — it belongs to whichever
milestone actually revives the dormant `AnimationManager`/`SoundManager`
tree and builds real effects against it.

**Scope note:** this is a specification/roadmap change only. No button
gesture recognition, effect switching, or audio modulation exists in
the currently-active `EngineeringConsole` build — that firmware has no
`ButtonManager`/`AnimationManager` equivalent at all. The one concrete
code change made alongside this doc update is removing
`EngineeringConsole`'s button-gated Developer Mode boot check, since it
directly contradicted point 1 above. The full gesture table and audio
overlay system remain future work in the dormant tree, gated on that
tree's own hardware bring-up being completed and verified first.

---

## 12. Button roadmap finalization for Milestone 2: click-count scheme replaces continuous-hold ladder [BRoadmap v1.2]

**Decision:** replace the v1.1 button table's continuous-hold family
(Long Press → toggle on/off, Hold → brightness ramp, Very Long Press →
factory reset) with a discrete click-count scheme: 6 presses for Power
On/Off, 10 presses for Factory Reset (pending-only, confirmation
deferred), and Long Hold (2–3s) repurposed as a Quick Settings Menu
candidate rather than a power toggle.

**Why:** the old design packed three destructively-different outcomes
(toggle, ramp, wipe) onto the same gesture family, distinguished only
by exactly how long a single continuous hold lasted — a design that
asks a lot of a user's sense of time, and asks a lot of the firmware's
debounce/timing precision to get right without a false trigger of the
wrong one. A click-count scheme separates them structurally instead of
temporally: a deliberate, countable action (6 or 10 discrete presses)
is harder to trigger by accident than "I held it a little too long,"
and it reads clearly in Serial debug output as a request/decision made
by us, not us guessing where along a hold-duration continuum the user
intended to land.

**Why Factory Reset is pending-only in Milestone 2:** 10 presses is
still just a click count — nothing structurally prevents an
enthusiastic user (or a bouncing/faulty button) from accidentally
reaching 10 during normal use of lower counts. Requiring a second,
independent confirmation step (a follow-up hold) before anything
destructive happens keeps the actual erase gated behind two different
kinds of intentional input (a count AND a duration), rather than either
alone. Implementing that confirmation step is deferred rather than
rushed alongside the click-counting logic itself, so it can get its own
focused design and hardware verification.

**What's now undefined:** Brightness adjustment has no gesture as of
this revision (`PRODUCT_SPEC.md` Section 4's open item). This is a
known gap, not an oversight — resolving it (new gesture, folding it
into the Quick Settings Menu, or something else) is left to a future
BRoadmap revision rather than guessed at here.

**Scope note:** as with v1.1, this is a specification change plus one
new, standalone implementation (`ButtonGestureEngine`, Milestone 2) that
detects and reports these gestures via Serial only. No gesture is wired
to a real action (no effect switching, no power toggle, no settings
erase) — that wiring is separate, later-milestone work gated on the
dormant tree's own hardware bring-up.

## 13. Milestone 3: Effect Engine, Mode categories, and Audio Overlay navigation contract [BRoadmap v1.3]

**Decision:** add a new standalone `EffectEngine` class (six starter
effects, four palettes) and wire 1/2/3/4-press gestures to it for real:
Next/Previous Effect (scoped to the current Mode category), Next
Palette, and Next Mode (Static → Motion → Reactive → wraps). Double
Press + Hold toggles a tracked-only Audio Reactive Overlay flag. Long
Hold, 6-press, and 10-press remain detection/report-only, unchanged
from v1.2.

**Why Mode categories, and why now:** once a real effect list exists,
"Next Effect" cycling through all six unfiltered means two clicks can
jump between a static color and a fire simulation — a jarring, low-
control experience on a single-button device with no screen to preview
what's coming. Grouping by visual character (Static/Motion/Reactive)
and giving 4-press a dedicated "change category" gesture lets 1/2-press
stay predictable within a category, while still reaching everything.

**Why "Reactive" needed an explicit disambiguation note:** the Mode
category name and the Audio Reactive Overlay toggle are two unrelated
axes that happen to share a word — a category is "what does this look
like" (organic/randomized vs. static vs. moving), the overlay is "is
the mic modulating whatever's currently showing." Conflating them would
make `s`-command status output and Serial gesture logs ambiguous, so
`PRODUCT_SPEC.md` Section 3 states the distinction explicitly rather
than relying on the reader to infer it.

**Why the Overlay is wired now but stays mic-free:** the navigation
contract (never blocks 1/2/3/4-press, persists across effect/palette/
Mode changes, applies to whichever effect is active if that effect
supports modulation) is a piece of *button/state-machine* behavior that
doesn't require I2S sampling to implement or verify — flipping a
tracked bool and Serial-reporting it lets the gesture wiring and its
persistence be exercised on real hardware now, while `SoundManager`
(actual mic sampling/AGC/beat detection) remains separate, later-
milestone work gated on the dormant tree.

**Why `ButtonGestureEngine::update()` gained a return value instead of
a new callback/observer mechanism:** the caller (`EngineeringConsole`)
already owns a `switch` for the (very small) set of possible gestures;
a `Gesture` enum return is the smallest change that lets it react
without parsing the existing `BUTTON: ...` Serial strings, and it's
purely additive — every existing Serial print from Milestone 2 is
unchanged, so nothing about the hardware-verified detection behavior
was altered, only exposed.

**What's now newly classified:** 4 completed presses previously fell
into `ButtonGestureEngine`'s "unclassified click count" default case
(explicitly out of scope for the v1.2 map). It's now a first-class
`FourPress` gesture (`BUTTON: FOUR_PRESS`), reflecting that Next Mode
is a defined, wired action as of this revision.

**Scope note:** `EffectEngine` and the `ButtonGestureEngine` changes
are standalone, hardware-decoupled, and not edits to the dormant
`AnimationManager`/`EffectRegistry`/`PaletteManager`/`ButtonManager`.
No settings persistence — effect/palette/mode selection resets to
Solid / Rainbow palette / Static mode on every reboot. No microphone
input, no `SoundManager` code. No power toggle, no factory-reset erase.

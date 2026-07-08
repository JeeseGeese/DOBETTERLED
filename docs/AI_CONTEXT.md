# DOBETTERLED — Engineering Handbook

This document is the permanent, stable reference for the DOBETTERLED firmware
project. It describes what the project *is*, how it's *built*, and the
*rules* that govern changes to it — not what any single milestone did. If
you are starting a new development session (human or AI), read this file
first.

This file intentionally contains no milestone-by-milestone history and no
open TODOs. For "what happened and when," see `CHANGELOG.md`. For "what's
left to build," ask the person driving the session — that state changes too
often to belong in a stable handbook.

---

## Project Vision

**Purpose.** DOBETTERLED is open-source ESP32 firmware for festival totems,
flow arts, LED wearables, poi, and art installations — small, single-button,
screen-less battery/mains-powered LED devices that need to look good, be
reliable in the field, and be controllable with almost no UI surface.

**Long-term goals.**
- A firmware **platform**, not a single-board sketch: one codebase that can
  target multiple physical products by supplying a new hardware config, not
  by forking or rewriting behavior code.
- A single physical button as the entire user interface, expressive enough
  to reach effect selection, palette selection, audio-reactive behavior, and
  eventually power/settings control — without ever needing a screen, app, or
  second input.
- Audio-reactive lighting that gracefully degrades: every effect must keep
  working, modulated or not, whether or not audio input is active.
- Durable, field-usable firmware: no crashes, no dark LEDs, no bricked
  boards. Reliability is a feature, not a nice-to-have.

**Design philosophy.**
- **Hardware truth over documentation.** If code and a design doc disagree,
  that's a bug in one of them to be resolved deliberately — never something
  to paper over. Verified hardware behavior is the only real source of
  truth.
- **Small, reversible steps.** Every milestone is scoped so a failure is
  cheap to detect and cheap to undo.
- **Standalone before integrated.** New behavior is built as an independent,
  testable class first; architectural reconciliation (e.g., merging into a
  larger application-coordinator layer) is deliberately deferred until the
  standalone version is proven on hardware.

---

## Current Hardware

**Product (v1): QuinLED Dig2Go**, single hardware revision, ESP32, PlatformIO
+ Arduino framework, FastLED for LED rendering.

| Subsystem | Detail |
|---|---|
| **Controller** | ESP32 (QuinLED Dig2Go), Arduino framework via PlatformIO, `esp32dev` board target |
| **LEDs** | 15× WS2812B, GRB color order, data on **GPIO16** |
| **Button** | Single physical button, **GPIO0**, active-LOW, `INPUT_PULLUP`. Also an ESP32 boot-strapping pin — see Engineering Principles for why this matters |
| **Relay** | LED power relay, **GPIO12**, HIGH = powered. Allows a true power-off (not just brightness-to-zero) |
| **Microphone** | Onboard ICS-43434 digital I2S MEMS mic. **SD (data) = GPIO19, WS (word select/LR clock) = GPIO4, SCK (bit clock) = GPIO18**, I2S port 0, 16 kHz sample rate, 32-bit word / 24-bit data. Hardware-confirmed: the mic's L/R channel select is hard-wired on the board (not software-selectable) — **`I2S_CHANNEL_FMT_ONLY_RIGHT`** is the correct setting; `LEFT` reads all-zero |
| **Reserved, unimplemented** | IR receiver (GPIO5), expansion header (SDA=21, SCL=22, GPIOA=23, GPIOB=25/ADC-capable) — pins reserved in hardware, no firmware support yet |

---

## Firmware Architecture

Two parallel trees exist in this repository. Only one is active.

### Active tree (linked, executed, the one to build on)

`main.cpp` instantiates and drives **`EngineeringConsole`** only.

| Module | Responsibility |
|---|---|
| **`main.cpp`** | Entry point. Instantiates one top-level object and calls `begin()`/`update()`. Currently `EngineeringConsole`; documented inline as a one-line swap back to `BringUpDashboard` if a build ever fails hardware acceptance. |
| **`EngineeringConsole`** | Owns *all* real hardware access: GPIO, relay, `FastLED.show()`, brightness. The application coordinator for the active tree — wires `ButtonGestureEngine`, `EffectEngine`, and `AudioInput` together, and owns the Serial command surface (diagnostics + status). Nothing else in the active tree touches hardware directly except where noted below. |
| **`ButtonGestureEngine`** | Pure gesture-detection logic. Takes a raw `bool` button reading + `millis()`, returns a `Gesture` enum (also Serial-reports each gesture directly, independent of the return value). Zero GPIO access, zero FastLED, zero dormant-tree dependency. Designed to be extractable into `ButtonManager` later without a rewrite. |
| **`EffectEngine`** | Pure effect/palette rendering. Fills a caller-provided `CRGB` buffer only — never calls `FastLED.show()`, never touches relay or button. Owns the effect list, palette list, and Mode-category grouping/navigation. Designed to be extractable into `AnimationManager`/`EffectRegistry`/`PaletteManager` later. |
| **`AudioInput`** | Owns the I2S microphone peripheral directly (an intentional exception to the "hardware-decoupled" pattern above — its entire job *is* the hardware read, so there's nothing to decouple). Produces raw sample, peak, an RMS-style smoothed average, an adaptive noise floor, and a normalized 0–255 level. Safe to fail: if I2S init fails, every accessor returns a neutral value and `isAvailable()` reports false rather than crashing. |
| **`BringUpDashboard`** | The original diagnostic firmware. Left in the repository unmodified and unreferenced — the permanent one-line rollback target if a change ever fails hardware acceptance. Do not edit this file except to deliberately update the rollback baseline itself. |

### Dormant tree (not linked, not executed, not hardware-confirmed)

Architecturally complete, matches the ownership boundaries in
`docs/ARCHITECTURE_CONTRACT.md`, but its own hardware bring-up stalled before
ever confirming it could light an LED. **Do not build new features against
this tree, and do not "fix" it as a side effect of unrelated work.** Reviving
it is its own future milestone, requiring its own hardware verification.

| Module | Intended responsibility (per `ARCHITECTURE_CONTRACT.md`) |
|---|---|
| `SystemManager` | Application coordinator — owns every manager instance, maps `ButtonEvent`s to actions, sequences boot/shutdown/factory-reset. |
| `Hal` | Hardware Abstraction Layer — the only module (besides one documented FastLED template exception) permitted to touch GPIO/I2S directly. |
| `LEDDriver` | FastLED encapsulation, pixel buffer, brightness scaling, relay control via the HAL. |
| `ButtonManager` | Gesture classification (debounce, click-count, hold-phase tracking) — reports events, decides nothing. |
| `SettingsManager` | NVS persistence with a dirty-flag/debounced-commit model. |
| `PaletteManager` | Built-in palette registry. |
| `EffectRegistry` / `AnimationManager` | Stateless effect lookup, and the stateful "current selection + per-frame invocation" layer on top of it. |
| `SoundManager` | Placeholder stub only — real mic sampling now lives in the active tree's `AudioInput`, not here. |
| `AnimationBase` | Effect interface + `AnimationContext` (LED buffer, resolved palette, settings, sound accessors) shared by all dormant-tree effects. |

**Why two trees exist:** the dormant tree's hardware bring-up stalled
mid-diagnostic (LEDs never confirmed lit), and the project pivoted to
evolving the working `BringUpDashboard` into `EngineeringConsole` instead —
which is why the active tree is a set of standalone classes rather than a
direct extension of `SystemManager`. Reconciling the two trees (either
finishing the dormant tree's bring-up and retiring the console, or folding
the console's proven behavior into `SystemManager` once trusted) remains
unscheduled future work.

---

## Engineering Principles

1. **Hardware first.** A design is only as good as its last hardware test.
   Documentation and code review cannot substitute for flashing a board.
2. **Build in small, reversible milestones.** Each one should be cheap to
   verify and cheap to revert independently of the others.
3. **One responsibility per class.** Gesture detection, effect rendering,
   and audio sampling are separate classes for a reason — each can be
   tested, reasoned about, and eventually extracted independently.
4. **Preserve working firmware.** Never let an in-progress change leave the
   board in a worse state than before you started, for longer than
   necessary to test the next step.
5. **Never break hardware-confirmed functionality without a stated reason.**
   If a change affects previously-verified behavior, that's worth calling
   out explicitly, not discovering by accident.
6. **Compile before calling anything done.** A design that hasn't been
   built by the actual toolchain is a hypothesis, not a result.
7. **Hardware verify before committing to Git.** A commit represents code
   that has been tested on real hardware, not just code that compiles —
   see Git Workflow.
8. **Diagnostic/developer access is Serial-only, permanently.** The
   physical button is reserved entirely for product features. This was
   tried the other way once (a boot-time button gesture gating a
   "Developer Mode") and it caused a real hardware failure — dark LEDs and
   a Serial crash. GPIO0 is also an ESP32 boot-strapping pin, which makes
   gating anything on it at boot doubly fragile. Do not reintroduce
   boot-time button logic.
9. **New logic is standalone and hardware-decoupled by default.** Pure
   logic classes (gesture detection, effect rendering) take inputs and
   return outputs; they don't reach into GPIO or FastLED themselves. The
   deliberate exception is a class whose entire purpose *is* a hardware
   read (e.g. `AudioInput`) — in that case it owns the peripheral directly,
   the same way `EngineeringConsole` owns GPIO/relay/FastLED.
10. **A one-line rollback must always work.** `main.cpp` should always be
    able to fall back to `BringUpDashboard` via a two-line swap.
11. **Don't guess silently on undocumented hardware facts.** Where a
    physical detail isn't recorded anywhere (e.g. which I2S channel slot a
    mic's hardwired L/R pin actually selects), say so explicitly, test
    empirically, and record the answer once it's known.

---

## Repository Workflow

```
DOBETTERLED_PlatformIO/
├── platformio.ini
├── README.md
├── CHANGELOG.md
├── include/            Shared config headers for the DORMANT tree
│   ├── Config.h            Firmware-generic timing/defaults/limits
│   ├── ProductConfig.h     Dig2Go-specific pin map & hardware facts
│   └── Types.h             Shared enums & value types
├── docs/
│   ├── PRODUCT_SPEC.md         Behavioral source of truth
│   ├── DESIGN_LOG.md           Numbered rationale entries per decision
│   ├── ARCHITECTURE_CONTRACT.md  Dormant-tree module ownership boundaries
│   ├── VS_CODE_PLATFORMIO_SETUP.md
│   └── AI_CONTEXT.md           This file
└── src/                 PlatformIO compiles everything under here
    ├── main.cpp
    ├── EngineeringConsole.h/.cpp
    ├── ButtonGestureEngine.h/.cpp
    ├── EffectEngine.h/.cpp
    ├── AudioInput.h/.cpp
    ├── BringUpDashboard.h/.cpp
    ├── SystemManager.h/.cpp, HAL/, Managers/*, Effects/*, AnimationBase.h   (dormant)
```

**Important architectural detail:** the active tree deliberately does
**not** `#include` `ProductConfig.h`/`Config.h`/`Types.h`. Those headers
exist for the dormant tree. Active-tree classes hard-code their own pin/
timing constants locally (with a comment cross-referencing the matching
`ProductConfig.h` fact), specifically to stay independent of the dormant
tree until a deliberate reconciliation milestone. Don't "clean this up" by
wiring the active tree to those headers without that being its own
proposed, reviewed change.

**When to add a new file:** new behavior gets its own class/header pair
(see Engineering Principles #3, #9), not a new method bolted onto
`EngineeringConsole`. `EngineeringConsole` wires things together; it
shouldn't grow the *logic* of every new subsystem itself.

**When to refactor:** only as its own explicit, reviewed step — not as a
side effect of an unrelated feature change. This applies doubly to the
dormant tree (Engineering Principle #4 in `ARCHITECTURE_CONTRACT.md`: don't
extend or "fix" it incidentally).

**When to create new folders:** mirror the existing pattern — a folder
groups a family of related dormant-tree modules (`Managers/`, `Effects/`,
`HAL/`). The active tree has stayed flat under `src/` by design so far,
since it's currently a small, flat set of standalone classes.

---

## Build Workflow

```
Inspect repository
        │   (read the actual files before assuming prior context is synced —
        │    docs and code have drifted before; don't repeat that mistake)
        ▼
Propose the smallest safe patch
        │   (goal, architecture/dependency/compile/hardware risk,
        │    files touched, testing checklist — wait for go-ahead)
        ▼
Implement
        ▼
Compile        (`pio run` — must succeed with no new warnings/errors
        │        attributable to the changed files)
        ▼
Upload          (`pio run -t upload`)
        ▼
Hardware verify (actually exercise the change on the board — an LED
        │        failure is a stop condition: revert to the last
        │        known-good state and report back, don't layer more
        │        changes on top of a broken build)
        ▼
Git commit      (only after hardware verification passes — see Git Workflow)
        ▼
Documentation update  (CHANGELOG always; README/PRODUCT_SPEC/DESIGN_LOG
                        when button/audio/architecture behavior changed —
                        only when explicitly requested for a given session)
```

---

## Button Map

Single physical button, software-debounced, edge- and duration-based
gesture classification (`ButtonGestureEngine`). This is the **current,
hardware-confirmed** map — see `docs/PRODUCT_SPEC.md` Section 3 for full
gesture-definition detail and revision history.

| Gesture | Action | Status |
|---|---|---|
| 1 Press | Next Effect (within current Mode category) | Hardware-confirmed |
| 2 Presses | Previous Effect (within current Mode category) | Hardware-confirmed |
| 3 Presses | Next Palette | Hardware-confirmed |
| 4 Presses | Next Mode (Static → Motion → Reactive → wraps) | Hardware-confirmed |
| 2 Presses + Hold | Toggle Audio Reactive Overlay flag | Hardware-confirmed (flag only — not yet connected to any LED behavior) |
| Long Hold (2–3s, 1st press only) | Quick Settings Menu candidate | Detected/reported only, no action |
| 6 Presses | Power On/Off candidate | Detected/reported only, no action |
| 10 Presses | Factory Reset candidate ("pending" report only) | Detected/reported only, no erase |

Serial command surface (always available, no gesture ever gates it):
`1`–`9` (diagnostic modes), `+`/`-` (brightness), `m` (menu), `s` (status),
`a` (one-shot audio diagnostics), `A` (toggle continuous audio diagnostics).

---

## Effect System

Four concepts, each an independent axis:

- **Effect** — the actual rendering algorithm (`Solid`, `Rainbow`,
  `Confetti`, `Sparkle`, `Chase`, `Fire`). Owned by `EffectEngine`.
- **Palette** — the color source an effect draws from (`Rainbow`, `Party`,
  `Ocean`, `Fire` — FastLED's built-in gradient palettes). Not every effect
  consumes it: `Rainbow` (the effect) is a full-spectrum sweep and
  intentionally ignores palette; `Fire` uses the *current* palette instead
  of a fixed heat gradient, so palette-cycling reskins the flame.
- **Mode (category)** — a grouping over the effect list that scopes 1/2-
  press navigation: **Static** (Solid), **Motion** (Rainbow, Chase),
  **Reactive** (Confetti, Sparkle, Fire). 4-press switches category and
  selects that category's first effect. *"Reactive" here names a visual
  character (organic/randomized), and is unrelated to the axis below —
  they share a word, not a meaning.*
- **Audio Overlay** — a single global on/off flag, not a separate effect
  and not a fifth axis of *what's showing*. It's a modifier: whichever
  effect/palette/mode is currently active keeps rendering exactly as it
  would otherwise; when audio input exists and the flag is on, that
  effect's parameters get modulated by sound wherever it supports that.
  Turning the Overlay on/off never changes effect, palette, or mode
  selection, and must never block 1/2/3/4-press navigation.

`EffectEngine::render()` only computes pixel colors into a caller-provided
buffer — `EngineeringConsole` owns `FastLED.show()`, brightness, and relay.

---

## Audio Architecture

**Current state:** `AudioInput` reliably reads the onboard mic and exposes
raw sample, peak, a smoothed RMS-style average, an adaptive noise floor,
and a normalized 0–255 level. This is hardware-confirmed. Nothing consumes
these values yet — the Audio Reactive Overlay flag (toggled by Double
Press + Hold) is currently just a tracked boolean with no connection to
`AudioInput` or `EffectEngine`.

**Planned overlay model** (design intent, not yet implemented):
- The Overlay is purely a *modifier*, never a selector — see Effect System
  above. Turning it on must never change what effect/palette/mode is
  active.
- **Graceful degradation is a hard requirement.** An effect with no
  defined audio behavior must keep rendering normally, unmodulated, when
  the Overlay is on — never crash, hang, glitch, or blank. The mechanism
  for this (e.g. a default no-op modulation hook every effect gets for
  free) is a design decision for whichever milestone implements it, not
  decided here.
- Each effect that *does* support modulation will define its own mapping
  from the normalized audio level to its own parameters (brightness,
  speed, density, intensity) — the mapping is effect-specific, decided
  when that effect's modulation is built, not standardized in advance.
- Turning the Overlay off stops audio-driven behavior; whether it also
  stops sampling the mic entirely (for power/CPU) is an implementation
  detail for that future milestone.

This document deliberately does not describe FFT, beat detection, or any
specific per-effect modulation formula — those are implementation details
of a not-yet-built milestone, not stable architecture.

---

## Documentation Rules

| Document | Purpose | Update when |
|---|---|---|
| `README.md` | Project orientation for a new reader: what's active vs. dormant, repo structure, how to build | Any milestone that changes what's active/dormant, or the file map |
| `CHANGELOG.md` | Dated, per-milestone record of what changed and why, newest first | Every milestone, before considering it done (unless a given session explicitly defers doc updates) |
| `docs/PRODUCT_SPEC.md` | Behavioral source of truth — what the firmware is supposed to do, independent of how it's implemented. Has its own "BRoadmap" version counter for button/audio design changes specifically | Any change to button gesture design or audio-mode behavior (bump BRoadmap) |
| `docs/DESIGN_LOG.md` | Numbered, permanent rationale entries — *why* a non-trivial decision was made, not just what it was | Any non-trivial architecture or design decision; tag `[BRoadmap vX.Y]` entries when they touch button/audio design |
| `docs/ARCHITECTURE_CONTRACT.md` | Ownership boundaries for the dormant tree specifically | Only if the dormant tree's intended design changes (rare — that tree isn't being actively developed) |
| `docs/AI_CONTEXT.md` | This file — stable engineering handbook | Only when something *structurally* stable changes: a new active-tree subsystem, a new engineering principle, a hardware fact correction, a newly hardware-confirmed milestone graduating into the stable map. Not for milestone TODOs or temporary implementation notes — those belong in conversation/CHANGELOG, not here |

**General rule:** if code and a document disagree, that's a bug in one of
them, not something to leave unresolved. This project has been bitten by
doc/repo drift before (documentation describing work that was never
actually implemented, and a GitHub remote that went unpushed for multiple
milestones) — treat any discovered drift as worth fixing, not narrating
around.

---

## Git Workflow

- **Remote:** `https://github.com/JeeseGeese/DOBETTERLED` (branch `main`).
- **Branch strategy:** linear history on `main`. No feature-branch
  convention has been established yet — commits land directly on `main`
  once hardware-verified.
- **Commit philosophy:**
  - **Hardware-verify before committing.** A commit should represent code
    that was actually tested on the board, not just code that compiled.
  - Prefer a new commit over amending an existing one, even for a same-day
    follow-up fix — history should show what was tried and corrected, not
    be rewritten to look like it was right the first time.
  - Commit messages explain *what* changed and, importantly, *why* —
    especially for hardware-calibration fixes, where the "why" (e.g. "read
    all-zero on LEFT channel, RIGHT produced real data") is the part worth
    preserving.
  - Never force-push, never rewrite already-pushed history, without
    explicit request.
- **Tagging:** not yet a formal cadence. One early tag (`v0.1.0-alpha`)
  exists from before this handbook; establishing a real release/tagging
  policy is unscheduled future work.
- **Release cadence:** none formalized — this is pre-release, milestone-
  driven firmware, not a versioned product yet.

---

## Coding Standards

- **Naming:** classes `PascalCase`; member variables `m_camelCase`; public
  and private methods `camelCase`; enums are `enum class` with `PascalCase`
  type and value names; file-local constants in an anonymous namespace are
  `UPPER_SNAKE_CASE`.
- **Class responsibilities:** one class, one job (see Engineering
  Principles #3). If a class needs a comment explaining two unrelated
  things it's responsible for, it's a sign it should split.
- **Header organization:** every class gets its own `.h`/`.cpp` pair. The
  header carries a substantial top-of-file comment block explaining the
  class's purpose, its milestone context, and — most importantly — the
  *design rationale* for non-obvious choices (why it's standalone, why it
  owns hardware directly or doesn't, what it's explicitly not responsible
  for).
- **Comments explain why, not what.** Inline comments are reserved for
  non-obvious constraints, workarounds, or rationale — not restating what
  the next line of code already says.
- **No magic numbers.** Numeric constants are named `constexpr` values,
  ideally with units in the name (`_MS`, `_HZ`, etc.), grouped in an
  anonymous namespace at the top of the `.cpp` file.
- **Const correctness:** accessor methods are marked `const`; parameters
  that are read-only are passed accordingly.
- **No speculative complexity.** Don't add configurability, abstraction, or
  error handling for scenarios the hardware/firmware can't actually
  produce. Three similar lines beat a premature abstraction.

---

## Known Stable Milestones

Hardware-confirmed only. Anything not listed here — including audio-
reactive effect modulation, power toggle, factory reset, settings
persistence, or dormant-tree revival — is future, unconfirmed work and
intentionally not detailed in this handbook (see the top of this file).

| Milestone | What it delivered | Status |
|---|---|---|
| **1 — Engineering Console baseline** | `EngineeringConsole` active build: relay/LED/button-test diagnostics, Serial commands `1`–`9`/`+`/`-`/`m`/`s` | Hardware-confirmed |
| **2 — Button Gesture Engine** | `ButtonGestureEngine`: full click-count/hold gesture detection, Serial-reported | Hardware-confirmed |
| **3 — Effect Engine** | `EffectEngine`: 6 effects, 4 palettes, 3 Mode categories; 1/2/3/4-press gestures wired to real effect/palette/mode navigation | Hardware-confirmed |
| **4A — Audio Input bring-up** | `AudioInput`: I2S mic read (RIGHT channel), raw/peak/average/noise-floor/normalized level, `a`/`A` Serial diagnostics | Hardware-confirmed |

---

*This handbook is reviewed and revised as the project's stable shape
changes — not appended to as a running log. Keep it lean.*

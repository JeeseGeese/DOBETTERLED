/**
 * AnimationBase.h
 * -----------------------------------------------------------------------
 * Defines the contract every effect implements, and the read-only
 * context bundle passed into it each frame.
 *
 * Effects know NOTHING about buttons, settings persistence, or which
 * effect ran before them. Everything they need arrives through
 * AnimationContext -- pixel output, palette lookup, sound data, and
 * the current EffectSettings (speed/intensity/palette/primary/secondary
 * color). An effect never reaches into SettingsManager, ButtonManager,
 * or Hal directly.
 * -----------------------------------------------------------------------
 */

#pragma once

#include "Types.h" // EffectSettings
#include <stdint.h>

class LEDDriver;
class PaletteManager;
class SoundManager; // forward-declared; SoundManager itself is a later
                     // milestone, but AnimationContext reserves its slot
                     // now so effects and AnimationManager won't need
                     // another interface change once it's implemented.

// Read-only bundle handed to every effect's update() call. Effects
// render through `ledDriver` and `paletteManager`, and may read
// `soundManager`'s accessors (which return neutral/static values while
// AudioMode::Disabled, per spec Section 10 -- effects never need to
// branch on whether audio is active).
struct AnimationContext
{
    LEDDriver& ledDriver;
    PaletteManager& paletteManager;
    SoundManager& soundManager;
    const EffectSettings& settings;

    // Milliseconds elapsed since this effect's last update() call.
    // Effects use this (not millis() directly) so their motion stays
    // frame-rate-independent and so a future effect could be tested
    // with a synthetic clock if needed.
    uint32_t deltaMs;
};

class Animation
{
public:
    virtual ~Animation() = default;

    // Called once when this effect becomes the active one (including
    // when returning to a previously-active effect). Default is a
    // no-op -- most effects don't need explicit setup beyond what
    // their constructor already does.
    virtual void begin(const AnimationContext& context) { (void)context; }

    // Called once per frame while this effect is active. This is the
    // only method every effect must actually implement.
    virtual void update(const AnimationContext& context) = 0;

    // Returns the effect to its initial state without going through a
    // full begin() again (e.g. if AnimationManager wants to restart it
    // cleanly). Default is a no-op -- stateless effects don't need it.
    virtual void reset() {}
};

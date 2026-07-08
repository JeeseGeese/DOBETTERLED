/**
 * Types.h
 * -----------------------------------------------------------------------
 * Shared enums and small value types used across multiple modules.
 *
 * Kept separate from any single manager so that, for example,
 * AnimationManager.h and ButtonManager.h can both reference ButtonEvent
 * or AnimationId without creating a circular include between them.
 *
 * Deliberately FastLED-free: RgbColor is our own plain color type, not
 * FastLED's CRGB. LEDDriver.cpp is the only file that ever converts
 * between the two -- see the note on RgbColor below.
 * -----------------------------------------------------------------------
 */

#pragma once

#include <stdint.h>

// =========================================================================
// Button Events
// -------------------------------------------------------------------------
// Emitted by ButtonManager::update(). SystemManager is the only consumer
// that maps these to concrete actions -- ButtonManager itself never
// calls into AnimationManager, SettingsManager, etc.
// =========================================================================

enum class ButtonEvent : uint8_t
{
    None = 0,

    SingleClick,            // -> next animation
    DoubleClick,             // -> previous animation
    TripleClick,             // -> next color palette
    QuadClick,               // -> cycle audio mode (Disabled -> Overlay -> Dedicated -> ...)

    // Long-press is reported as three phases rather than one classified
    // event. ButtonManager only reports what's physically happening;
    // SystemManager decides what a given hold duration MEANS (toggle
    // on/off vs. brightness-ramp-then-stop) using the duration accessors
    // on ButtonManager, cross-referenced with Config::Timing thresholds.
    LongPressStart,          // hold has crossed the long-press threshold
    LongPressHold,           // still held; check ButtonManager accessors
                             // (getCurrentHoldDurationMs, isBrightnessRampActive,
                             // consumeBrightnessRampDelta, isFactoryResetWarningActive)
    LongPressRelease,        // released after a qualifying long press;
                             // check getLastHoldDurationMs() to classify

    // Fires once, automatically, the moment a continuous hold reaches
    // the full factory-reset duration -- does not require release.
    FactoryResetRequested,
};

// =========================================================================
// Power State
// =========================================================================

enum class PowerState : uint8_t
{
    Off = 0,
    On  = 1,
};

// =========================================================================
// Audio Mode
// -------------------------------------------------------------------------
// 3-state, independent of which effect is currently selected. See spec
// Section 10. Quad Press cycles Disabled -> Overlay -> Dedicated -> ...
// =========================================================================

enum class AudioMode : uint8_t
{
    Disabled = 0,       // mic not sampled, nothing modulated by sound
    Overlay,            // mic sampled; current effect modulated by sound
    Dedicated,          // mic sampled; SoundReactive effect family shown

    Count // sentinel -- number of modes, not a real one
};

// =========================================================================
// Animation Identifiers
// -------------------------------------------------------------------------
// Stable numeric IDs for persistence (stored in NVS). Append new effects
// at the end; do not reorder existing values, or saved settings on
// devices in the field will point to the wrong effect after an update.
// =========================================================================

enum class AnimationId : uint8_t
{
    Solid = 0,
    Rainbow,
    Fire,
    Twinkle,
    Confetti,
    Plasma,
    Chase,
    Sparkle,
    SoundReactive,

    Count // sentinel -- number of registered animations, not a real one
};

// =========================================================================
// Palette Identifiers
// -------------------------------------------------------------------------
// Same stability rule as AnimationId: append only, never reorder.
// =========================================================================

enum class PaletteId : uint8_t
{
    Rainbow = 0,
    Ocean,
    Forest,
    Lava,
    Cloud,
    Sunset,
    Ice,
    Neon,
    Pastel,
    Halloween,
    Christmas,

    Count // sentinel
};

// =========================================================================
// RgbColor
// -------------------------------------------------------------------------
// Plain, FastLED-independent color value. Used anywhere a color needs to
// travel outside LEDDriver -- EffectSettings, SettingsManager persistence,
// AnimationContext. LEDDriver.cpp converts RgbColor <-> CRGB internally;
// no other file needs to know CRGB exists.
// =========================================================================

struct RgbColor
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    constexpr RgbColor() = default;
    constexpr RgbColor(uint8_t red, uint8_t green, uint8_t blue)
        : r(red), g(green), b(blue) {}
};

// =========================================================================
// EffectSettings
// -------------------------------------------------------------------------
// The shared parameter set every effect is driven by (spec Section 6).
// Owned/persisted by SettingsManager, delivered to effects as part of
// AnimationContext. Not every effect consumes every field -- each
// effect's header documents which fields it actually uses.
// =========================================================================

struct EffectSettings
{
    uint8_t speed     = 128; // 0-255
    uint8_t intensity = 128; // 0-255

    PaletteId paletteId = PaletteId::Rainbow;

    RgbColor primaryColor;   // explicit user color, independent of palette
    RgbColor secondaryColor; // explicit second color (e.g. background/accent)
};

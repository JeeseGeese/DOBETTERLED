/**
 * PaletteManager.h
 * -----------------------------------------------------------------------
 * Owns built-in palette definitions, color lookup within a palette, and
 * palette cycling order. Nothing more.
 *
 * Strict boundaries: PaletteManager has NO knowledge of button events,
 * LED hardware, settings persistence, animation selection, or audio
 * mode. It is a stateless service -- it holds no "current palette"
 * itself (that's SettingsManager's job); every method takes whatever
 * context it needs as a parameter and returns an answer.
 *
 * FastLED isolation: PaletteManager does NOT use FastLED's palette
 * types (CRGBPalette16, ColorFromPalette, etc.) -- palettes are defined
 * and interpolated here using plain RgbColor (Types.h) only, so this
 * module stays as FastLED-free as everything except LEDDriver.
 * -----------------------------------------------------------------------
 */

#pragma once

#include "Types.h" // PaletteId, RgbColor
#include <stdint.h>

class PaletteManager
{
public:
    // Returns the interpolated color at `position` (0-255) within the
    // given palette's gradient. Effects call this once per pixel (or
    // per frame, per their own logic) to get a smoothly blended color
    // anywhere along the palette.
    RgbColor getColorAt(PaletteId id, uint8_t position) const;

    // Returns the palette that follows `current` in cycle order,
    // wrapping from the last palette back to the first. Stateless --
    // callers (SystemManager, via SettingsManager) own which palette is
    // "current"; this only answers "what comes after X."
    PaletteId getNextPaletteId(PaletteId current) const;

    // Human-readable name, for future debug output / serial menu / OLED
    // use. Not used by any milestone yet, but costs nothing to expose
    // now since it's part of a palette's definition either way.
    const char* getPaletteName(PaletteId id) const;

    // Linearly interpolates between two explicit colors. ratio=0 is
    // fully `a`, ratio=255 is fully `b`. Exposed as a shared utility so
    // effects blending between EffectSettings::primaryColor and
    // secondaryColor (rather than a named palette) reuse the same
    // interpolation math instead of duplicating it per-effect.
    static RgbColor blend(const RgbColor& a, const RgbColor& b, uint8_t ratio);
};

#pragma once

/**
 * EffectEngine.h
 * -----------------------------------------------------------------------
 * Milestone 3: standalone effect + palette rendering.
 *
 * Fills a caller-provided CRGB buffer only. Never calls FastLED.show(),
 * never touches the relay or button pin, never includes any dormant-tree
 * header (SystemManager/AnimationManager/EffectRegistry/PaletteManager).
 * EngineeringConsole owns FastLED.show(), brightness, and relay -- this
 * class only computes pixel colors. Explicitly designed to be
 * extractable into AnimationManager/EffectRegistry/PaletteManager later,
 * the same pattern ButtonGestureEngine already established for
 * ButtonManager.
 *
 * Mode categories (BRoadmap v1.3) group the effect list so 1/2-press
 * navigation stays within the current category, and 4-press switches
 * category:
 *   Static:   Solid
 *   Motion:   Rainbow, Chase
 *   Reactive: Confetti, Sparkle, Fire
 * ("Reactive" here is a visual-character category name -- unrelated to
 * the Audio Reactive Overlay toggle, which this class does not own; see
 * EngineeringConsole for that flag.)
 * -----------------------------------------------------------------------
 */

#include <Arduino.h>
#include <FastLED.h>

class EffectEngine
{
public:
    enum class Effect : uint8_t
    {
        Solid,
        Rainbow,
        Confetti,
        Sparkle,
        Chase,
        Fire
    };

    enum class Palette : uint8_t
    {
        Rainbow,
        Party,
        Ocean,
        Fire
    };

    enum class Mode : uint8_t
    {
        Static,
        Motion,
        Reactive
    };

    void begin();

    // Renders the current effect/palette into `buffer` (numLeds entries).
    // Never calls FastLED.show(). Buffer contents are read as well as
    // written for effects that fade/decay in place (Confetti, Sparkle) --
    // the caller must pass the same persistent buffer every frame.
    void render(CRGB* buffer, uint16_t numLeds);

    // Cycle within the current Mode category only.
    void nextEffect();
    void previousEffect();

    // Cycle across all palettes regardless of Mode/effect.
    void nextPalette();

    // Switch category: Static -> Motion -> Reactive -> wraps. Selects
    // that category's first effect.
    void nextMode();

    const char* currentEffectName() const;
    const char* currentPaletteName() const;
    const char* currentModeName() const;

private:
    static constexpr uint16_t MAX_LEDS = 64;

    Effect m_effect = Effect::Solid;
    Palette m_palette = Palette::Rainbow;
    Mode m_mode = Mode::Static;

    uint8_t m_hue = 0;
    uint8_t m_chaseIndex = 0;
    uint8_t m_heat[MAX_LEDS] = {0};
    bool m_warnedClamped = false;

    CRGBPalette16 resolvePalette() const;

    void renderSolid(CRGB* buffer, uint16_t n);
    void renderRainbow(CRGB* buffer, uint16_t n);
    void renderConfetti(CRGB* buffer, uint16_t n);
    void renderSparkle(CRGB* buffer, uint16_t n);
    void renderChase(CRGB* buffer, uint16_t n);
    void renderFire(CRGB* buffer, uint16_t n);
};

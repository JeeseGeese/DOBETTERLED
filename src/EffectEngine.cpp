#include "EffectEngine.h"

namespace
{
    constexpr EffectEngine::Effect kStaticEffects[]   = { EffectEngine::Effect::Solid };
    constexpr EffectEngine::Effect kMotionEffects[]   = { EffectEngine::Effect::Rainbow, EffectEngine::Effect::Chase };
    constexpr EffectEngine::Effect kReactiveEffects[] = { EffectEngine::Effect::Confetti, EffectEngine::Effect::Sparkle, EffectEngine::Effect::Fire };

    // Milestone 4B: audio-reactive modulation ranges. Each is a simple
    // linear map from audioLevel (0-255) added on top of the effect's
    // original baseline constant -- deliberately not a calibrated
    // response curve, just enough to make Audio Overlay ON visibly
    // different from OFF for beta field testing. audioActive == false
    // always uses the *_BASE value alone, which is the exact constant
    // each effect used before this milestone.
    constexpr uint8_t SOLID_BRIGHTNESS_FLOOR   = 96;  // never fully dark while pulsing
    constexpr uint8_t SOLID_BRIGHTNESS_RANGE   = 159; // floor + range == 255

    constexpr uint8_t RAINBOW_HUE_STEP_BASE    = 1;   // matches original ++m_hue
    constexpr uint8_t RAINBOW_HUE_STEP_RANGE   = 6;
    constexpr uint8_t RAINBOW_DELTA_HUE_BASE   = 7;   // matches original fill_rainbow(...,7)
    constexpr uint8_t RAINBOW_DELTA_HUE_RANGE  = 20;

    constexpr uint8_t CONFETTI_SPARK_CHANCE_BASE  = 80; // matches original random8() < 80
    constexpr uint8_t CONFETTI_SPARK_CHANCE_RANGE = 120;

    constexpr uint8_t SPARKLE_CHANCE_BASE      = 60; // matches original random8() < 60
    constexpr uint8_t SPARKLE_CHANCE_RANGE     = 140;

    constexpr uint8_t CHASE_BRIGHTNESS_FLOOR   = 120;
    constexpr uint8_t CHASE_BRIGHTNESS_RANGE   = 135;

    constexpr uint8_t FIRE_SPARKING_BASE       = 120; // matches original local SPARKING
    constexpr uint8_t FIRE_SPARKING_RANGE      = 100;

    const EffectEngine::Effect* categoryEffects(EffectEngine::Mode mode, uint8_t& countOut)
    {
        switch (mode)
        {
            case EffectEngine::Mode::Static:
                countOut = sizeof(kStaticEffects) / sizeof(kStaticEffects[0]);
                return kStaticEffects;
            case EffectEngine::Mode::Motion:
                countOut = sizeof(kMotionEffects) / sizeof(kMotionEffects[0]);
                return kMotionEffects;
            case EffectEngine::Mode::Reactive:
                countOut = sizeof(kReactiveEffects) / sizeof(kReactiveEffects[0]);
                return kReactiveEffects;
        }
        countOut = 0;
        return nullptr;
    }
}

void EffectEngine::begin()
{
    m_effect = Effect::Solid;
    m_palette = Palette::Rainbow;
    m_mode = Mode::Static;
    m_hue = 0;
    m_chaseIndex = 0;
    m_warnedClamped = false;
    for (uint16_t i = 0; i < MAX_LEDS; ++i)
    {
        m_heat[i] = 0;
    }
}

void EffectEngine::render(CRGB* buffer, uint16_t numLeds, bool audioActive, uint8_t audioLevel)
{
    uint16_t n = numLeds;
    if (n > MAX_LEDS)
    {
        if (!m_warnedClamped)
        {
            Serial.println("EffectEngine: WARNING -- LED count exceeds MAX_LEDS, clamping render.");
            m_warnedClamped = true;
        }
        n = MAX_LEDS;
    }

    switch (m_effect)
    {
        case Effect::Solid:    renderSolid(buffer, n, audioActive, audioLevel);    break;
        case Effect::Rainbow:  renderRainbow(buffer, n, audioActive, audioLevel);  break;
        case Effect::Confetti: renderConfetti(buffer, n, audioActive, audioLevel); break;
        case Effect::Sparkle:  renderSparkle(buffer, n, audioActive, audioLevel);  break;
        case Effect::Chase:    renderChase(buffer, n, audioActive, audioLevel);    break;
        case Effect::Fire:     renderFire(buffer, n, audioActive, audioLevel);     break;
    }
}

CRGBPalette16 EffectEngine::resolvePalette() const
{
    switch (m_palette)
    {
        case Palette::Rainbow: return RainbowColors_p;
        case Palette::Party:   return PartyColors_p;
        case Palette::Ocean:   return OceanColors_p;
        case Palette::Fire:    return HeatColors_p;
    }
    return RainbowColors_p;
}

void EffectEngine::renderSolid(CRGB* buffer, uint16_t n, bool audioActive, uint8_t audioLevel)
{
    // Audio Overlay ON: pulse brightness between a floor and full scale
    // with the current audio level. OFF: identical to pre-4B (solid
    // full-brightness fill).
    const uint8_t brightness = audioActive
        ? qadd8(SOLID_BRIGHTNESS_FLOOR, scale8(audioLevel, SOLID_BRIGHTNESS_RANGE))
        : 255;
    fill_solid(buffer, n, ColorFromPalette(resolvePalette(), 0, brightness, LINEARBLEND));
}

void EffectEngine::renderRainbow(CRGB* buffer, uint16_t n, bool audioActive, uint8_t audioLevel)
{
    // Palette-independent full-spectrum sweep -- deliberately ignores
    // the active palette (see header comment).
    //
    // Audio Overlay ON: faster hue advance (speed) and wider hue spread
    // across the strip (intensity) with the current audio level. OFF:
    // identical to pre-4B (hueStep=1, deltaHue=7).
    const uint8_t hueStep = audioActive
        ? static_cast<uint8_t>(RAINBOW_HUE_STEP_BASE + scale8(audioLevel, RAINBOW_HUE_STEP_RANGE))
        : RAINBOW_HUE_STEP_BASE;
    const uint8_t deltaHue = audioActive
        ? static_cast<uint8_t>(RAINBOW_DELTA_HUE_BASE + scale8(audioLevel, RAINBOW_DELTA_HUE_RANGE))
        : RAINBOW_DELTA_HUE_BASE;

    fill_rainbow(buffer, n, m_hue, deltaHue);
    m_hue += hueStep;
}

void EffectEngine::renderConfetti(CRGB* buffer, uint16_t n, bool audioActive, uint8_t audioLevel)
{
    fadeToBlackBy(buffer, n, 10);

    // Audio Overlay ON: raise spark spawn chance (density) with level.
    // OFF: identical to pre-4B (chance == 80/255).
    const uint8_t sparkChance = audioActive
        ? qadd8(CONFETTI_SPARK_CHANCE_BASE, scale8(audioLevel, CONFETTI_SPARK_CHANCE_RANGE))
        : CONFETTI_SPARK_CHANCE_BASE;

    if (random8() < sparkChance)
    {
        uint16_t pos = random16(n);
        buffer[pos] += ColorFromPalette(resolvePalette(), random8(), 255, LINEARBLEND);
    }
}

void EffectEngine::renderSparkle(CRGB* buffer, uint16_t n, bool audioActive, uint8_t audioLevel)
{
    fadeToBlackBy(buffer, n, 40);

    // Audio Overlay ON: raise flash spawn chance (density) with level.
    // OFF: identical to pre-4B (chance == 60/255).
    const uint8_t sparkChance = audioActive
        ? qadd8(SPARKLE_CHANCE_BASE, scale8(audioLevel, SPARKLE_CHANCE_RANGE))
        : SPARKLE_CHANCE_BASE;

    if (random8() < sparkChance)
    {
        uint16_t pos = random16(n);
        buffer[pos] = ColorFromPalette(resolvePalette(), random8(), 255, LINEARBLEND);
    }
}

void EffectEngine::renderChase(CRGB* buffer, uint16_t n, bool audioActive, uint8_t audioLevel)
{
    fill_solid(buffer, n, CRGB::Black);
    if (n > 0)
    {
        // Audio Overlay ON: pulse the moving pixel's brightness with
        // level (speed is left alone -- on a 15-LED strip, skipping
        // steps to go "faster" reads as flicker, not motion). OFF:
        // identical to pre-4B (brightness == 255).
        const uint8_t brightness = audioActive
            ? qadd8(CHASE_BRIGHTNESS_FLOOR, scale8(audioLevel, CHASE_BRIGHTNESS_RANGE))
            : 255;
        buffer[m_chaseIndex % n] = ColorFromPalette(resolvePalette(), 0, brightness, LINEARBLEND);
        m_chaseIndex = static_cast<uint8_t>((m_chaseIndex + 1) % n);
    }
}

void EffectEngine::renderFire(CRGB* buffer, uint16_t n, bool audioActive, uint8_t audioLevel)
{
    // Classic Fire2012 heat-diffusion technique, adapted to read color
    // from the *current palette* instead of a fixed heat gradient, so
    // palette-cycling reskins the flame (deliberate -- see header).
    if (n == 0)
    {
        return;
    }

    constexpr uint8_t COOLING = 55;

    // Audio Overlay ON: raise ignition chance (flame intensity) with
    // level. OFF: identical to pre-4B (sparking == 120/255).
    const uint8_t sparking = audioActive
        ? qadd8(FIRE_SPARKING_BASE, scale8(audioLevel, FIRE_SPARKING_RANGE))
        : FIRE_SPARKING_BASE;

    for (uint16_t i = 0; i < n; ++i)
    {
        uint8_t cooldown = random8(0, static_cast<uint8_t>(((COOLING * 10) / n) + 2));
        m_heat[i] = (cooldown >= m_heat[i]) ? 0 : (m_heat[i] - cooldown);
    }

    for (uint16_t k = n - 1; k >= 2; --k)
    {
        m_heat[k] = (m_heat[k - 1] + m_heat[k - 2] + m_heat[k - 2]) / 3;
    }

    if (random8() < sparking)
    {
        uint16_t y = random8(7);
        if (y < n)
        {
            m_heat[y] = qadd8(m_heat[y], random8(160, 255));
        }
    }

    CRGBPalette16 pal = resolvePalette();
    for (uint16_t j = 0; j < n; ++j)
    {
        buffer[j] = ColorFromPalette(pal, scale8(m_heat[j], 240), 255, LINEARBLEND);
    }
}

void EffectEngine::nextEffect()
{
    uint8_t count = 0;
    const Effect* list = categoryEffects(m_mode, count);
    if (!list || count == 0)
    {
        return;
    }

    uint8_t idx = 0;
    for (uint8_t i = 0; i < count; ++i)
    {
        if (list[i] == m_effect) { idx = i; break; }
    }
    idx = static_cast<uint8_t>((idx + 1) % count);
    m_effect = list[idx];
}

void EffectEngine::previousEffect()
{
    uint8_t count = 0;
    const Effect* list = categoryEffects(m_mode, count);
    if (!list || count == 0)
    {
        return;
    }

    uint8_t idx = 0;
    for (uint8_t i = 0; i < count; ++i)
    {
        if (list[i] == m_effect) { idx = i; break; }
    }
    idx = static_cast<uint8_t>((idx == 0) ? (count - 1) : (idx - 1));
    m_effect = list[idx];
}

void EffectEngine::nextPalette()
{
    m_palette = static_cast<Palette>((static_cast<uint8_t>(m_palette) + 1) % 4);
}

void EffectEngine::nextMode()
{
    m_mode = static_cast<Mode>((static_cast<uint8_t>(m_mode) + 1) % 3);

    uint8_t count = 0;
    const Effect* list = categoryEffects(m_mode, count);
    if (list && count > 0)
    {
        m_effect = list[0];
    }
}

const char* EffectEngine::currentEffectName() const
{
    switch (m_effect)
    {
        case Effect::Solid:    return "Solid";
        case Effect::Rainbow:  return "Rainbow";
        case Effect::Confetti: return "Confetti";
        case Effect::Sparkle:  return "Sparkle";
        case Effect::Chase:    return "Chase";
        case Effect::Fire:     return "Fire";
    }
    return "Unknown";
}

const char* EffectEngine::currentPaletteName() const
{
    switch (m_palette)
    {
        case Palette::Rainbow: return "Rainbow";
        case Palette::Party:   return "Party";
        case Palette::Ocean:   return "Ocean";
        case Palette::Fire:    return "Fire";
    }
    return "Unknown";
}

const char* EffectEngine::currentModeName() const
{
    switch (m_mode)
    {
        case Mode::Static:   return "Static";
        case Mode::Motion:   return "Motion";
        case Mode::Reactive: return "Reactive";
    }
    return "Unknown";
}

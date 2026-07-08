#include "EffectEngine.h"

namespace
{
    constexpr EffectEngine::Effect kStaticEffects[]   = { EffectEngine::Effect::Solid };
    constexpr EffectEngine::Effect kMotionEffects[]   = { EffectEngine::Effect::Rainbow, EffectEngine::Effect::Chase };
    constexpr EffectEngine::Effect kReactiveEffects[] = { EffectEngine::Effect::Confetti, EffectEngine::Effect::Sparkle, EffectEngine::Effect::Fire };

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

void EffectEngine::render(CRGB* buffer, uint16_t numLeds)
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
        case Effect::Solid:    renderSolid(buffer, n);    break;
        case Effect::Rainbow:  renderRainbow(buffer, n);  break;
        case Effect::Confetti: renderConfetti(buffer, n); break;
        case Effect::Sparkle:  renderSparkle(buffer, n);  break;
        case Effect::Chase:    renderChase(buffer, n);    break;
        case Effect::Fire:     renderFire(buffer, n);     break;
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

void EffectEngine::renderSolid(CRGB* buffer, uint16_t n)
{
    fill_solid(buffer, n, ColorFromPalette(resolvePalette(), 0, 255, LINEARBLEND));
}

void EffectEngine::renderRainbow(CRGB* buffer, uint16_t n)
{
    // Palette-independent full-spectrum sweep -- deliberately ignores
    // the active palette (see header comment).
    fill_rainbow(buffer, n, m_hue, 7);
    ++m_hue;
}

void EffectEngine::renderConfetti(CRGB* buffer, uint16_t n)
{
    fadeToBlackBy(buffer, n, 10);
    if (random8() < 80)
    {
        uint16_t pos = random16(n);
        buffer[pos] += ColorFromPalette(resolvePalette(), random8(), 255, LINEARBLEND);
    }
}

void EffectEngine::renderSparkle(CRGB* buffer, uint16_t n)
{
    fadeToBlackBy(buffer, n, 40);
    if (random8() < 60)
    {
        uint16_t pos = random16(n);
        buffer[pos] = ColorFromPalette(resolvePalette(), random8(), 255, LINEARBLEND);
    }
}

void EffectEngine::renderChase(CRGB* buffer, uint16_t n)
{
    fill_solid(buffer, n, CRGB::Black);
    if (n > 0)
    {
        buffer[m_chaseIndex % n] = ColorFromPalette(resolvePalette(), 0, 255, LINEARBLEND);
        m_chaseIndex = static_cast<uint8_t>((m_chaseIndex + 1) % n);
    }
}

void EffectEngine::renderFire(CRGB* buffer, uint16_t n)
{
    // Classic Fire2012 heat-diffusion technique, adapted to read color
    // from the *current palette* instead of a fixed heat gradient, so
    // palette-cycling reskins the flame (deliberate -- see header).
    if (n == 0)
    {
        return;
    }

    constexpr uint8_t COOLING = 55;
    constexpr uint8_t SPARKING = 120;

    for (uint16_t i = 0; i < n; ++i)
    {
        uint8_t cooldown = random8(0, static_cast<uint8_t>(((COOLING * 10) / n) + 2));
        m_heat[i] = (cooldown >= m_heat[i]) ? 0 : (m_heat[i] - cooldown);
    }

    for (uint16_t k = n - 1; k >= 2; --k)
    {
        m_heat[k] = (m_heat[k - 1] + m_heat[k - 2] + m_heat[k - 2]) / 3;
    }

    if (random8() < SPARKING)
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

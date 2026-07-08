/**
 * SettingsManager.cpp
 * -----------------------------------------------------------------------
 * See SettingsManager.h for the full contract. This file talks only to
 * ESP32 Preferences (NVS) -- never Hal, never LEDDriver. NVS access is
 * not considered "hardware control" under the HAL boundary; it's
 * persistent storage, the same category of thing SettingsManager was
 * always specified to own directly (spec Section 7).
 * -----------------------------------------------------------------------
 */

#include "SettingsManager.h"

#include <Arduino.h> // millis()

namespace
{
    // NVS namespace for this firmware's settings. Kept generic (not
    // product-specific) since SettingsManager itself is generic.
    constexpr const char* kNamespace = "settings";

    // Keys are kept short; ESP32 Preferences enforces a 15-char limit.
    constexpr const char* kKeyBrightness     = "bri";
    constexpr const char* kKeyAnimationId    = "anim";
    constexpr const char* kKeyPaletteId      = "pal";
    constexpr const char* kKeyPowerState     = "pwr";
    constexpr const char* kKeyAudioMode      = "snd";
    constexpr const char* kKeySpeed          = "spd";
    constexpr const char* kKeyIntensity      = "inten";
    constexpr const char* kKeyPrimaryColor   = "pric";   // packed as a single uint32
    constexpr const char* kKeySecondaryColor = "secc";   // packed as a single uint32

    inline uint32_t packColor(const RgbColor& c)
    {
        return (static_cast<uint32_t>(c.r) << 16)
             | (static_cast<uint32_t>(c.g) << 8)
             |  static_cast<uint32_t>(c.b);
    }

    inline RgbColor unpackColor(uint32_t v)
    {
        return RgbColor(
            static_cast<uint8_t>((v >> 16) & 0xFF),
            static_cast<uint8_t>((v >> 8) & 0xFF),
            static_cast<uint8_t>(v & 0xFF));
    }

    inline bool colorsEqual(const RgbColor& a, const RgbColor& b)
    {
        return a.r == b.r && a.g == b.g && a.b == b.b;
    }

    // Clamp a raw byte loaded from flash into a valid enum range, so a
    // corrupted or stale NVS value can't produce an out-of-range enum.
    template <typename EnumT>
    EnumT clampEnum(uint8_t raw, EnumT fallback)
    {
        const uint8_t count = static_cast<uint8_t>(EnumT::Count);
        if (raw >= count)
        {
            return fallback;
        }
        return static_cast<EnumT>(raw);
    }
}

void SettingsManager::begin()
{
    m_preferences.begin(kNamespace, false); // false = read/write mode

    if (!m_preferences.isKey(kKeyBrightness))
    {
        // First boot, or NVS was cleared -- start from Config.h defaults
        // and persist that baseline immediately (spec Section 1.3).
        applyDefaultsInMemory();
        commitToNvs();
        m_dirty = false;
        return;
    }

    loadFromNvs();
}

void SettingsManager::applyDefaultsInMemory()
{
    m_brightness = LedConfig::DefaultBrightness;
    m_animationId = static_cast<AnimationId>(AnimationConfig::DefaultAnimationId);
    m_paletteId = static_cast<PaletteId>(AnimationConfig::DefaultPaletteId);
    m_powerState = PowerState::On;
    m_audioMode = AudioMode::Disabled;
    m_speed = AnimationConfig::DefaultSpeed;
    m_intensity = AnimationConfig::DefaultIntensity;
    m_primaryColor = RgbColor(
        AnimationConfig::DefaultPrimaryR,
        AnimationConfig::DefaultPrimaryG,
        AnimationConfig::DefaultPrimaryB);
    m_secondaryColor = RgbColor(
        AnimationConfig::DefaultSecondaryR,
        AnimationConfig::DefaultSecondaryG,
        AnimationConfig::DefaultSecondaryB);
}

void SettingsManager::loadFromNvs()
{
    m_brightness = m_preferences.getUChar(kKeyBrightness, LedConfig::DefaultBrightness);

    m_animationId = clampEnum<AnimationId>(
        m_preferences.getUChar(kKeyAnimationId, AnimationConfig::DefaultAnimationId),
        static_cast<AnimationId>(AnimationConfig::DefaultAnimationId));

    m_paletteId = clampEnum<PaletteId>(
        m_preferences.getUChar(kKeyPaletteId, AnimationConfig::DefaultPaletteId),
        static_cast<PaletteId>(AnimationConfig::DefaultPaletteId));

    m_powerState = (m_preferences.getUChar(kKeyPowerState, static_cast<uint8_t>(PowerState::On)) != 0)
        ? PowerState::On
        : PowerState::Off;

    m_audioMode = clampEnum<AudioMode>(
        m_preferences.getUChar(kKeyAudioMode, static_cast<uint8_t>(AudioMode::Disabled)),
        AudioMode::Disabled);

    m_speed = m_preferences.getUChar(kKeySpeed, AnimationConfig::DefaultSpeed);
    m_intensity = m_preferences.getUChar(kKeyIntensity, AnimationConfig::DefaultIntensity);

    m_primaryColor = unpackColor(m_preferences.getUInt(
        kKeyPrimaryColor,
        packColor(RgbColor(AnimationConfig::DefaultPrimaryR, AnimationConfig::DefaultPrimaryG, AnimationConfig::DefaultPrimaryB))));

    m_secondaryColor = unpackColor(m_preferences.getUInt(
        kKeySecondaryColor,
        packColor(RgbColor(AnimationConfig::DefaultSecondaryR, AnimationConfig::DefaultSecondaryG, AnimationConfig::DefaultSecondaryB))));
}

void SettingsManager::commitToNvs()
{
    m_preferences.putUChar(kKeyBrightness, m_brightness);
    m_preferences.putUChar(kKeyAnimationId, static_cast<uint8_t>(m_animationId));
    m_preferences.putUChar(kKeyPaletteId, static_cast<uint8_t>(m_paletteId));
    m_preferences.putUChar(kKeyPowerState, static_cast<uint8_t>(m_powerState));
    m_preferences.putUChar(kKeyAudioMode, static_cast<uint8_t>(m_audioMode));
    m_preferences.putUChar(kKeySpeed, m_speed);
    m_preferences.putUChar(kKeyIntensity, m_intensity);
    m_preferences.putUInt(kKeyPrimaryColor, packColor(m_primaryColor));
    m_preferences.putUInt(kKeySecondaryColor, packColor(m_secondaryColor));
}

void SettingsManager::markDirty()
{
    m_dirty = true;
    m_lastChangeMs = millis(); // sliding debounce: resets on every change
}

void SettingsManager::update()
{
    if (!m_dirty)
    {
        return;
    }

    if ((millis() - m_lastChangeMs) >= Timing::SettingsSaveDebounceMs)
    {
        commitToNvs();
        m_dirty = false;
    }
}

// =========================================================================
// Accessors
// =========================================================================

uint8_t SettingsManager::getBrightness() const { return m_brightness; }
AnimationId SettingsManager::getAnimationId() const { return m_animationId; }
PaletteId SettingsManager::getPaletteId() const { return m_paletteId; }
PowerState SettingsManager::getPowerState() const { return m_powerState; }
AudioMode SettingsManager::getAudioMode() const { return m_audioMode; }
uint8_t SettingsManager::getSpeed() const { return m_speed; }
uint8_t SettingsManager::getIntensity() const { return m_intensity; }
RgbColor SettingsManager::getPrimaryColor() const { return m_primaryColor; }
RgbColor SettingsManager::getSecondaryColor() const { return m_secondaryColor; }

EffectSettings SettingsManager::getEffectSettings() const
{
    EffectSettings settings;
    settings.speed = m_speed;
    settings.intensity = m_intensity;
    settings.paletteId = m_paletteId;
    settings.primaryColor = m_primaryColor;
    settings.secondaryColor = m_secondaryColor;
    return settings;
}

// =========================================================================
// Mutators -- each is a no-op (no dirty mark, no wasted flash write later)
// if the value hasn't actually changed.
// =========================================================================

void SettingsManager::setBrightness(uint8_t value)
{
    if (value == m_brightness) return;
    m_brightness = value;
    markDirty();
}

void SettingsManager::setAnimationId(AnimationId id)
{
    if (id == m_animationId) return;
    m_animationId = id;
    markDirty();
}

void SettingsManager::setPaletteId(PaletteId id)
{
    if (id == m_paletteId) return;
    m_paletteId = id;
    markDirty();
}

void SettingsManager::setPowerState(PowerState state)
{
    if (state == m_powerState) return;
    m_powerState = state;
    markDirty();
}

void SettingsManager::setAudioMode(AudioMode mode)
{
    if (mode == m_audioMode) return;
    m_audioMode = mode;
    markDirty();
}

void SettingsManager::setSpeed(uint8_t value)
{
    if (value == m_speed) return;
    m_speed = value;
    markDirty();
}

void SettingsManager::setIntensity(uint8_t value)
{
    if (value == m_intensity) return;
    m_intensity = value;
    markDirty();
}

void SettingsManager::setPrimaryColor(const RgbColor& color)
{
    if (colorsEqual(color, m_primaryColor)) return;
    m_primaryColor = color;
    markDirty();
}

void SettingsManager::setSecondaryColor(const RgbColor& color)
{
    if (colorsEqual(color, m_secondaryColor)) return;
    m_secondaryColor = color;
    markDirty();
}

// =========================================================================
// Factory reset
// =========================================================================

void SettingsManager::resetToDefaults()
{
    m_preferences.clear();
    applyDefaultsInMemory();
    commitToNvs(); // deliberate, explicit action -- persists immediately,
                   // does not wait for the debounce window
    m_dirty = false;
}

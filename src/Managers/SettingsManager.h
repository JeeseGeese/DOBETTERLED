/**
 * SettingsManager.h
 * -----------------------------------------------------------------------
 * Owns persistent user preferences ONLY: brightness, current animation
 * ID, current palette ID, power state, audio mode, speed, intensity,
 * primary color, secondary color.
 *
 * Strict boundaries (see docs/ARCHITECTURE_CONTRACT.md):
 *   - Never calls Hal::* or LEDDriver::* -- it is a pure state store.
 *     SystemManager reads values out of it and applies them to
 *     hardware-facing managers itself.
 *   - Does not interpret what a value means (it stores AnimationId::Fire
 *     as the number 2, it doesn't know that renders fire).
 *   - Does not own any in-progress/transient state that shouldn't be
 *     persisted -- e.g., the live brightness value DURING an active
 *     brightness ramp belongs to SystemManager until the ramp ends and
 *     the final value is written back here.
 *
 * Persistence model (spec Section 7): all state lives in RAM; any
 * setter marks the store dirty and resets a debounce timer. update()
 * (called every loop pass) only commits to NVS once
 * Timing::SettingsSaveDebounceMs has elapsed with no further changes.
 * This is a hard requirement, not an optimization -- flash has finite
 * write endurance, and rapid interactions (brightness ramping, quickly
 * cycling animations) must produce zero flash writes until the user
 * stops and the debounce window elapses.
 * -----------------------------------------------------------------------
 */

#pragma once

#include "Types.h"
#include "Config.h"
#include <Preferences.h>

class SettingsManager
{
public:
    // Opens the NVS namespace and loads persisted state, or initializes
    // and persists Config.h defaults if this is the first boot (or NVS
    // was previously cleared).
    void begin();

    // Call every loop() iteration. Cheap no-op unless a change is
    // pending and the debounce window has elapsed.
    void update();

    // ---------------------------------------------------------------
    // Accessors -- read the current in-RAM value (always up to date,
    // regardless of whether it's been committed to flash yet).
    // ---------------------------------------------------------------

    uint8_t getBrightness() const;
    AnimationId getAnimationId() const;
    PaletteId getPaletteId() const;
    PowerState getPowerState() const;
    AudioMode getAudioMode() const;
    uint8_t getSpeed() const;
    uint8_t getIntensity() const;
    RgbColor getPrimaryColor() const;
    RgbColor getSecondaryColor() const;

    // Convenience bundle matching the shared EffectSettings shape
    // (Types.h) that AnimationManager will hand to effects.
    EffectSettings getEffectSettings() const;

    // ---------------------------------------------------------------
    // Mutators -- update the in-RAM value immediately and mark the
    // store dirty (if the value actually changed). Never touches NVS
    // synchronously; see update().
    // ---------------------------------------------------------------

    void setBrightness(uint8_t value);
    void setAnimationId(AnimationId id);
    void setPaletteId(PaletteId id);
    void setPowerState(PowerState state);
    void setAudioMode(AudioMode mode);
    void setSpeed(uint8_t value);
    void setIntensity(uint8_t value);
    void setPrimaryColor(const RgbColor& color);
    void setSecondaryColor(const RgbColor& color);

    // ---------------------------------------------------------------
    // Factory reset (spec Section 8)
    // ---------------------------------------------------------------

    // Clears all persisted keys, resets in-RAM state to Config.h
    // defaults (power state resets to On, per spec), and immediately
    // persists the new baseline -- does not wait for the debounce
    // window, since this is an explicit, deliberate action.
    void resetToDefaults();

private:
    Preferences m_preferences;

    uint8_t m_brightness;
    AnimationId m_animationId;
    PaletteId m_paletteId;
    PowerState m_powerState;
    AudioMode m_audioMode;
    uint8_t m_speed;
    uint8_t m_intensity;
    RgbColor m_primaryColor;
    RgbColor m_secondaryColor;

    bool m_dirty = false;
    unsigned long m_lastChangeMs = 0;

    void applyDefaultsInMemory();
    void loadFromNvs();
    void commitToNvs();
    void markDirty();
};

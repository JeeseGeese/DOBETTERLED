/**
 * SystemManager.h
 * -----------------------------------------------------------------------
 * The single application coordinator. Owns every manager instance and
 * is the only module that maps ButtonEvents to concrete actions.
 *
 * Per docs/ARCHITECTURE_CONTRACT.md, SystemManager owns:
 *   - The setup sequence (constructing/initializing every manager)
 *   - Frame pacing (deciding when a new frame is due)
 *   - Mapping ButtonEvent values to actions
 *   - Applying settings values to LEDDriver
 *   - Calling AnimationManager::update() and SettingsManager::update()
 *
 * This is intentionally the one file allowed to know about every other
 * manager -- that's what "orchestration" means. It still never touches
 * Hal, GPIO, or FastLED directly; it always goes through a manager.
 *
 * Milestone scope: boot, render a solid color, respond to button
 * gestures, apply brightness ramp, persist via debounced settings.
 * No fades, no transitions, no additional effects yet -- those are
 * later milestones.
 * -----------------------------------------------------------------------
 */

#pragma once

#include "Types.h"
#include "Config.h"
#include "HAL/Hal.h"
#include "Managers/LEDDriver.h"
#include "Managers/ButtonManager.h"
#include "Managers/SettingsManager.h"
#include "Managers/PaletteManager.h"
#include "Managers/SoundManager.h"
#include "Managers/EffectRegistry.h"
#include "Managers/AnimationManager.h"
#include "Effects/SolidEffect.h"

#include <stdint.h>

class SystemManager
{
public:
    // Runs the full boot sequence: HAL, every manager, effect
    // registration, and applying the persisted (or default) state to
    // LEDDriver. Called once from the sketch's setup().
    void begin();

    // Called every loop() iteration from the sketch. Reads the button,
    // paces frame rendering to Timing::TargetFrameIntervalMs, and
    // flushes any pending settings commit.
    void update();

private:
    LEDDriver m_ledDriver;
    ButtonManager m_buttonManager;
    SettingsManager m_settingsManager;
    PaletteManager m_paletteManager;
    SoundManager m_soundManager;
    EffectRegistry m_effectRegistry;
    AnimationManager m_animationManager;

    // Effect instances are owned here for now -- EffectRegistry never
    // owns or allocates them (see EffectRegistry.h). As more effects
    // are added in a later milestone, they'll be constructed and
    // registered the same way.
    SolidEffect m_solidEffect;

    unsigned long m_lastFrameMs = 0;

    // Translates one ButtonEvent into whatever action it means, per
    // docs/PRODUCT_SPEC.md Section 3 and docs/DESIGN_LOG.md's notes on
    // toggle-vs-ramp mutual exclusivity. This is the only place in the
    // firmware that turns "the button did X" into "the system does Y."
    void handleButtonEvent(ButtonEvent event);

    // Applies a signed brightness delta (from ButtonManager's ramp
    // intent) to the authoritative brightness value, clamps it, and
    // pushes the result to both SettingsManager and LEDDriver.
    void applyBrightnessDelta(int16_t delta);

    // Pushes the current SettingsManager power state to LEDDriver.
    void applyPowerState();
};

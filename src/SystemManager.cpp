/**
 * SystemManager.cpp
 * -----------------------------------------------------------------------
 * See SystemManager.h for the contract. This file is the one place in
 * the firmware where "a button gesture happened" becomes "the system
 * does something" -- every other manager stays ignorant of that
 * mapping by design.
 * -----------------------------------------------------------------------
 */

#include "SystemManager.h"

#include <Arduino.h> // millis()

void SystemManager::begin()
{
    // =====================================================================
    // TEMPORARY DIAGNOSTIC -- bypasses SettingsManager power state,
    // AnimationManager, and all normal boot behavior, so nothing can
    // touch the strip after LEDDriver::begin()'s own inlined diagnostic
    // sequence runs (see LEDDriver.cpp for that half of this test).
    //
    // RESTORE, once root cause is found, to the full sequence: Hal::begin(),
    // m_ledDriver.setPower(true), m_ledDriver.begin(), m_buttonManager.begin(),
    // m_settingsManager.begin(), m_soundManager.begin(), registerEffect,
    // m_animationManager.begin(...), m_ledDriver.setBrightness(...),
    // applyPowerState(), m_lastFrameMs = millis() -- this exact block
    // existed one commit ago; see version control / CHANGELOG v1.0.1 for
    // the pre-diagnostic text if needed.
    // =====================================================================

    Hal::begin(); // harmless -- only configures button pin + relay pin mode

    m_ledDriver.begin(); // now contains its own inlined diagnostic sequence

    // Intentionally nothing else runs. No settings load, no animation
    // manager, no applyPowerState(), no clear/black-frame logic -- so
    // the red fill from LEDDriver::begin() is the last thing that ever
    // touches the strip.
}

void SystemManager::update()
{
    // TEMPORARY DIAGNOSTIC -- intentionally empty, matching the working
    // isolation sketch's empty loop(). RESTORE the real button-read /
    // frame-pacing / settings-flush body (see CHANGELOG v1.0.1 or prior)
    // once LEDDriver::begin()'s inlined diagnostic is confirmed working.
}

void SystemManager::handleButtonEvent(ButtonEvent event)
{
    switch (event)
    {
        case ButtonEvent::SingleClick:
        {
            m_animationManager.selectNext();
            m_settingsManager.setAnimationId(m_animationManager.getCurrentAnimationId());
            break;
        }

        case ButtonEvent::DoubleClick:
        {
            m_animationManager.selectPrevious();
            m_settingsManager.setAnimationId(m_animationManager.getCurrentAnimationId());
            break;
        }

        case ButtonEvent::TripleClick:
        {
            const PaletteId next = m_paletteManager.getNextPaletteId(m_settingsManager.getPaletteId());
            m_settingsManager.setPaletteId(next);
            break;
        }

        case ButtonEvent::QuadClick:
        {
            uint8_t nextMode = static_cast<uint8_t>(m_settingsManager.getAudioMode()) + 1;
            if (nextMode >= static_cast<uint8_t>(AudioMode::Count))
            {
                nextMode = 0;
            }
            m_settingsManager.setAudioMode(static_cast<AudioMode>(nextMode));
            break;
        }

        case ButtonEvent::LongPressStart:
        {
            // Nothing to do yet at this milestone -- visual "pending"
            // feedback for an in-progress hold is later-milestone polish.
            break;
        }

        case ButtonEvent::LongPressHold:
        {
            if (m_buttonManager.isBrightnessRampActive())
            {
                applyBrightnessDelta(m_buttonManager.consumeBrightnessRampDelta());
            }
            // Else: still short of ramp territory, waiting to see if
            // this resolves into a toggle or a ramp on release.
            break;
        }

        case ButtonEvent::LongPressRelease:
        {
            const unsigned long duration = m_buttonManager.getLastHoldDurationMs();

            if (duration >= Timing::BrightnessHoldStartMs)
            {
                // Was a ramp -- brightness was already applied live
                // during LongPressHold ticks. Releasing does NOT also
                // toggle power (see docs/DESIGN_LOG.md, Section 6).
            }
            else if (duration >= Timing::LongPressMinMs)
            {
                const PowerState newState =
                    (m_settingsManager.getPowerState() == PowerState::On) ? PowerState::Off : PowerState::On;
                m_settingsManager.setPowerState(newState);
                applyPowerState();
            }
            break;
        }

        case ButtonEvent::FactoryResetRequested:
        {
            m_settingsManager.resetToDefaults();
            m_animationManager.selectById(m_settingsManager.getAnimationId());
            m_ledDriver.setBrightness(m_settingsManager.getBrightness());
            applyPowerState();
            break;
        }

        case ButtonEvent::None:
        default:
            break;
    }
}

void SystemManager::applyBrightnessDelta(int16_t delta)
{
    if (delta == 0)
    {
        return;
    }

    int16_t newBrightness = static_cast<int16_t>(m_settingsManager.getBrightness()) + delta;

    if (newBrightness < static_cast<int16_t>(LedConfig::MinBrightness))
    {
        newBrightness = static_cast<int16_t>(LedConfig::MinBrightness);
    }
    else if (newBrightness > static_cast<int16_t>(LedConfig::MaxBrightness))
    {
        newBrightness = static_cast<int16_t>(LedConfig::MaxBrightness);
    }

    m_settingsManager.setBrightness(static_cast<uint8_t>(newBrightness));
    m_ledDriver.setBrightness(m_settingsManager.getBrightness());
}

void SystemManager::applyPowerState()
{
    m_ledDriver.setPower(m_settingsManager.getPowerState() == PowerState::On);
}

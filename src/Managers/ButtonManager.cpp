/**
 * ButtonManager.cpp
 * -----------------------------------------------------------------------
 * See ButtonManager.h for the full contract. This file implements the
 * gesture state machine using only Hal::isButtonPressed() and
 * Config::Timing constants -- no GPIO, no ProductConfig, no knowledge
 * of animations or settings.
 * -----------------------------------------------------------------------
 */

#include "ButtonManager.h"
#include "../HAL/Hal.h"
#include "Config.h"

#include <Arduino.h> // millis()

void ButtonManager::begin()
{
    const unsigned long now = millis();

    m_lastRawSample = Hal::isButtonPressed();
    m_lastRawChangeMs = now;
    m_stablePressed = m_lastRawSample;
    m_lastPollMs = now;

    m_pendingClickCount = 0;
    m_clickWindowDeadlineMs = 0;

    resetHoldState();
}

void ButtonManager::resetHoldState()
{
    m_isHeld = false;
    m_pressStartMs = 0;
    m_longPressStartFired = false;
    m_factoryResetFired = false;
    m_lastHoldEventMs = 0;
    m_rampDirection = 1;
    m_rampPhaseStartMs = 0;
    m_lastRampUpdateMs = 0;
    m_rampAccumulatedDelta = 0.0f;
}

ButtonEvent ButtonManager::update()
{
    const unsigned long now = millis();

    // Rate-limit raw sampling so this is cheap to call every loop pass.
    if (now - m_lastPollMs < Timing::ButtonPollIntervalMs)
    {
        // Still allow click-window expiry to be noticed between polls,
        // since that's a pure time check, not a hardware sample.
        return finalizeClickSequenceIfExpired(now);
    }
    m_lastPollMs = now;

    const bool wasStable = m_stablePressed;
    const bool edgeOccurred = sampleDebouncedState(now);

    if (edgeOccurred)
    {
        if (m_stablePressed && !wasStable)
        {
            return handlePressEdge(now);
        }
        if (!m_stablePressed && wasStable)
        {
            return handleReleaseEdge(now);
        }
    }

    if (m_isHeld)
    {
        return processOngoingHold(now);
    }

    return finalizeClickSequenceIfExpired(now);
}

bool ButtonManager::sampleDebouncedState(unsigned long nowMs)
{
    const bool raw = Hal::isButtonPressed();

    if (raw != m_lastRawSample)
    {
        m_lastRawSample = raw;
        m_lastRawChangeMs = nowMs;
    }

    if ((nowMs - m_lastRawChangeMs) >= Timing::DebounceMs)
    {
        if (raw != m_stablePressed)
        {
            m_stablePressed = raw;
            return true; // a real, debounced edge occurred
        }
    }

    return false;
}

ButtonEvent ButtonManager::handlePressEdge(unsigned long nowMs)
{
    m_isHeld = true;
    m_pressStartMs = nowMs;
    m_longPressStartFired = false;
    m_factoryResetFired = false;
    m_lastHoldEventMs = nowMs;

    // A fresh press doesn't itself resolve into an event -- we only
    // know what it means once it's released (a click) or crosses the
    // long-press threshold (handled in processOngoingHold).
    return ButtonEvent::None;
}

ButtonEvent ButtonManager::handleReleaseEdge(unsigned long nowMs)
{
    const unsigned long heldDuration = nowMs - m_pressStartMs;
    m_isHeld = false;
    m_lastHoldDurationMs = heldDuration;

    if (m_factoryResetFired)
    {
        // Reset already fired automatically mid-hold; this release just
        // signals "the hold has ended" so SystemManager can stop any
        // reset-pending visual feedback. No new classification needed.
        resetHoldState();
        return ButtonEvent::LongPressRelease;
    }

    if (m_longPressStartFired)
    {
        // Was a qualifying long press (toggle candidate or a ramp that
        // never reached factory-reset territory). SystemManager reads
        // getLastHoldDurationMs() to decide which.
        resetHoldState();
        return ButtonEvent::LongPressRelease;
    }

    // Short press: count it as a click. Any press that reaches this
    // branch released before the long-press threshold, by construction.
    m_pendingClickCount++;
    m_clickWindowDeadlineMs = nowMs + Timing::MultiClickWindowMs;

    if (m_pendingClickCount >= 4)
    {
        m_pendingClickCount = 0;
        return ButtonEvent::QuadClick;
    }

    return ButtonEvent::None; // wait for window expiry or the next click
}

ButtonEvent ButtonManager::processOngoingHold(unsigned long nowMs)
{
    const unsigned long heldDuration = nowMs - m_pressStartMs;

    // Automatic factory reset -- fires once, does not require release,
    // and takes priority over everything else once reached.
    if (!m_factoryResetFired && heldDuration >= Timing::FactoryResetMaxMs)
    {
        m_factoryResetFired = true;
        m_pendingClickCount = 0; // any prior click sequence is moot now
        return ButtonEvent::FactoryResetRequested;
    }

    if (m_factoryResetFired)
    {
        // Already fired; nothing further to report while still held.
        return ButtonEvent::None;
    }

    if (heldDuration >= Timing::LongPressMinMs)
    {
        if (!m_longPressStartFired)
        {
            m_longPressStartFired = true;
            m_pendingClickCount = 0; // a long hold supersedes any partial click count
            m_lastHoldEventMs = nowMs;
            return ButtonEvent::LongPressStart;
        }

        // Keep accumulating ramp intent whenever we're in ramp territory,
        // regardless of event throttling below, so the accumulator is
        // always current the instant a caller consumes it.
        if (heldDuration >= Timing::BrightnessHoldStartMs)
        {
            accumulateRampIntent(nowMs);
        }

        // Throttle repeated Hold events to the ramp step cadence rather
        // than firing on every raw poll -- keeps the event stream from
        // being needlessly noisy while still feeling smooth.
        if ((nowMs - m_lastHoldEventMs) >= Timing::BrightnessRampStepMs)
        {
            m_lastHoldEventMs = nowMs;
            return ButtonEvent::LongPressHold;
        }
    }

    return ButtonEvent::None; // still short of the long-press threshold
}

ButtonEvent ButtonManager::finalizeClickSequenceIfExpired(unsigned long nowMs)
{
    if (m_pendingClickCount == 0 || m_isHeld)
    {
        return ButtonEvent::None;
    }

    if (nowMs < m_clickWindowDeadlineMs)
    {
        return ButtonEvent::None; // still within the window, more clicks may arrive
    }

    const uint8_t count = m_pendingClickCount;
    m_pendingClickCount = 0;

    switch (count)
    {
        case 1: return ButtonEvent::SingleClick;
        case 2: return ButtonEvent::DoubleClick;
        case 3: return ButtonEvent::TripleClick;
        default: return ButtonEvent::None; // 4+ is handled immediately on the 4th release
    }
}

void ButtonManager::accumulateRampIntent(unsigned long nowMs)
{
    if (m_lastRampUpdateMs == 0)
    {
        // First tick of ramp territory this hold -- start the direction
        // timer fresh. No brightness value is initialized here because
        // ButtonManager doesn't have one; it only tracks direction/time.
        m_rampDirection = 1;
        m_rampPhaseStartMs = nowMs;
        m_lastRampUpdateMs = nowMs;
        return; // nothing accumulated yet this hold
    }

    const unsigned long elapsedMs = nowMs - m_lastRampUpdateMs;
    m_lastRampUpdateMs = nowMs;

    // Rate is expressed in brightness-units-per-ms, derived from the
    // configured full-sweep duration and the configured brightness
    // range. This is a read-only scaling constant, not owned state --
    // ButtonManager never stores or clamps an actual brightness number.
    const float range = static_cast<float>(LedConfig::MaxBrightness - LedConfig::MinBrightness);
    const float unitsPerMs = range / static_cast<float>(Timing::BrightnessRampFullSweepMs);

    m_rampAccumulatedDelta += unitsPerMs * static_cast<float>(elapsedMs) * static_cast<float>(m_rampDirection);

    // Flip direction on a fixed timer matching how long a full-range
    // sweep would take, giving the same "breathing" bounce feel as
    // before -- but purely time-based now, since ButtonManager has no
    // way to know when a real, externally-owned brightness value has
    // actually hit a bound. SystemManager still clamps the real value;
    // this is only what drives the back-and-forth suggestion.
    if ((nowMs - m_rampPhaseStartMs) >= Timing::BrightnessRampFullSweepMs)
    {
        m_rampDirection = static_cast<int8_t>(-m_rampDirection);
        m_rampPhaseStartMs = nowMs;
    }
}

// =========================================================================
// Query accessors
// =========================================================================

unsigned long ButtonManager::getCurrentHoldDurationMs() const
{
    if (!m_isHeld)
    {
        return 0;
    }
    return millis() - m_pressStartMs;
}

bool ButtonManager::isFactoryResetWarningActive() const
{
    if (!m_isHeld || m_factoryResetFired)
    {
        return false;
    }
    return (millis() - m_pressStartMs) >= Timing::FactoryResetMinMs;
}

bool ButtonManager::isBrightnessRampActive() const
{
    if (!m_isHeld || m_factoryResetFired || !m_longPressStartFired)
    {
        return false;
    }
    return (millis() - m_pressStartMs) >= Timing::BrightnessHoldStartMs;
}

int16_t ButtonManager::consumeBrightnessRampDelta()
{
    if (!isBrightnessRampActive())
    {
        return 0;
    }

    const int16_t wholeDelta = static_cast<int16_t>(m_rampAccumulatedDelta);
    m_rampAccumulatedDelta -= static_cast<float>(wholeDelta); // keep the fractional remainder
    return wholeDelta;
}

unsigned long ButtonManager::getLastHoldDurationMs() const
{
    return m_lastHoldDurationMs;
}

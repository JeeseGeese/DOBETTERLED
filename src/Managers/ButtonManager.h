/**
 * ButtonManager.h
 * -----------------------------------------------------------------------
 * Owns the entire button gesture state machine: debouncing, click
 * counting (single/double/triple/quad), long-press phase tracking,
 * brightness-ramp intent/direction reporting, and factory-reset
 * confirmation timing. ButtonManager reports a brightness ADJUSTMENT
 * intent, never an absolute brightness value -- it owns no brightness
 * state at all; that belongs to SettingsManager/SystemManager.
 *
 * Strict boundaries:
 *   - Consumes ONLY Hal::isButtonPressed() -- no GPIO, no ProductConfig.
 *   - Contains no animation, palette, or settings logic. It reports
 *     what the button is physically doing; SystemManager decides what
 *     that means for the rest of the system.
 *   - All timing constants come from Config::Timing -- nothing here is
 *     hard-coded, and nothing here is board-specific.
 *
 * Architecture position: a Manager, sitting between SystemManager and
 * the HAL, exactly as defined in Hal.h's layering diagram.
 * -----------------------------------------------------------------------
 */

#pragma once

#include "Types.h"
#include <stdint.h>

class ButtonManager
{
public:
    // Resets internal state. Does not touch hardware -- Hal::begin()
    // already configured the button pin; this just initializes timers.
    void begin();

    // Call every loop() iteration. Internally rate-limits its own raw
    // sampling to Config::Timing::ButtonPollIntervalMs, so it is always
    // safe and cheap to call unconditionally on every pass. Returns at
    // most one event per call.
    ButtonEvent update();

    // ---------------------------------------------------------------
    // Query accessors -- read alongside LongPressHold / LongPressRelease
    // events to determine what a hold actually means. ButtonManager
    // exposes the facts; it does not decide the resulting action.
    // ---------------------------------------------------------------

    // Milliseconds the button has been continuously held right now.
    // Returns 0 if the button is not currently pressed.
    unsigned long getCurrentHoldDurationMs() const;

    // True once the current hold has crossed Timing::FactoryResetMinMs
    // and a reset has not yet fired (or the button been released).
    // Consumers use this to drive "reset pending" visual feedback.
    bool isFactoryResetWarningActive() const;

    // True while the current hold is within brightness-ramp territory
    // (past Timing::BrightnessHoldStartMs, short of a fired reset).
    bool isBrightnessRampActive() const;

    // Returns the suggested brightness ADJUSTMENT (not an absolute value)
    // accumulated since the last call -- positive means "increase
    // brightness," negative means "decrease." ButtonManager owns no
    // brightness state at all; it only tracks its own elapsed-time-based
    // direction/intent. SystemManager (or SettingsManager) owns the
    // actual current brightness, adds this delta to it each time it's
    // consumed, and clamps to LedConfig::MinBrightness/MaxBrightness
    // itself. Calling this resets the internal accumulator, so it
    // should be called exactly once per LongPressHold event received.
    // Returns 0 when isBrightnessRampActive() is false.
    int16_t consumeBrightnessRampDelta();

    // Total duration (ms) of the most recently completed long-press
    // hold. Valid immediately after a LongPressRelease event; stale
    // otherwise. This is the value SystemManager compares against
    // Config::Timing thresholds to decide toggle-on/off vs. "a ramp
    // just ended, do nothing further."
    unsigned long getLastHoldDurationMs() const;

private:
    // ---------------------------------------------------------------
    // Debounce state
    // ---------------------------------------------------------------
    bool m_lastRawSample = false;
    unsigned long m_lastRawChangeMs = 0;
    bool m_stablePressed = false; // debounced logical state
    unsigned long m_lastPollMs = 0;

    // ---------------------------------------------------------------
    // Click counting state
    // ---------------------------------------------------------------
    uint8_t m_pendingClickCount = 0;
    unsigned long m_clickWindowDeadlineMs = 0;

    // ---------------------------------------------------------------
    // Current hold state
    // ---------------------------------------------------------------
    bool m_isHeld = false;
    unsigned long m_pressStartMs = 0;
    bool m_longPressStartFired = false;
    bool m_factoryResetFired = false;
    unsigned long m_lastHoldEventMs = 0;   // throttles LongPressHold rate
    unsigned long m_lastHoldDurationMs = 0; // snapshot for getLastHoldDurationMs()

    // ---------------------------------------------------------------
    // Brightness ramp state -- intent/direction only. ButtonManager
    // never stores an absolute brightness number; it only tracks a
    // direction sign (which it flips on its own fixed timer, since it
    // has no way to know when a real brightness value hits a bound)
    // and a fractional delta accumulator so small per-tick amounts
    // aren't lost to integer rounding between consume() calls.
    // ---------------------------------------------------------------
    int8_t m_rampDirection = 1;
    unsigned long m_rampPhaseStartMs = 0;  // when the current direction leg began
    unsigned long m_lastRampUpdateMs = 0;  // last time we accumulated elapsed time
    float m_rampAccumulatedDelta = 0.0f;   // fractional delta owed, not yet consumed

    // ---------------------------------------------------------------
    // Internal helpers
    // ---------------------------------------------------------------
    bool sampleDebouncedState(unsigned long nowMs);
    ButtonEvent handlePressEdge(unsigned long nowMs);
    ButtonEvent handleReleaseEdge(unsigned long nowMs);
    ButtonEvent processOngoingHold(unsigned long nowMs);
    ButtonEvent finalizeClickSequenceIfExpired(unsigned long nowMs);
    void resetHoldState();
    void accumulateRampIntent(unsigned long nowMs);
};

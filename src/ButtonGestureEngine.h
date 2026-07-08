#pragma once

/**
 * ButtonGestureEngine.h
 * -----------------------------------------------------------------------
 * Milestone 2: standalone button gesture detection, reporting only.
 *
 * Detects the BRoadmap v1.2 gesture set (docs/PRODUCT_SPEC.md Section 3)
 * and prints a "BUTTON: ..." line to Serial when each is recognized.
 * Wiring any gesture to a real action is explicitly out of scope for
 * this class -- see PRODUCT_SPEC.md's Milestone 2 scope note.
 *
 * Design choices, and why:
 *
 *   - This class does NOT call digitalRead() or pinMode() itself. The
 *     caller (EngineeringConsole) already owns the button pin and
 *     passes the current raw reading into update(bool). This keeps
 *     the engine pure logic -- debounce, click-counting, hold-timing
 *     driven only by a bool + millis() -- which is what makes it a
 *     "testable layer" and "suitable for later extraction into
 *     ButtonManager" per the milestone's implementation goal: none of
 *     this logic is coupled to Arduino GPIO calls, FastLED, or any
 *     other hardware/dormant-tree dependency.
 *
 *   - Zero includes beyond <Arduino.h> (for millis()/Serial/uint8_t
 *     types). No Config.h, no Types.h, no Hal.h, no Managers/ header.
 *     Its own timing constants live in ButtonGestureEngine.cpp,
 *     independent of Config.h::Timing (which still describes the
 *     dormant tree's own, not-yet-reconciled thresholds).
 *
 *   - Gesture classification distinguishes "which press in a click
 *     sequence is being held": a hold on the 1st press of a sequence
 *     is the Long Hold (2-3s) family; a hold on the 2nd press is
 *     2 Presses + Hold; a hold on the 3rd+ press, or a finalized click
 *     count outside {1,2,3,4,6,10}, is explicitly out of scope and is
 *     reported as an informational (non-"BUTTON:") line so nothing is
 *     silently dropped, but nothing crashes or takes action either.
 *
 *   - MILESTONE 3: update() now also returns a Gesture enum classifying
 *     what (if anything) was detected this call, in addition to its
 *     existing Serial "BUTTON: ..." prints. Purely additive -- the
 *     detection logic and every existing print line are unchanged from
 *     the hardware-verified Milestone 2 behavior. This lets a caller
 *     (EngineeringConsole) react to a specific gesture without parsing
 *     Serial text.
 * -----------------------------------------------------------------------
 */

#include <Arduino.h>

class ButtonGestureEngine
{
public:
    enum class Gesture : uint8_t
    {
        None,
        SinglePress,
        DoublePress,
        TriplePress,
        FourPress,
        DoublePressHold,
        LongHold,
        SixPressPowerCandidate,
        TenPressFactoryResetPending,
        Unclassified
    };

    // Resets internal state. Touches no hardware -- the caller is
    // responsible for pinMode() on whichever pin it reads.
    void begin();

    // Call every loop() iteration, passing the current debounced-or-raw
    // button reading (true = physically pressed). This class performs
    // its own debounce on top of whatever it's given, so passing a raw
    // digitalRead() result is fine. Returns the gesture (if any)
    // finalized on this call; Gesture::None most calls.
    Gesture update(bool isPressedRaw);

private:
    // ---------------------------------------------------------------
    // Debounce state
    // ---------------------------------------------------------------
    bool m_lastRawSample = false;
    unsigned long m_lastRawChangeMs = 0;
    bool m_stablePressed = false;

    // ---------------------------------------------------------------
    // Click-sequence state
    // ---------------------------------------------------------------
    // Number of completed (pressed-then-released-as-a-click) presses
    // in the current sequence. Does NOT include a press currently in
    // progress -- that one is counted only once it either releases as
    // a click, or gets reclassified into a hold gesture instead.
    uint8_t m_completedClickCount = 0;

    // 0 = no pending sequence to finalize. Otherwise, the sequence
    // finalizes (based on m_completedClickCount) once millis() reaches
    // this deadline while the button is not currently pressed.
    unsigned long m_clickWindowDeadlineMs = 0;

    // ---------------------------------------------------------------
    // Current ongoing press (hold) state
    // ---------------------------------------------------------------
    unsigned long m_pressStartMs = 0;

    // True once a hold gesture (Long Hold or 2 Presses + Hold, or the
    // informational 3rd+-press-hold fallback) has already been
    // reported for the CURRENT ongoing press -- prevents re-firing
    // every loop() while still held, and tells onReleaseEdge() this
    // press should NOT also be counted as a completed click.
    bool m_holdReportedThisPress = false;

    void onPressEdge(unsigned long nowMs);
    void onReleaseEdge(unsigned long nowMs);
    Gesture onOngoingHold(unsigned long nowMs);
    Gesture checkClickWindowExpiry(unsigned long nowMs);
    Gesture finalizeClickSequence(uint8_t completedCount);
};

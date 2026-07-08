#include "ButtonGestureEngine.h"

namespace
{
    // Local timing constants for this engine only -- deliberately
    // independent of Config.h::Timing (dormant tree). If/when this
    // class is extracted into ButtonManager, these are the values to
    // reconcile against Config.h at that time, not before.

    constexpr uint16_t DEBOUNCE_MS = 30;

    // Max gap after a release before the click sequence finalizes with
    // whatever count it has. Each new click within the window restarts
    // this deadline from that click's own release.
    constexpr uint16_t CLICK_WINDOW_MS = 350;

    // Minimum duration an ongoing press must be held before it's
    // reclassified from "might still be a click" into "this is a
    // hold." Used both to detect 2 Presses + Hold and as the point
    // past which we start watching for Long Hold's own longer window.
    constexpr uint16_t HOLD_CONFIRM_MS = 500;

    // Long Hold (2-3s) window -- applies only to the 1st press of a
    // sequence (see header comment). Only the lower bound is acted on;
    // the upper bound is documentation of the intended window, not an
    // additional fired event in this milestone.
    constexpr uint16_t LONG_HOLD_MIN_MS = 2000;
    constexpr uint16_t LONG_HOLD_MAX_MS = 3000; // documentation only

    static_assert(LONG_HOLD_MAX_MS > LONG_HOLD_MIN_MS,
        "Long Hold's documented 2-3s window must have MAX > MIN; if this "
        "ever needs to become an enforced upper bound rather than just "
        "documentation, that's a deliberate design change, not a typo fix.");
}

void ButtonGestureEngine::begin()
{
    m_lastRawSample = false;
    m_lastRawChangeMs = 0;
    m_stablePressed = false;
    m_completedClickCount = 0;
    m_clickWindowDeadlineMs = 0;
    m_pressStartMs = 0;
    m_holdReportedThisPress = false;
}

ButtonGestureEngine::Gesture ButtonGestureEngine::update(bool isPressedRaw)
{
    const unsigned long now = millis();
    Gesture result = Gesture::None;

    if (isPressedRaw != m_lastRawSample)
    {
        m_lastRawSample = isPressedRaw;
        m_lastRawChangeMs = now;
    }

    if (now - m_lastRawChangeMs >= DEBOUNCE_MS && m_stablePressed != m_lastRawSample)
    {
        m_stablePressed = m_lastRawSample;

        if (m_stablePressed)
        {
            onPressEdge(now);
        }
        else
        {
            onReleaseEdge(now);
        }
    }

    if (m_stablePressed)
    {
        const Gesture holdGesture = onOngoingHold(now);
        if (holdGesture != Gesture::None)
        {
            result = holdGesture;
        }
    }

    const Gesture clickGesture = checkClickWindowExpiry(now);
    if (clickGesture != Gesture::None)
    {
        result = clickGesture;
    }

    return result;
}

void ButtonGestureEngine::onPressEdge(unsigned long nowMs)
{
    m_pressStartMs = nowMs;
    m_holdReportedThisPress = false;
}

ButtonGestureEngine::Gesture ButtonGestureEngine::onOngoingHold(unsigned long nowMs)
{
    if (m_holdReportedThisPress)
    {
        return Gesture::None; // Already classified this press's hold; nothing more to do until release.
    }

    const unsigned long heldMs = nowMs - m_pressStartMs;

    // Which press, within the current sequence, is this ongoing one?
    // m_completedClickCount only counts FINISHED clicks, so the press
    // still in progress is completedCount + 1.
    const uint8_t currentPressIndex = static_cast<uint8_t>(m_completedClickCount + 1);

    if (currentPressIndex == 1)
    {
        // 1st press of a sequence, held long enough -> Long Hold family.
        if (heldMs >= LONG_HOLD_MIN_MS)
        {
            Serial.println("BUTTON: LONG_HOLD");
            m_holdReportedThisPress = true;
            return Gesture::LongHold;
        }
    }
    else if (currentPressIndex == 2)
    {
        // 2nd press of a sequence, held -> 2 Presses + Hold (Audio
        // Reactive Overlay toggle candidate).
        if (heldMs >= HOLD_CONFIRM_MS)
        {
            Serial.println("BUTTON: DOUBLE_PRESS_HOLD");
            m_holdReportedThisPress = true;
            return Gesture::DoublePressHold;
        }
    }
    else
    {
        // 3rd+ press held -- explicitly out of scope for the finalized
        // map. Reported for visibility (never silently dropped), but
        // deliberately NOT one of the seven "BUTTON: ..." acceptance
        // lines, so it can't be confused with a real, defined gesture.
        if (heldMs >= HOLD_CONFIRM_MS)
        {
            Serial.print("Button: unclassified hold on press #");
            Serial.print(currentPressIndex);
            Serial.println(" of sequence -- no action taken");
            m_holdReportedThisPress = true;
            return Gesture::Unclassified;
        }
    }

    return Gesture::None;
}

void ButtonGestureEngine::onReleaseEdge(unsigned long nowMs)
{
    if (m_holdReportedThisPress)
    {
        // This press was already classified as a hold gesture (Long
        // Hold, Double Press + Hold, or the unclassified-hold notice).
        // It contributes no click, and the whole sequence ends here --
        // per PRODUCT_SPEC.md Section 3, these gestures are reported
        // once the hold threshold is crossed, not on release.
        m_completedClickCount = 0;
        m_clickWindowDeadlineMs = 0;
        return;
    }

    // Ordinary click: count it, and (re)start the click window from
    // this release.
    ++m_completedClickCount;
    m_clickWindowDeadlineMs = nowMs + CLICK_WINDOW_MS;
}

ButtonGestureEngine::Gesture ButtonGestureEngine::checkClickWindowExpiry(unsigned long nowMs)
{
    if (m_clickWindowDeadlineMs == 0)
    {
        return Gesture::None; // No pending sequence.
    }

    if (m_stablePressed)
    {
        return Gesture::None; // Don't finalize mid-press -- it might still become a hold gesture.
    }

    if (nowMs < m_clickWindowDeadlineMs)
    {
        return Gesture::None; // Window hasn't expired yet.
    }

    const Gesture result = finalizeClickSequence(m_completedClickCount);
    m_completedClickCount = 0;
    m_clickWindowDeadlineMs = 0;
    return result;
}

ButtonGestureEngine::Gesture ButtonGestureEngine::finalizeClickSequence(uint8_t completedCount)
{
    switch (completedCount)
    {
        case 1:
            Serial.println("BUTTON: SINGLE_PRESS");
            return Gesture::SinglePress;
        case 2:
            Serial.println("BUTTON: DOUBLE_PRESS");
            return Gesture::DoublePress;
        case 3:
            Serial.println("BUTTON: TRIPLE_PRESS");
            return Gesture::TriplePress;
        case 4:
            // BRoadmap v1.3: Next Mode (Static/Motion/Reactive category
            // switch). Newly classified in Milestone 3 -- previously
            // fell into the "unclassified" default case below.
            Serial.println("BUTTON: FOUR_PRESS");
            return Gesture::FourPress;
        case 6:
            Serial.println("BUTTON: SIX_PRESS_POWER_CANDIDATE");
            return Gesture::SixPressPowerCandidate;
        case 10:
            Serial.println("BUTTON: TEN_PRESS_FACTORY_RESET_PENDING");
            return Gesture::TenPressFactoryResetPending;
        default:
            // Any other completed count (5, 7, 8, 9, 11+) is explicitly
            // out of scope for the finalized map. Reported for
            // visibility, deliberately not a "BUTTON: ..." line.
            Serial.print("Button: unclassified click count (");
            Serial.print(completedCount);
            Serial.println(") -- no action taken");
            return Gesture::Unclassified;
    }
}

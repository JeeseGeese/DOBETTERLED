#pragma once

/**
 * EngineeringConsole.h
 * -----------------------------------------------------------------------
 * Evolves BringUpDashboard into a permanent diagnostics interface.
 *
 * REVISION (button roadmap update): Developer/diagnostic access is
 * Serial-only. There is NO button-held-at-boot gesture, and no
 * "Developer Mode" as a distinct runtime state -- 's' (status) and 'm'
 * (menu) are simply always-available commands, exactly like 1-9/+/-,
 * with no gating of any kind. This removes the earlier button-gated
 * Developer Mode entirely (it had failed hardware acceptance -- LEDs
 * dark, Serial disconnect on 's' -- and, independent of root cause,
 * gating diagnostics behind a gesture on GPIO0, a boot-strapping pin,
 * was a fragile design on this hardware regardless). See
 * docs/DESIGN_LOG.md Section 11 for the full rationale. The physical
 * button is reserved entirely for product features going forward, per
 * the same revision.
 *
 * MILESTONE 2: adds a ButtonGestureEngine member for gesture detection
 * and Serial reporting only (docs/PRODUCT_SPEC.md Section 3, BRoadmap
 * v1.2). This class still owns the button pin itself (pinMode,
 * digitalRead for Button Test mode) -- it just also forwards the same
 * raw reading into the gesture engine each loop().
 *
 * MILESTONE 3: wires 1/2/3/4-press gestures to a new EffectEngine
 * member (Next/Previous Effect within Mode category, Next Palette,
 * Next Mode). Double Press + Hold toggles a tracked-only Audio Reactive
 * Overlay flag (no microphone input implemented). Long Hold, 6-press,
 * and 10-press remain detection/report-only -- no Quick Settings Menu,
 * no power toggle, no factory-reset erase. See PRODUCT_SPEC.md Section
 * 3, BRoadmap v1.3.
 *
 * MILESTONE 4A: adds an AudioInput member -- I2S mic hardware bring-up
 * only. `a` prints one-shot audio diagnostics, `A` toggles continuous
 * diagnostics. The Audio Reactive Overlay flag from Milestone 3 is
 * still just a flag here -- this milestone does NOT connect AudioInput
 * readings to it or to EffectEngine in any way. No LED behavior change.
 *
 * Scope boundary: this class is intentionally a standalone,
 * self-contained evolution of BringUpDashboard's own code -- it does
 * NOT include or depend on SystemManager, Hal, LEDDriver,
 * ButtonManager, SettingsManager, PaletteManager, EffectRegistry, or
 * AnimationManager. Those remain the dormant architecture tree (see
 * docs/ARCHITECTURE_CONTRACT.md) until a later milestone separately
 * proves that tree lights LEDs on real hardware. ButtonGestureEngine is
 * likewise standalone -- not an edit to the dormant ButtonManager.
 *
 * Command surface: 1-9, +, -, m behave identically to BringUpDashboard.
 * 's' is new, always available, read-only (status printout).
 * -----------------------------------------------------------------------
 */

#include <Arduino.h>
#include "ButtonGestureEngine.h"
#include "EffectEngine.h"
#include "AudioInput.h"

class EngineeringConsole
{
public:
    void begin();
    void update();

private:
    enum class Mode : uint8_t
    {
        SolidRed,
        SolidGreen,
        SolidBlue,
        SolidWhite,
        Rainbow,
        Chase,
        RelayOff,
        ButtonTest,
        Off,
        EffectEngineMode
    };

    // ---------------------------------------------------------------
    // Core state -- mirrors BringUpDashboard exactly; nothing here
    // changes existing rendering, relay, or brightness behavior.
    // ---------------------------------------------------------------
    Mode m_mode = Mode::SolidRed;
    const char* m_modeLabel = "Solid RED";
    uint8_t m_brightness = 80;
    uint8_t m_hue = 0;
    uint8_t m_chaseIndex = 0;
    unsigned long m_lastFrameMs = 0;
    unsigned long m_lastStatusMs = 0;
    bool m_relayOn = true;

    // Milestone 2: gesture detection, now also returns a Gesture enum
    // (Milestone 3) so this class can react to specific gestures. See
    // ButtonGestureEngine.h for why it's standalone and hardware-free.
    ButtonGestureEngine m_gestureEngine;

    // Milestone 3: standalone effect/palette rendering. See
    // EffectEngine.h for why it's standalone and hardware-free.
    EffectEngine m_effectEngine;

    // Tracked-only Audio Reactive Overlay flag. No microphone input is
    // implemented -- toggling this only changes what gets Serial-
    // printed. See docs/PRODUCT_SPEC.md Section 10. Milestone 4A does
    // NOT connect this flag to m_audioInput or to any LED behavior.
    bool m_audioReactiveOverlay = false;

    // Milestone 4A: standalone I2S mic bring-up. See AudioInput.h for
    // why it owns the peripheral directly instead of being hardware-
    // decoupled like ButtonGestureEngine/EffectEngine.
    AudioInput m_audioInput;
    bool m_continuousAudioDiag = false;
    unsigned long m_lastAudioDiagMs = 0;

    void printMenu();
    void handleSerial();
    void handleGesture(ButtonGestureEngine::Gesture gesture);
    void enterEffectEngineMode();
    void setMode(Mode mode, const char* label);
    void renderFrame();
    void renderSolid(uint8_t r, uint8_t g, uint8_t b);
    void renderRainbow();
    void renderChase();
    void clearStrip();
    void applyRelay(bool on);
    void applyBrightness();
    void printButtonStatus();
    void printAudioDiagnostics();

    // Read-only: touches no GPIO, no FastLED buffer, no relay -- only
    // prints already-tracked state plus millis()/digitalRead()/
    // ESP.getFreeHeap(). Only ever called from handleSerial()'s 's'
    // case -- never automatically, never during begin().
    void printBanner();
    void printEngineeringStatus();
};

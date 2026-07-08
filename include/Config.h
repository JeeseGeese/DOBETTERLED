/**
 * Config.h
 * -----------------------------------------------------------------------
 * Firmware-generic configuration: timing, tunable defaults, and limits
 * that apply to the platform regardless of which product it's running
 * on. Nothing board-specific lives here -- see ProductConfig.h for pin
 * assignments, LED count, relay presence, and mic bus wiring.
 *
 * Rule of thumb: if a future hardware target with a different pinout
 * would need to change a value, it belongs in ProductConfig.h, not here.
 * If every product built from this firmware would plausibly want the
 * same value (or the same tuning knob with a different number), it
 * belongs here.
 * -----------------------------------------------------------------------
 */

#pragma once

#include <stdint.h>

// =========================================================================
// Firmware Identity & Version
// -------------------------------------------------------------------------
// Identifies the firmware PLATFORM itself, independent of which product
// it's running on (that's ProductConfig::Name). Bump these deliberately
// per the project's version history / release notes, not casually.
// =========================================================================

namespace FirmwareInfo
{
    constexpr const char* Name = "DOBETTERLED";

    constexpr uint8_t VersionMajor = 1; // incompatible settings/behavior changes
    constexpr uint8_t VersionMinor = 0; // new features, backward compatible
    constexpr uint8_t VersionPatch = 2; // fixes, no behavior change
}

// =========================================================================
// LED Behavior Limits & Defaults (generic -- actual LED count/pins are
// in ProductConfig.h)
// =========================================================================

namespace LedConfig
{
    constexpr uint8_t MaxBrightness     = 255;
    constexpr uint8_t DefaultBrightness = 128;
    constexpr uint8_t MinBrightness     = 4; // never let "on" mean invisible
}

// =========================================================================
// Timing / Performance
// =========================================================================

namespace Timing
{
    // Target render rate. 16-17 ms per frame ~= 60 FPS.
    constexpr uint16_t TargetFrameIntervalMs = 16;

    // How often ButtonManager polls the raw pin state via the HAL.
    constexpr uint16_t ButtonPollIntervalMs = 4;

    // Debounce: raw pin must be stable for this long to count as a real edge.
    constexpr uint16_t DebounceMs = 30;

    // Maximum gap between releases that still counts as part of the same
    // multi-click sequence (single/double/triple/quad press detection).
    constexpr uint16_t MultiClickWindowMs = 350;

    // Press duration thresholds.
    constexpr uint16_t LongPressMinMs        = 1000; // long press: 1-2s -> on/off toggle
    constexpr uint16_t LongPressMaxMs        = 2000;
    constexpr uint16_t BrightnessHoldStartMs = 2000; // holding past this starts brightness ramp
    constexpr uint16_t FactoryResetMinMs     = 5000; // very long press: 5-10s -> factory reset
    constexpr uint16_t FactoryResetMaxMs     = 10000;

    // Brightness ramp behavior while the button is held.
    constexpr uint16_t BrightnessRampFullSweepMs = 2500; // time for a 0->255 sweep
    constexpr uint16_t BrightnessRampStepMs      = 20;   // how often the ramp updates

    // Settings persistence: writes to NVS are debounced so rapid changes
    // (e.g. brightness ramping) don't hammer flash.
    constexpr uint16_t SettingsSaveDebounceMs = 3000;

    // Visual feedback timing (spec Section 9).
    constexpr uint16_t StartupFadeMs      = 600;  // fade-in on power-up
    constexpr uint16_t ShutdownFadeMs     = 500;  // fade-out on power-down
    constexpr uint16_t EffectTransitionMs = 250;  // blend when switching effect/palette
}

// =========================================================================
// Animation Defaults
// =========================================================================

namespace AnimationConfig
{
    constexpr uint8_t DefaultSpeed       = 128; // 0-255
    constexpr uint8_t DefaultIntensity   = 128; // 0-255
    constexpr uint8_t DefaultAnimationId = 0;   // index into EffectRegistry
    constexpr uint8_t DefaultPaletteId   = 0;   // index into PaletteManager registry

    // Default primary/secondary colors, used until a future input method
    // lets the user set these directly (spec Section 6 / 13).
    constexpr uint8_t DefaultPrimaryR = 255, DefaultPrimaryG = 255, DefaultPrimaryB = 255;
    constexpr uint8_t DefaultSecondaryR = 0, DefaultSecondaryG = 0, DefaultSecondaryB = 0;
}

// =========================================================================
// Sound Reactive Configuration (generic tuning -- bus/pins are in
// ProductConfig.h::Mic)
// =========================================================================

namespace SoundConfig
{
    // Sample rate for I2S capture.
    constexpr uint32_t SampleRateHz = 16000;

    // Number of samples pulled per read cycle.
    constexpr uint16_t SamplesPerRead = 256;

    // Noise floor calibration: how long to listen before trusting
    // volume/beat readings. Runs on first activation of any non-Disabled
    // audio mode, not at boot (see spec Section 1.6).
    constexpr uint16_t CalibrationDurationMs = 1500;

    // Smoothing factor for envelope follower (applied as an EMA alpha --
    // higher = more responsive, lower = smoother).
    constexpr float EnvelopeAttackAlpha  = 0.6f;
    constexpr float EnvelopeReleaseAlpha = 0.05f;

    // Automatic gain control bounds.
    constexpr float AgcMinGain = 1.0f;
    constexpr float AgcMaxGain = 8.0f;

    // Minimum interval between beat detections (prevents double-triggering).
    constexpr uint16_t MinBeatIntervalMs = 120;
}

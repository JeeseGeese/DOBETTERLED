#pragma once

/**
 * AudioInput.h
 * -----------------------------------------------------------------------
 * Milestone 4A: audio hardware bring-up only. Answers one question --
 * "can DOBETTERLED read the microphone reliably?" -- nothing more. No
 * FFT, no beat detection, no effect modulation. See docs/DESIGN_LOG.md
 * for why this is its own milestone before Milestone 4C touches
 * EffectEngine at all.
 *
 * Unlike ButtonGestureEngine/EffectEngine, this class is NOT hardware-
 * decoupled -- its entire job is the hardware read, so it owns the I2S
 * peripheral directly. This mirrors the pattern EngineeringConsole
 * already uses for GPIO/relay/FastLED: a standalone class that touches
 * real hardware, but stays independent of the dormant tree.
 *
 * Hardware: QuinLED Dig2Go built-in ICS-43434 digital I2S MEMS mic.
 * Pin/port/rate values below match include/ProductConfig.h's
 * Product::Pins::MicSerialClock/MicWordSelect/MicSerialData,
 * Product::Mic::I2sPortNumber, and include/Config.h's
 * SoundConfig::SampleRateHz/SamplesPerRead -- hard-coded locally here
 * rather than #including ProductConfig.h/Config.h, the same pattern
 * EngineeringConsole.cpp already uses for its own LED/relay/button
 * pins, to keep this class standalone and independent of the dormant
 * tree (ProductConfig.h's own header comment restricts direct
 * inclusion to Hal.cpp/LEDDriver.cpp).
 *
 * IMPORTANT: these pins are a confirmed hardware fact (the ICS-43434 is
 * physically wired at SD=GPIO19, WS=GPIO4, SCK=GPIO18, I2S port 0), but
 * the I2S *init sequence* itself has never been hardware-verified
 * before this milestone -- the dormant src/HAL/Hal.cpp contains an
 * equivalent, also-never-tested I2S config that this class's begin()
 * was cross-checked against, but is not included or called. Treat
 * every stat below as a first-pass, un-calibrated estimate until the
 * `a`/`A` Serial diagnostics confirm real readings on real hardware.
 * -----------------------------------------------------------------------
 */

#include <Arduino.h>

class AudioInput
{
public:
    // Attempts to start the I2S peripheral for the onboard mic. Safe to
    // call even if the mic is missing/miswired/fails to init -- sets
    // isAvailable() false and every stat stays at a neutral 0 rather
    // than crashing or hanging.
    void begin();

    // Pulls any newly available I2S samples (non-blocking) and updates
    // raw/peak/average/noiseFloor/level. No-op if begin() failed. Call
    // every loop() iteration.
    void update();

    // Whether the I2S peripheral was successfully claimed for the mic.
    // NOTE: this reflects driver init success, not proof a physical mic
    // is actually connected -- garbage-but-present data vs. no data at
    // all is what the `a` diagnostics are for.
    bool isAvailable() const;

    // Last raw sample this update(), shifted down from the I2S word
    // into a smaller signed range (see .cpp -- provisional scaling).
    int32_t raw() const;

    // Normalized loudness above the noise floor, 0-255.
    uint8_t level() const;

    // Peak |sample| seen recently, with slow decay so it's readable on
    // a Serial print instead of flickering every call.
    int32_t peak() const;

    // Smoothed RMS-style envelope of |sample| (exponential moving
    // average of each read batch's RMS) -- this is the "how loud right
    // now" value level()/noiseFloor() are both derived from.
    int32_t average() const;

    // Slow-moving estimate of the RMS floor during quiet periods.
    // Minimum-following, not full AGC/calibration.
    int32_t noiseFloor() const;

private:
    bool m_i2sStarted = false;

    int32_t m_raw = 0;
    int32_t m_peak = 0;
    int32_t m_average = 0;
    int32_t m_noiseFloor = 0;
    uint8_t m_level = 0;

    void resetStats();
};

/**
 * Hal.h
 * -----------------------------------------------------------------------
 * Hardware Abstraction Layer -- the ONLY module (besides the narrow,
 * documented FastLED-template exception in LEDDriver.cpp) permitted to
 * touch GPIO, digitalRead/Write, or the I2S peripheral directly.
 *
 * Strict boundary:
 *   - HAL abstracts PHYSICAL HARDWARE ACCESS ONLY.
 *   - HAL contains NO product behavior or application logic.
 *   - HAL knows NOTHING about animations, settings, button gestures,
 *     or audio modes -- it does not include Types.h and never will.
 *   - Debouncing, click counting, hold-duration classification, factory
 *     reset thresholds, etc. all live in the managers that call this
 *     interface (ButtonManager, SoundManager, LEDDriver), not here.
 *
 * Architecture position:
 *
 *     SystemManager
 *          |
 *       Managers   (ButtonManager, LEDDriver, SoundManager, ...)
 *          |
 *         HAL       <-- this file
 *          |
 *      Hardware     (GPIO, I2S, ADC)
 *
 * Hal.cpp is the only .cpp file that includes ProductConfig.h (other
 * than LEDDriver.cpp's compile-time FastLED pin template, which is a
 * documented exception -- see LEDDriver.cpp when we write it). This is
 * what lets a future product target supply its own ProductConfig.h and
 * reuse every manager above this line unmodified.
 * -----------------------------------------------------------------------
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace Hal
{
    // ---------------------------------------------------------------
    // Lifecycle
    // ---------------------------------------------------------------

    // Configures pin modes for always-on hardware (button, relay).
    // Does NOT touch the microphone -- that is initialized lazily via
    // micBegin(), only when a manager actually needs it.
    void begin();

    // ---------------------------------------------------------------
    // Button
    // ---------------------------------------------------------------

    // Returns the raw, instantaneous physical state of the button pin.
    // No debouncing, no edge detection, no gesture logic -- a single
    // digitalRead translated to "is it physically pressed right now."
    // ButtonManager is responsible for everything built on top of this.
    bool isButtonPressed();

    // ---------------------------------------------------------------
    // LED Power Relay
    // ---------------------------------------------------------------

    // Energizes or de-energizes the LED power relay, if this product
    // has one. No-op (returns immediately) if hasLedPowerRelay() is
    // false -- callers should check that first if they need to know
    // whether physical power was actually cut.
    void setLedPower(bool energized);

    // Whether this product's hardware has a relay capable of fully
    // cutting LED power. LEDDriver uses this to decide whether "off"
    // means relay-cut or brightness-to-zero.
    bool hasLedPowerRelay();

    // ---------------------------------------------------------------
    // Microphone (I2S)
    // ---------------------------------------------------------------

    // Starts the I2S peripheral for microphone capture. Safe to call
    // repeatedly (no-ops if already started). Returns false if this
    // product has no usable microphone or init fails.
    bool micBegin();

    // Stops the I2S peripheral, releasing the hardware resource. Called
    // when audio mode returns to Disabled, so idle power/CPU isn't
    // spent on sampling nobody is using.
    void micEnd();

    // Reads up to maxSamples raw I2S samples into buffer. Returns the
    // number of samples actually read (may be less than maxSamples, or
    // zero -- this call does not block waiting for a full buffer).
    // SoundManager owns all interpretation of these raw values
    // (calibration, AGC, envelope, beat detection) -- HAL only moves
    // bytes off the peripheral.
    size_t micReadSamples(int32_t* buffer, size_t maxSamples);

    // Whether this product's hardware exposes a usable microphone at
    // all. SoundManager can check this once at audio-mode activation
    // rather than assuming every product has one.
    bool isMicAvailable();
}

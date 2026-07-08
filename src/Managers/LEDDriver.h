/**
 * LEDDriver.h
 * -----------------------------------------------------------------------
 * Encapsulates FastLED completely. No other module in this firmware may
 * include <FastLED.h> or reference a CRGB directly -- everything outside
 * this file speaks in RgbColor (Types.h), which LEDDriver.cpp converts
 * internally.
 *
 * Scope for this milestone (deliberately excluded, per instruction):
 *   - Fade-in/fade-out and effect/palette blend transitions are NOT
 *     implemented here yet. setPower() is instant, not smooth. Those
 *     belong to a later milestone once the rest of the framework exists
 *     to coordinate them.
 *
 * Responsibilities:
 *   - Initialize LED hardware (FastLED + the data pin)
 *   - Own the pixel buffer
 *   - Apply brightness
 *   - Render frames (push the buffer to the physical strip)
 *   - Isolate FastLED from the rest of the codebase
 *
 * Architecture position: a Manager, sitting between SystemManager and
 * the HAL. Uses Hal::setLedPower()/hasLedPowerRelay() for the relay,
 * and is the one documented exception that includes ProductConfig.h
 * directly (in the .cpp) -- solely because FastLED's addLeds<>() needs
 * the data pin as a compile-time template argument, which a HAL
 * function call cannot supply.
 * -----------------------------------------------------------------------
 */

#pragma once

#include "Types.h" // RgbColor
#include "Config.h" // LedConfig::DefaultBrightness
#include <stdint.h>

class LEDDriver
{
public:
    // Initializes FastLED against the product's LED data pin and LED
    // count, sets initial brightness, and clears the strip to black.
    void begin();

    // ---------------------------------------------------------------
    // Pixel buffer
    // ---------------------------------------------------------------

    // Sets a single pixel in the buffer. No-ops silently if index is
    // out of range rather than crashing -- effects should not need to
    // bounds-check against getLedCount() on every write.
    void setPixel(uint16_t index, const RgbColor& color);

    // Sets every pixel in the buffer to the given color.
    void fill(const RgbColor& color);

    // Sets every pixel in the buffer to black. Does not call show() --
    // callers decide when to push the frame.
    void clear();

    // Number of physical LEDs actually in use on this product.
    uint16_t getLedCount() const;

    // ---------------------------------------------------------------
    // Brightness
    // ---------------------------------------------------------------

    // Applies a global brightness scale (0-255) to all future show()
    // calls. Takes effect immediately -- does not fade.
    void setBrightness(uint8_t brightness);

    uint8_t getBrightness() const;

    // ---------------------------------------------------------------
    // Rendering
    // ---------------------------------------------------------------

    // Pushes the current pixel buffer to the physical strip at the
    // current brightness. Effects/managers call this once per frame.
    void show();

    // ---------------------------------------------------------------
    // Power (instant only -- no fade yet, see file header note)
    // ---------------------------------------------------------------

    // Energizes or de-energizes LED power. If this product has a relay
    // (hasPowerRelay() true), drives it via the HAL. Also clears and
    // pushes a black frame when turning off, so the strip goes dark
    // immediately even on products without a relay, or during any
    // relay switching delay.
    void setPower(bool energized);

    // Whether this product's hardware can fully cut LED power via a
    // relay, versus only being able to fall back to brightness = 0.
    bool hasPowerRelay() const;

private:
    uint8_t m_brightness = LedConfig::DefaultBrightness;
};

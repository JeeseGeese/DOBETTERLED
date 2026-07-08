/**
 * LEDDriver.cpp
 * -----------------------------------------------------------------------
 * The only file in this firmware that includes <FastLED.h>. Every other
 * module -- effects, managers, SystemManager -- speaks RgbColor only.
 *
 * Also one of two files (with Hal.cpp) that includes ProductConfig.h
 * directly. Here, that's solely to supply Product::Pins::LedData and
 * Product::Led::Count/MaxCount as compile-time constants: FastLED's
 * addLeds<>() requires the data pin as a template argument, which
 * cannot be satisfied through a runtime HAL function call. This is a
 * documented, narrow exception to "only the HAL touches hardware,"
 * required by FastLED's API shape, not a break in the boundary --
 * LEDDriver still never touches a GPIO register itself; FastLED does.
 * -----------------------------------------------------------------------
 */

#include "LEDDriver.h"
#include "ProductConfig.h" // exception: compile-time FastLED template args only
#include "../HAL/Hal.h"

#include <Arduino.h> // TEMPORARY -- pinMode/digitalWrite/delay for the diagnostic below
#include <FastLED.h>

namespace
{
    // Sized to the product's upper bound so the buffer never needs to
    // reallocate; only the first Product::Led::Count entries are ever
    // pushed to the strip.
    CRGB g_leds[Product::Led::MaxCount];

    // FastLED's chipset is a compile-time template argument, so it can't
    // be selected from Product::Led::Chipset at runtime. This assertion
    // just ensures that if a future ProductConfig ever changes Chipset,
    // whoever does it is forced to notice and update the addLeds<> call
    // below to match, rather than the two silently drifting apart.
    static_assert(Product::Led::Chipset == Product::Led::ChipsetFamily::WS2812B,
        "LEDDriver.cpp hard-codes the WS2812B FastLED template argument -- "
        "update the FastLED.addLeds<>() call below to match ProductConfig::Led::Chipset.");

    // Same reasoning as the chipset assertion above, for color order --
    // confirmed via hardware isolation test to be GRB, not RGB or BRG.
    static_assert(Product::Led::Order == Product::Led::ColorOrderFamily::GRB,
        "LEDDriver.cpp hard-codes the GRB FastLED template argument -- "
        "update the FastLED.addLeds<>() call below to match ProductConfig::Led::Order.");

    inline CRGB toCRGB(const RgbColor& color)
    {
        return CRGB(color.r, color.g, color.b);
    }
}

void LEDDriver::begin()
{
    // =====================================================================
    // TEMPORARY DIAGNOSTIC -- inlines the exact sequence from the working
    // standalone isolation sketch, as literally as possible, bypassing
    // every layer of indirection (Hal::setLedPower, ProductConfig
    // constants, RgbColor conversion) to rule them out one at a time.
    //
    // RESTORE, once root cause is found, to:
    //
    //   FastLED.addLeds<WS2812B, Product::Pins::LedData, GRB>(g_leds, Product::Led::Count);
    //   m_brightness = LedConfig::DefaultBrightness;
    //   FastLED.setBrightness(m_brightness);
    //   clear();
    //   show();
    //
    // -- and delete this whole diagnostic block, including the raw
    // pinMode/digitalWrite calls below (relay control belongs in Hal,
    // not here -- this is a deliberate, temporary exception).
    // =====================================================================

    pinMode(12, OUTPUT);
    digitalWrite(12, HIGH);
    delay(100); // diagnostic only

    FastLED.addLeds<WS2812B, 16, GRB>(g_leds, 15);
    FastLED.setBrightness(80);
    fill_solid(g_leds, 15, CRGB::Red);
    FastLED.show();

    delay(2000); // diagnostic only -- hold so it's visible

    // NOTE: m_brightness is intentionally NOT updated here, and clear()/
    // show() are intentionally NOT called again, so nothing in this
    // function can blank the strip after the diagnostic fill above.
    // ===================== END TEMPORARY DIAGNOSTIC =====================
}

void LEDDriver::setPixel(uint16_t index, const RgbColor& color)
{
    if (index >= Product::Led::Count)
    {
        return; // silently ignore out-of-range writes
    }
    g_leds[index] = toCRGB(color);
}

void LEDDriver::fill(const RgbColor& color)
{
    const CRGB c = toCRGB(color);
    for (uint16_t i = 0; i < Product::Led::Count; ++i)
    {
        g_leds[i] = c;
    }
}

void LEDDriver::clear()
{
    fill(RgbColor(0, 0, 0));
}

uint16_t LEDDriver::getLedCount() const
{
    return Product::Led::Count;
}

void LEDDriver::setBrightness(uint8_t brightness)
{
    m_brightness = brightness;
    FastLED.setBrightness(m_brightness);
}

uint8_t LEDDriver::getBrightness() const
{
    return m_brightness;
}

void LEDDriver::show()
{
    FastLED.show();
}

void LEDDriver::setPower(bool energized)
{
    if (hasPowerRelay())
    {
        Hal::setLedPower(energized);
    }

    if (!energized)
    {
        // Ensures the strip reads black immediately, whether or not a
        // relay is present, and covers any relay switching delay.
        // No fade here yet -- instant only, per this milestone's scope.
        clear();
        show();
    }
}

bool LEDDriver::hasPowerRelay() const
{
    return Hal::hasLedPowerRelay();
}

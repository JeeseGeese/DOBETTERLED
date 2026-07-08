/**
 * ProductConfig.h
 * -----------------------------------------------------------------------
 * Hardware identity for ONE specific product target: the QuinLED Dig2Go.
 *
 * This file answers "what board am I running on" -- pin assignments,
 * physical LED count, whether a relay exists, which mic bus is wired
 * where. It does NOT contain firmware behavior tuning (that's Config.h).
 *
 * A second hardware target (a future product in this firmware family)
 * gets its own ProductConfig.h with its own pin map and constants, and
 * everything else in the firmware -- HAL, managers, effects -- is
 * reused unmodified. The Dig2Go is a supported target, not the
 * firmware's identity.
 *
 * Only Hal.cpp (and, for the FastLED template pin argument specifically,
 * LEDDriver.cpp -- see the note in Hal.h) may include this file directly.
 * Every other module receives hardware facts through the HAL's function
 * interface, never by reading these constants itself.
 * -----------------------------------------------------------------------
 */

#pragma once

#include <stdint.h>

namespace Product
{
    // Human-readable identity, useful for boot logging / future serial menu.
    constexpr const char* Name     = "QuinLED Dig2Go";
    constexpr const char* Revision = "single hardware revision (no variants)";

    // ---------------------------------------------------------------
    // Pin Map
    // ---------------------------------------------------------------
    namespace Pins
    {
        // Onboard user button (active LOW, needs INPUT_PULLUP)
        constexpr uint8_t Button = 0;

        // Level-shifted addressable LED data output
        constexpr uint8_t LedData = 16;

        // LED relay -- cuts power to the strip completely (HIGH = powered)
        constexpr uint8_t LedRelay = 12;

        // Built-in ICS-43434 I2S digital MEMS microphone
        constexpr uint8_t MicSerialData  = 19; // SD  (I2S data in)
        constexpr uint8_t MicWordSelect  = 4;  // WS  (I2S word select / LR clock)
        constexpr uint8_t MicSerialClock = 18; // SCK (I2S bit clock)

        // IR receiver (reserved for future use)
        constexpr uint8_t IrReceiver = 5;

        // Expansion header (reserved for future modules)
        constexpr uint8_t ExpansionSda   = 21;
        constexpr uint8_t ExpansionScl   = 22;
        constexpr uint8_t ExpansionGpioA = 23;
        constexpr uint8_t ExpansionGpioB = 25; // ADC-capable
    }

    // ---------------------------------------------------------------
    // LED Hardware Facts
    // ---------------------------------------------------------------
    namespace Led
    {
        // Physical LED count actually connected on this product/build.
        // Confirmed via hardware isolation test: 15 WS2812B LEDs on the
        // Dig2Go's 3-pin output, GPIO16, all lit correctly at GRB.
        constexpr uint16_t Count = 15;

        // Upper bound used to size static buffers in the generic layer.
        // Must be >= Count for any variant built from this firmware base.
        constexpr uint16_t MaxCount = 100;

        // Does this board have a relay that can fully cut LED power?
        // The Dig2Go does. A future bare-ESP32 product might not --
        // that product's ProductConfig would set this false, and
        // LEDDriver falls back to brightness-to-zero on power-off.
        constexpr bool HasPowerRelay = true;

        // FastLED chipset/color-order identity for this product's strip.
        // Named here (not as raw FastLED template args) so ProductConfig
        // stays declarative; LEDDriver.cpp is what actually spells the
        // FastLED template using these names, and asserts against them
        // so the two can't silently drift apart.
        enum class ChipsetFamily : uint8_t { WS2812B };
        constexpr ChipsetFamily Chipset = ChipsetFamily::WS2812B;

        // Confirmed via hardware isolation test: GRB, not RGB or BRG.
        enum class ColorOrderFamily : uint8_t { GRB, RGB, BRG };
        constexpr ColorOrderFamily Order = ColorOrderFamily::GRB;
    }

    // ---------------------------------------------------------------
    // Microphone Hardware Facts
    // ---------------------------------------------------------------
    namespace Mic
    {
        // This product's mic is digital I2S (ICS-43434), not analog ADC.
        // A future product with an analog mic would flip this and the
        // HAL's audio init path would branch accordingly.
        constexpr bool IsI2S = true;

        constexpr uint8_t I2sPortNumber = 0;
    }
}

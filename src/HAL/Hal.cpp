/**
 * Hal.cpp
 * -----------------------------------------------------------------------
 * Implementation of the hardware abstraction interface declared in
 * Hal.h, for the QuinLED Dig2Go target.
 *
 * This is one of only two files in the firmware that include
 * ProductConfig.h (the other being LEDDriver.cpp, for the compile-time
 * FastLED template pin argument -- see the note there). Every hardware
 * fact this file needs -- which pin is the button, whether a relay
 * exists, which pins carry the I2S mic bus -- comes from ProductConfig,
 * never a hard-coded number.
 *
 * Config.h is included only for generic, non-board-specific tuning
 * (SoundConfig::SampleRateHz, SamplesPerRead) -- never for pin facts.
 * -----------------------------------------------------------------------
 */

#include "Hal.h"
#include "ProductConfig.h"
#include "Config.h"

#include <Arduino.h>
#include <driver/i2s.h>

namespace
{
    bool g_micInitialized = false;

    constexpr i2s_port_t kMicI2sPort =
        static_cast<i2s_port_t>(Product::Mic::I2sPortNumber);
}

namespace Hal
{

void begin()
{
    // Button: active LOW, pulled up internally. Pressing it connects
    // the pin to ground.
    pinMode(Product::Pins::Button, INPUT_PULLUP);

    // Relay: only configure the pin if this product actually has one.
    // Starts de-energized -- SystemManager decides when to enable it
    // during the startup sequence (spec Section 1), not the HAL.
    if (Product::Led::HasPowerRelay)
    {
        pinMode(Product::Pins::LedRelay, OUTPUT);
        digitalWrite(Product::Pins::LedRelay, LOW);
    }

    // Microphone is intentionally NOT touched here. I2S is started
    // lazily by micBegin(), only when a manager activates a non-Disabled
    // audio mode (spec Section 1.6) -- this keeps boot fast and avoids
    // holding the I2S peripheral for devices that never use sound mode.
}

bool isButtonPressed()
{
    return digitalRead(Product::Pins::Button) == LOW;
}

void setLedPower(bool energized)
{
    if (!Product::Led::HasPowerRelay)
    {
        return; // nothing to do on a product with no relay
    }
    digitalWrite(Product::Pins::LedRelay, energized ? HIGH : LOW);
}

bool hasLedPowerRelay()
{
    return Product::Led::HasPowerRelay;
}

bool micBegin()
{
    if (g_micInitialized)
    {
        return true; // already running, nothing to do
    }

    if (!Product::Mic::IsI2S)
    {
        return false; // this product build has no I2S mic wired
    }

    i2s_config_t i2sConfig = {};
    i2sConfig.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
    i2sConfig.sample_rate = SoundConfig::SampleRateHz;
    i2sConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    i2sConfig.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2sConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2sConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2sConfig.dma_buf_count = 4;
    i2sConfig.dma_buf_len = SoundConfig::SamplesPerRead;
    i2sConfig.use_apll = false;
    i2sConfig.tx_desc_auto_clear = false;
    i2sConfig.fixed_mclk = 0;

    if (i2s_driver_install(kMicI2sPort, &i2sConfig, 0, nullptr) != ESP_OK)
    {
        return false;
    }

    i2s_pin_config_t pinConfig = {};
    pinConfig.bck_io_num = Product::Pins::MicSerialClock;
    pinConfig.ws_io_num = Product::Pins::MicWordSelect;
    pinConfig.data_out_num = I2S_PIN_NO_CHANGE;
    pinConfig.data_in_num = Product::Pins::MicSerialData;

    if (i2s_set_pin(kMicI2sPort, &pinConfig) != ESP_OK)
    {
        i2s_driver_uninstall(kMicI2sPort);
        return false;
    }

    g_micInitialized = true;
    return true;
}

void micEnd()
{
    if (!g_micInitialized)
    {
        return;
    }
    i2s_driver_uninstall(kMicI2sPort);
    g_micInitialized = false;
}

size_t micReadSamples(int32_t* buffer, size_t maxSamples)
{
    if (!g_micInitialized || buffer == nullptr || maxSamples == 0)
    {
        return 0;
    }

    const size_t maxBytes = maxSamples * sizeof(int32_t);
    size_t bytesRead = 0;

    // Short timeout (5ms), not a blocking wait for a full buffer -- the
    // HAL must never stall the caller's loop. SoundManager is expected
    // to poll this every iteration and tolerate a partial or empty read.
    esp_err_t result = i2s_read(kMicI2sPort, buffer, maxBytes, &bytesRead, pdMS_TO_TICKS(5));
    if (result != ESP_OK)
    {
        return 0;
    }

    return bytesRead / sizeof(int32_t);
}

bool isMicAvailable()
{
    return Product::Mic::IsI2S;
}

} // namespace Hal

#include "AudioInput.h"

#include <driver/i2s.h>
#include <math.h>

namespace
{
    // ICS-43434 digital I2S MEMS mic wiring on the Dig2Go -- see the
    // header comment for why these are hard-coded here instead of
    // #including ProductConfig.h.
    constexpr uint8_t MIC_SCK_PIN = 18; // bit clock
    constexpr uint8_t MIC_WS_PIN  = 4;  // word select / LR clock
    constexpr uint8_t MIC_SD_PIN  = 19; // serial data in
    constexpr i2s_port_t MIC_I2S_PORT = I2S_NUM_0;

    constexpr uint32_t SAMPLE_RATE_HZ = 16000;
    constexpr uint16_t SAMPLES_PER_READ = 256;

    constexpr int32_t PEAK_DECAY_PER_UPDATE = 400;   // how fast the displayed peak relaxes
    constexpr float AVERAGE_ALPHA = 0.2f;             // envelope smoothing (higher = more responsive)
    constexpr float NOISE_FLOOR_RISE_ALPHA = 0.001f;  // how slowly the floor is allowed to creep up

    // The ICS-43434 delivers 24-bit data left-justified in the 32-bit
    // I2S word; >>11 brings that into a smaller signed range that's
    // easier to eyeball on a Serial print. RAW_SHIFT is unchanged from
    // the first pass -- Peak/Average already showed good dynamic range
    // (hundreds to low thousands, no clipping/pinning) at this shift.
    constexpr int RAW_SHIFT = 11;

    // Calibrated from a real hardware capture (this milestone's `A`
    // continuous-diagnostics session): quiet-room noiseFloor settled
    // around ~224, average during normal speech/room noise ran
    // ~250-435, i.e. only ~10-200 above the floor. The original guess
    // of 20000 here was ~100x too large, which is why Level sat at 0-2
    // the whole time despite Average/Peak clearly moving. 400 gives a
    // moderate speaking voice roughly the middle of the 0-255 range;
    // still a first-pass calibration, not a proper dB scale, and may
    // want further tuning once Milestone 4B/4C exercise it with music.
    constexpr int32_t ASSUMED_MAX_ABOVE_FLOOR = 400;
}

void AudioInput::begin()
{
    resetStats();

    i2s_config_t i2sConfig = {};
    i2sConfig.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
    i2sConfig.sample_rate = SAMPLE_RATE_HZ;
    i2sConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    // The ICS-43434's L/R select pin is hardwired on the board (not a
    // GPIO we control), and which side it's tied to isn't documented
    // anywhere in this repo. First hardware test came back flat 0
    // (Raw/Peak/Average all exactly zero, not just quiet) with LEFT --
    // the classic symptom of reading the inactive channel slot.
    // Flipped to RIGHT as the next thing to try.
    i2sConfig.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
    i2sConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2sConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2sConfig.dma_buf_count = 4;
    i2sConfig.dma_buf_len = SAMPLES_PER_READ;
    i2sConfig.use_apll = false;
    i2sConfig.tx_desc_auto_clear = false;
    i2sConfig.fixed_mclk = 0;

    if (i2s_driver_install(MIC_I2S_PORT, &i2sConfig, 0, nullptr) != ESP_OK)
    {
        Serial.println("AudioInput: WARNING -- i2s_driver_install failed. Audio unavailable.");
        m_i2sStarted = false;
        return;
    }

    i2s_pin_config_t pinConfig = {};
    pinConfig.bck_io_num = MIC_SCK_PIN;
    pinConfig.ws_io_num = MIC_WS_PIN;
    pinConfig.data_out_num = I2S_PIN_NO_CHANGE;
    pinConfig.data_in_num = MIC_SD_PIN;

    if (i2s_set_pin(MIC_I2S_PORT, &pinConfig) != ESP_OK)
    {
        Serial.println("AudioInput: WARNING -- i2s_set_pin failed. Audio unavailable.");
        i2s_driver_uninstall(MIC_I2S_PORT);
        m_i2sStarted = false;
        return;
    }

    m_i2sStarted = true;
    Serial.println("AudioInput: I2S mic initialized (SCK=18, WS=4, SD=19, port 0).");
}

void AudioInput::resetStats()
{
    m_raw = 0;
    m_peak = 0;
    m_average = 0;
    m_noiseFloor = 0;
    m_level = 0;
}

void AudioInput::update()
{
    if (!m_i2sStarted)
    {
        return;
    }

    static int32_t buffer[SAMPLES_PER_READ];
    size_t bytesRead = 0;

    // Zero-tick timeout: never block the caller's loop() waiting on the
    // mic. DMA keeps filling in the background (dma_buf_count=4), so
    // this just drains whatever's already ready -- often nothing on a
    // given call, which is fine; stats simply hold their last value.
    const esp_err_t result = i2s_read(MIC_I2S_PORT, buffer, sizeof(buffer), &bytesRead, 0);
    if (result != ESP_OK || bytesRead == 0)
    {
        return;
    }

    const size_t samplesRead = bytesRead / sizeof(int32_t);
    if (samplesRead == 0)
    {
        return;
    }

    int64_t sumSquares = 0;
    int32_t batchPeak = 0;
    int32_t lastSample = 0;

    for (size_t i = 0; i < samplesRead; ++i)
    {
        const int32_t sample = buffer[i] >> RAW_SHIFT;
        lastSample = sample;

        const int32_t absSample = sample < 0 ? -sample : sample;
        if (absSample > batchPeak)
        {
            batchPeak = absSample;
        }
        sumSquares += static_cast<int64_t>(sample) * static_cast<int64_t>(sample);
    }

    m_raw = lastSample;

    const double meanSquare = static_cast<double>(sumSquares) / static_cast<double>(samplesRead);
    const int32_t batchRms = static_cast<int32_t>(sqrt(meanSquare));

    // Envelope: exponential moving average of each batch's RMS. This is
    // the "RMS-style level" the task asked for -- a smoothed loudness
    // value, not an instantaneous sample.
    m_average = static_cast<int32_t>((1.0f - AVERAGE_ALPHA) * static_cast<float>(m_average)
                                      + AVERAGE_ALPHA * static_cast<float>(batchRms));

    if (batchPeak > m_peak)
    {
        m_peak = batchPeak;
    }
    else if (m_peak > 0)
    {
        m_peak = (m_peak > PEAK_DECAY_PER_UPDATE) ? (m_peak - PEAK_DECAY_PER_UPDATE) : 0;
    }

    // Noise floor: a minimum-following envelope. Drops immediately to
    // match a quieter average, but is only allowed to creep upward
    // slowly -- a simple floor estimate, not full calibration/AGC.
    if (m_noiseFloor == 0 || m_average < m_noiseFloor)
    {
        m_noiseFloor = m_average;
    }
    else
    {
        m_noiseFloor = static_cast<int32_t>((1.0f - NOISE_FLOOR_RISE_ALPHA) * static_cast<float>(m_noiseFloor)
                                             + NOISE_FLOOR_RISE_ALPHA * static_cast<float>(m_average));
    }

    const int32_t above = m_average - m_noiseFloor;
    int32_t normalized = 0;
    if (above > 0)
    {
        normalized = (above * 255) / ASSUMED_MAX_ABOVE_FLOOR;
    }
    m_level = static_cast<uint8_t>(normalized > 255 ? 255 : (normalized < 0 ? 0 : normalized));
}

bool AudioInput::isAvailable() const
{
    return m_i2sStarted;
}

int32_t AudioInput::raw() const
{
    return m_raw;
}

uint8_t AudioInput::level() const
{
    return m_level;
}

int32_t AudioInput::peak() const
{
    return m_peak;
}

int32_t AudioInput::average() const
{
    return m_average;
}

int32_t AudioInput::noiseFloor() const
{
    return m_noiseFloor;
}

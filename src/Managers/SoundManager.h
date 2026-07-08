/**
 * SoundManager.h
 * -----------------------------------------------------------------------
 * PLACEHOLDER -- not the real SoundManager milestone.
 *
 * AnimationContext (AnimationBase.h) reserves a SoundManager& slot so
 * effects and AnimationManager won't need another interface change once
 * real audio processing exists. This stub exists purely to give that
 * reference a concrete, constructible type for this hardware test.
 *
 * None of the real responsibilities from docs/ARCHITECTURE_CONTRACT.md
 * are implemented yet: no I2S sampling, no calibration, no AGC, no
 * envelope/beat detection. Every accessor returns the neutral value the
 * spec already requires while AudioMode::Disabled (Section 10), so
 * nothing downstream needs to branch on "is real audio processing
 * implemented yet."
 *
 * Replace this file's contents (not its public interface, ideally) when
 * SoundManager becomes its own dedicated milestone.
 * -----------------------------------------------------------------------
 */

#pragma once

class SoundManager
{
public:
    void begin() {}
    void update() {}

    float getVolume() const { return 0.0f; }
    float getBass() const { return 0.0f; }
    bool isBeat() const { return false; }
};

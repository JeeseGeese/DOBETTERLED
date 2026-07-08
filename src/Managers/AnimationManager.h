/**
 * AnimationManager.h
 * -----------------------------------------------------------------------
 * Owns current effect selection, next/previous/select-by-ID navigation,
 * and per-frame invocation of whichever effect is active -- including
 * the audio-mode override where AudioMode::Dedicated shows the
 * SoundReactive effect without touching the "regular" selection.
 *
 * Strict boundaries (see docs/ARCHITECTURE_CONTRACT.md):
 *   - Never reads ButtonManager or Hal -- SystemManager tells this
 *     class "select next" etc.; it never asks a button what happened.
 *   - Never calls SettingsManager. It holds the current selection only
 *     in RAM; SystemManager reads getCurrentAnimationId() and is
 *     responsible for persisting it.
 *   - No FastLED or GPIO knowledge whatsoever -- it only ever sees
 *     LEDDriver and PaletteManager through opaque references.
 *   - Does not define palette data (that's PaletteManager's) or effect
 *     rendering logic (that's each Animation subclass's).
 *
 * Frame cadence: update() renders whatever is currently active every
 * time it's called. It does not throttle itself to a target frame
 * rate -- SystemManager is responsible for calling update() at
 * Timing::TargetFrameIntervalMs cadence, since frame-rate gating is an
 * orchestration concern, not an effect-selection concern.
 * -----------------------------------------------------------------------
 */

#pragma once

#include "Types.h" // AnimationId, EffectSettings, AudioMode
#include <stdint.h>

class LEDDriver;
class PaletteManager;
class SoundManager;
class EffectRegistry;
class Animation;

class AnimationManager
{
public:
    // Wires in the collaborators this manager needs each frame. None of
    // these are owned by AnimationManager -- they're constructed and
    // owned elsewhere (SystemManager / main.ino) and must outlive it.
    // `initialAnimationId` is whatever SystemManager already read from
    // SettingsManager at boot.
    void begin(EffectRegistry& registry,
               PaletteManager& paletteManager,
               SoundManager& soundManager,
               AnimationId initialAnimationId);

    // Advances to the next effect ID, wrapping from the last back to
    // the first. Does not persist anything -- SystemManager reads
    // getCurrentAnimationId() afterward and writes it to SettingsManager.
    void selectNext();

    // Moves to the previous effect ID, wrapping.
    void selectPrevious();

    // Selects a specific effect ID directly (e.g. restoring a persisted
    // selection at boot, or a future direct-select input method).
    // No-ops silently if the ID is out of range.
    void selectById(AnimationId id);

    // The effect currently selected for "regular" display. Note this is
    // NOT necessarily what's rendering right now -- see update()'s
    // audioMode parameter and AudioMode::Dedicated.
    AnimationId getCurrentAnimationId() const;

    // Renders exactly one frame of whichever effect is active.
    //
    // `settings` and `audioMode` are supplied by the caller each call
    // (sourced from SettingsManager) rather than stored here, so this
    // class never needs a SettingsManager dependency of its own.
    //
    // audioMode decides which effect actually renders THIS frame:
    //   - Disabled / Overlay -> the currently selected "regular" effect.
    //     (Overlay's sound modulation is each effect's own concern via
    //     context.soundManager -- AnimationManager doesn't special-case it.)
    //   - Dedicated -> the SoundReactive effect, without reading or
    //     altering getCurrentAnimationId().
    //
    // If nothing is registered yet for the effect that should render
    // (expected during incremental effect rollout), this silently
    // renders nothing rather than treating it as an error.
    void update(LEDDriver& ledDriver,
                const EffectSettings& settings,
                AudioMode audioMode,
                uint32_t deltaMs);

private:
    EffectRegistry* m_registry = nullptr;
    PaletteManager* m_paletteManager = nullptr;
    SoundManager* m_soundManager = nullptr;

    AnimationId m_currentAnimationId = AnimationId::Solid;

    // Tracks whichever Animation* actually received the last begin()/
    // update() call -- may be the "regular" selection or the Dedicated
    // SoundReactive override. Used to detect when a switch happened so
    // begin() fires exactly once per activation, not every frame.
    Animation* m_activeEffect = nullptr;
};

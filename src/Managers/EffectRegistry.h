/**
 * EffectRegistry.h
 * -----------------------------------------------------------------------
 * A pure, stateless lookup table mapping AnimationId -> Animation*.
 *
 * EffectRegistry does not own effect instances (no allocation, no
 * deletion) and does not track "current" selection -- that's
 * AnimationManager's job, built on top of this. EffectRegistry only
 * ever answers "what effect is registered under this ID."
 *
 * This split exists so adding a new effect later means constructing it
 * once (wherever effect instances are owned -- likely SystemManager or
 * a static array in the .ino) and registering it here, without
 * AnimationManager needing to know how effects are constructed or
 * where they live in memory.
 * -----------------------------------------------------------------------
 */

#pragma once

#include "Types.h" // AnimationId

class Animation;

class EffectRegistry
{
public:
    // Registers an effect instance under the given ID. Overwrites any
    // previous registration for that ID. EffectRegistry never owns the
    // pointer -- the caller is responsible for the effect's lifetime.
    void registerEffect(AnimationId id, Animation* effect);

    // Returns the effect registered under `id`, or nullptr if nothing
    // has been registered for that ID yet.
    Animation* getEffect(AnimationId id) const;

    // Whether an effect has been registered under `id`.
    bool hasEffect(AnimationId id) const;

private:
    Animation* m_effects[static_cast<uint8_t>(AnimationId::Count)] = {};

    // Returns -1 (via a bool out-param) for an out-of-range ID rather
    // than trusting every caller to have validated it first.
    bool indexFor(AnimationId id, uint8_t& outIndex) const;
};

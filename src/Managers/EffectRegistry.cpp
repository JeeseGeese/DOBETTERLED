/**
 * EffectRegistry.cpp
 * -----------------------------------------------------------------------
 * See EffectRegistry.h for the contract. Deliberately tiny -- this is a
 * lookup table, not a manager; all behavior/selection logic belongs to
 * AnimationManager, built on top of this.
 * -----------------------------------------------------------------------
 */

#include "EffectRegistry.h"

bool EffectRegistry::indexFor(AnimationId id, uint8_t& outIndex) const
{
    const uint8_t index = static_cast<uint8_t>(id);
    if (index >= static_cast<uint8_t>(AnimationId::Count))
    {
        return false;
    }
    outIndex = index;
    return true;
}

void EffectRegistry::registerEffect(AnimationId id, Animation* effect)
{
    uint8_t index = 0;
    if (!indexFor(id, index))
    {
        return; // invalid ID -- silently ignored, nothing to register into
    }
    m_effects[index] = effect;
}

Animation* EffectRegistry::getEffect(AnimationId id) const
{
    uint8_t index = 0;
    if (!indexFor(id, index))
    {
        return nullptr;
    }
    return m_effects[index];
}

bool EffectRegistry::hasEffect(AnimationId id) const
{
    return getEffect(id) != nullptr;
}

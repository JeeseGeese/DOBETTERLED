/**
 * AnimationManager.cpp
 * -----------------------------------------------------------------------
 * See AnimationManager.h for the contract. This file includes
 * AnimationBase.h and EffectRegistry.h for their full definitions (it
 * needs to construct an AnimationContext and call through an Animation*)
 * -- the header only forward-declares them to keep AnimationManager.h
 * itself minimal for anything that just needs to call selectNext() etc.
 * -----------------------------------------------------------------------
 */

#include "AnimationManager.h"
#include "../AnimationBase.h"   // Animation, AnimationContext
#include "EffectRegistry.h"  // EffectRegistry::getEffect()

void AnimationManager::begin(EffectRegistry& registry,
                              PaletteManager& paletteManager,
                              SoundManager& soundManager,
                              AnimationId initialAnimationId)
{
    m_registry = &registry;
    m_paletteManager = &paletteManager;
    m_soundManager = &soundManager;
    m_currentAnimationId = initialAnimationId;

    // Force the first update() call to treat whatever resolves as a
    // fresh activation, so begin() fires on it exactly once.
    m_activeEffect = nullptr;
}

void AnimationManager::selectNext()
{
    uint8_t next = static_cast<uint8_t>(m_currentAnimationId) + 1;
    if (next >= static_cast<uint8_t>(AnimationId::Count))
    {
        next = 0;
    }
    m_currentAnimationId = static_cast<AnimationId>(next);
}

void AnimationManager::selectPrevious()
{
    const uint8_t count = static_cast<uint8_t>(AnimationId::Count);
    const uint8_t current = static_cast<uint8_t>(m_currentAnimationId);
    const uint8_t previous = (current == 0) ? (count - 1) : (current - 1);
    m_currentAnimationId = static_cast<AnimationId>(previous);
}

void AnimationManager::selectById(AnimationId id)
{
    if (static_cast<uint8_t>(id) < static_cast<uint8_t>(AnimationId::Count))
    {
        m_currentAnimationId = id;
    }
}

AnimationId AnimationManager::getCurrentAnimationId() const
{
    return m_currentAnimationId;
}

void AnimationManager::update(LEDDriver& ledDriver,
                               const EffectSettings& settings,
                               AudioMode audioMode,
                               uint32_t deltaMs)
{
    if (m_registry == nullptr || m_paletteManager == nullptr || m_soundManager == nullptr)
    {
        return; // begin() hasn't been called yet -- nothing to render with
    }

    // Dedicated audio mode overrides which effect renders THIS frame,
    // without ever reading or modifying m_currentAnimationId -- so
    // switching back to Disabled/Overlay restores exactly what was
    // showing before, per spec Section 10.
    const AnimationId desiredId =
        (audioMode == AudioMode::Dedicated) ? AnimationId::SoundReactive : m_currentAnimationId;

    Animation* desiredEffect = m_registry->getEffect(desiredId);

    AnimationContext context{
        ledDriver,
        *m_paletteManager,
        *m_soundManager,
        settings,
        deltaMs
    };

    if (desiredEffect != m_activeEffect)
    {
        m_activeEffect = desiredEffect;
        if (m_activeEffect != nullptr)
        {
            m_activeEffect->begin(context);
        }
    }

    if (m_activeEffect != nullptr)
    {
        m_activeEffect->update(context);
    }
    // else: nothing registered yet for the desired ID -- silently
    // render nothing this frame. Expected during incremental effect
    // rollout (Milestone 4), not an error condition.
}

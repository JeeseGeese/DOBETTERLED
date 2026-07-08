/**
 * SolidEffect.h
 * -----------------------------------------------------------------------
 * The simplest possible effect: fills every pixel with
 * EffectSettings::primaryColor. Ignores palette, speed, and intensity
 * entirely -- per spec Section 6, Solid is explicitly a primary-color-
 * only effect.
 *
 * Exists specifically to let the framework be wired end-to-end
 * (SystemManager -> AnimationManager -> EffectRegistry -> this ->
 * LEDDriver -> physical strip) and verified on real hardware before any
 * of the more complex effects are built.
 * -----------------------------------------------------------------------
 */

#pragma once

#include "../AnimationBase.h"

class SolidEffect : public Animation
{
public:
    void update(const AnimationContext& context) override;
};

/**
 * SolidEffect.cpp
 * -----------------------------------------------------------------------
 * See SolidEffect.h. Deliberately trivial -- no palette lookup, no
 * per-pixel variation, no time-based motion. This is the "does the
 * whole pipeline actually work" effect, not a feature showcase.
 * -----------------------------------------------------------------------
 */

#include "SolidEffect.h"
#include "../Managers/LEDDriver.h"

void SolidEffect::update(const AnimationContext& context)
{
    // Uses fill() rather than a manual per-pixel loop -- LEDDriver
    // already exposes exactly this operation, no need to duplicate it
    // here or reach into anything lower-level.
    context.ledDriver.fill(context.settings.primaryColor);
}

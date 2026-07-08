/**
 * main.cpp
 * -----------------------------------------------------------------------
 * DOBETTERLED Engineering Console entry point (v0.2.0-alpha).
 *
 * MILESTONE 2: adds ButtonGestureEngine (see EngineeringConsole.h) for
 * gesture detection and Serial-only reporting. No gesture is wired to
 * a real action yet -- see docs/PRODUCT_SPEC.md's Milestone 2 scope
 * note and docs/DESIGN_LOG.md Section 12 [BRoadmap v1.2].
 *
 * This is a candidate build awaiting hardware re-verification.
 *
 * ROLLBACK: if this build fails acceptance, change the two lines below
 * to instantiate and call BringUpDashboard instead of EngineeringConsole.
 * BringUpDashboard.h/.cpp are unmodified and still compiled into this
 * build, so this is a one-line-per-call swap.
 * -----------------------------------------------------------------------
 */

#include <Arduino.h>
#include "EngineeringConsole.h"

EngineeringConsole console;

void setup()
{
    console.begin();
}

void loop()
{
    console.update();
}

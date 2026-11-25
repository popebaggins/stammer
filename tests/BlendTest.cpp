#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Blend.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("Balance crossfades Forward / Reverse / Octave-up Reverse, weighted by its -1..1 position", "[blend]")
{
    constexpr float forward = 2.0f;
    constexpr float reverse = 10.0f;
    constexpr float octaveUp = 50.0f;

    // Endpoints: each extreme is a single voice, pure and exact.
    CHECK_THAT((double) ReverseModeC::blendBalance(forward, reverse, octaveUp, -1.0f), WithinAbs(2.0, 1.0e-6));
    CHECK_THAT((double) ReverseModeC::blendBalance(forward, reverse, octaveUp, 1.0f), WithinAbs(50.0, 1.0e-6));

    // Centre: Reverse alone, matching the ticket-02 2-way convention this
    // extends (balance == 0 was pure Reverse there too).
    CHECK_THAT((double) ReverseModeC::blendBalance(forward, reverse, octaveUp, 0.0f), WithinAbs(10.0, 1.0e-6));

    // Midpoints of each half-range: linear interpolation's own definition
    // (independent of the implementation) puts the midpoint of a two-point
    // segment at the arithmetic mean of its ends.
    CHECK_THAT((double) ReverseModeC::blendBalance(forward, reverse, octaveUp, -0.5f), WithinAbs(6.0, 1.0e-6));
    CHECK_THAT((double) ReverseModeC::blendBalance(forward, reverse, octaveUp, 0.5f), WithinAbs(30.0, 1.0e-6));
}

TEST_CASE("Mix crossfades between dry input and the blended wet signal, weighted by its 0..1 position", "[blend]")
{
    constexpr float dry = 3.0f;
    constexpr float wet = 9.0f;

    CHECK_THAT((double) ReverseModeC::blendMix(dry, wet, 0.0f), WithinAbs(3.0, 1.0e-6));
    CHECK_THAT((double) ReverseModeC::blendMix(dry, wet, 1.0f), WithinAbs(9.0, 1.0e-6));
    CHECK_THAT((double) ReverseModeC::blendMix(dry, wet, 0.25f), WithinAbs(4.5, 1.0e-6));
}

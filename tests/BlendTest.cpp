#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Blend.h"

#include <cmath>

using Catch::Matchers::WithinAbs;

namespace
{
constexpr float halfPi = 1.5707963267948966f;
}

TEST_CASE("Balance crossfades Forward / Reverse / Octave-up Reverse, weighted by its -1..1 position", "[blend]")
{
    constexpr float forward = 2.0f;
    constexpr float reverse = 10.0f;
    constexpr float octaveUp = 50.0f;

    // Endpoints: each extreme is a single voice, pure and exact.
    CHECK_THAT((double) ReverseModeC::blendBalance(forward, reverse, octaveUp, -1.0f), WithinAbs(2.0, 1.0e-6));
    CHECK_THAT((double) ReverseModeC::blendBalance(forward, reverse, octaveUp, 1.0f), WithinAbs(50.0, 1.0e-6));

    // Centre: Reverse alone, matching the ticket-02 2-way convention this
    // extends (balance == 0 was pure Reverse there too), and continuous
    // from both sides of the curve's branch point.
    CHECK_THAT((double) ReverseModeC::blendBalance(forward, reverse, octaveUp, 0.0f), WithinAbs(10.0, 1.0e-6));

    // Midpoints of each half-range: an equal-power (sin/cos) crossfade,
    // not a linear one -- ticket 06 replaces ticket 03's linear curve
    // specifically because it dips in level at this point. The expected
    // values are the curve's own definition (sin/cos of pi/4), independent
    // of the blendBalance implementation under test.
    const auto midGain = std::sin(0.5f * halfPi); // sin(pi/4) == cos(pi/4), equal-power's own midpoint
    const auto expectedForwardReverseMid = (double) forward * midGain + (double) reverse * midGain;
    const auto expectedReverseOctaveUpMid = (double) reverse * midGain + (double) octaveUp * midGain;
    CHECK_THAT((double) ReverseModeC::blendBalance(forward, reverse, octaveUp, -0.5f),
               WithinAbs(expectedForwardReverseMid, 1.0e-4));
    CHECK_THAT((double) ReverseModeC::blendBalance(forward, reverse, octaveUp, 0.5f),
               WithinAbs(expectedReverseOctaveUpMid, 1.0e-4));
}

TEST_CASE("Balance's per-voice gains are monotonic across each half of its range", "[blend]")
{
    // A "nonlinear" curve must still never double back on itself: each
    // voice should fade in/out monotonically as Balance sweeps across the
    // half-range it's active in, or a sweep would produce an audible
    // swell-then-dip (or worse, a perceived click) instead of a smooth
    // transition.
    auto forwardGain = [&](float balance) { return ReverseModeC::blendBalance(1.0f, 0.0f, 0.0f, balance); };
    auto reverseGain = [&](float balance) { return ReverseModeC::blendBalance(0.0f, 1.0f, 0.0f, balance); };
    auto octaveUpGain = [&](float balance) { return ReverseModeC::blendBalance(0.0f, 0.0f, 1.0f, balance); };

    constexpr int steps = 200;
    float previousForward = forwardGain(-1.0f);
    float previousReverseLeft = reverseGain(-1.0f);
    float previousReverseRight = reverseGain(0.0f);
    float previousOctaveUp = octaveUpGain(0.0f);

    for (int i = 1; i <= steps; ++i)
    {
        const auto tLeft = -1.0f + (float) i / (float) steps;       // -1 .. 0
        const auto tRight = (float) i / (float) steps;              //  0 .. 1

        const auto forwardNow = forwardGain(tLeft);
        CHECK(forwardNow <= previousForward + 1.0e-6f); // Forward only falls as balance -> 0
        previousForward = forwardNow;

        const auto reverseLeftNow = reverseGain(tLeft);
        CHECK(reverseLeftNow >= previousReverseLeft - 1.0e-6f); // Reverse only rises as balance -> 0
        previousReverseLeft = reverseLeftNow;

        const auto reverseRightNow = reverseGain(tRight);
        CHECK(reverseRightNow <= previousReverseRight + 1.0e-6f); // Reverse only falls as balance -> +1
        previousReverseRight = reverseRightNow;

        const auto octaveUpNow = octaveUpGain(tRight);
        CHECK(octaveUpNow >= previousOctaveUp - 1.0e-6f); // Octave-up only rises as balance -> +1
        previousOctaveUp = octaveUpNow;
    }
}

TEST_CASE("Balance produces no clicks (sample-to-sample jumps) on a fine continuous sweep", "[blend]")
{
    // Simulates a smoothed Balance parameter sweeping continuously across
    // its full range, including straight through the curve's branch point
    // at 0 -- the one place a two-piece curve could stitch together
    // discontinuously and produce an audible click.
    constexpr float forward = 2.0f;
    constexpr float reverse = 10.0f;
    constexpr float octaveUp = 50.0f;

    constexpr int steps = 2000;
    auto previous = ReverseModeC::blendBalance(forward, reverse, octaveUp, -1.0f);

    // The largest gap between consecutive sample values a genuinely
    // continuous curve sampled this finely (1/2000th of the full range per
    // step) could produce, given the voices' own magnitudes -- well below
    // what a click would look like (an isolated jump far larger than its
    // neighbours).
    constexpr double maxStep = 1.0;

    for (int i = 1; i <= steps; ++i)
    {
        const auto balance = -1.0f + 2.0f * (float) i / (float) steps;
        const auto current = ReverseModeC::blendBalance(forward, reverse, octaveUp, balance);

        INFO("balance " << balance);
        CHECK_THAT((double) current, WithinAbs((double) previous, maxStep));
        previous = current;
    }
}

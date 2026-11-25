#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ReverseBuffer.h"

#include <cmath>

using Catch::Matchers::WithinAbs;

namespace
{
constexpr double sampleRate = 44100.0;
constexpr double pi = 3.14159265358979323846;
}

// The dual-window granular reader uses a periodic raised-cosine window whose
// gain is exactly 1.0 at a grain's midpoint (age == grainLength / 2), with
// the other grain's complementary weight exactly 0.0 there. At that single
// instant the output is an unweighted readback of one buffered input sample,
// which lets this test assert an exact expected value without re-deriving
// the window math the implementation uses.
TEST_CASE("Reading back reproduces an earlier input sample, time-reversed, not the most recent one", "[reverseBuffer]")
{
    ReverseModeC::ReverseBuffer reverseBuffer;
    reverseBuffer.prepare(sampleRate);
    reverseBuffer.setGrainLengthSeconds(8.0 / sampleRate); // grainLength == 8 samples, hop == 4

    float outL = 0.0f, outR = 0.0f;

    // Drive the first grain cycle fully into real history (triggerPos >= grainLength)
    // so its full look-back window references only real, known samples rather
    // than pre-roll silence from before the stream started.
    for (int k = 0; k <= 12; ++k)
    {
        const auto value = (float) k;
        reverseBuffer.processSample(value, value, outL, outR);
    }

    // At k == 12 (the 13th call), grain A is in its second cycle (triggered
    // at samplesWritten == 9) and has just reached age 4 -- its unity-gain
    // midpoint. The expected readback is input sample 4, confirming both
    // correct reversal (we hear an earlier sample, not sample 12) and
    // correct indexing (specifically sample 4, not any other).
    CHECK_THAT((double) outL, WithinAbs(4.0, 1.0e-5));
    CHECK_THAT((double) outR, WithinAbs(4.0, 1.0e-5));
}

// The octave-up voice reuses the exact same grain/window timing as the 1x
// Reverse voice (ageA/ageB and the window weights are unaffected), but steps
// through recorded history twice as fast: at the same unity-gain midpoint
// (age == 4) where the 1x voice reads input sample 4 (triggerPos - 1 - age),
// the octave-up voice reads input sample 0 (triggerPos - 1 - age*2). Same
// instant, same grain, independently verifiable expected values -- confirms
// the rate multiplier is wired into the index math without re-deriving it.
TEST_CASE("Octave-up voice reads twice as far back in history as the 1x Reverse voice, at the same instant", "[reverseBuffer]")
{
    ReverseModeC::ReverseBuffer reverseBuffer;
    reverseBuffer.prepare(sampleRate);
    reverseBuffer.setGrainLengthSeconds(8.0 / sampleRate); // grainLength == 8 samples, hop == 4

    float outL = 0.0f, outR = 0.0f;
    float octaveUpL = 0.0f, octaveUpR = 0.0f;

    for (int k = 0; k <= 12; ++k)
    {
        const auto value = (float) k;
        reverseBuffer.processSample(value, value, outL, outR, &octaveUpL, &octaveUpR);
    }

    CHECK_THAT((double) outL, WithinAbs(4.0, 1.0e-5));
    CHECK_THAT((double) outR, WithinAbs(4.0, 1.0e-5));
    CHECK_THAT((double) octaveUpL, WithinAbs(0.0, 1.0e-5));
    CHECK_THAT((double) octaveUpR, WithinAbs(0.0, 1.0e-5));
}

TEST_CASE("Reading back after the record buffer has wrapped still resolves to the latest write at that slot", "[reverseBuffer]")
{
    // A tiny sample rate forces a tiny (16-sample) capacity, so writing 29
    // samples wraps the circular buffer's physical storage nearly twice
    // over. Physical slot 4 (index 20 % 16) is written first by input
    // sample 4, then overwritten by input sample 20 before this test reads
    // it back -- a naive implementation that mishandles the modulo would
    // either read the stale value (4), read out of bounds, or crash.
    constexpr double tinySampleRate = 4.0; // capacity = ceil(4.0 * 4.0) = 16 samples
    ReverseModeC::ReverseBuffer reverseBuffer;
    reverseBuffer.prepare(tinySampleRate);
    reverseBuffer.setGrainLengthSeconds(2.0); // grainLength == 8 samples, hop == 4

    float outL = 0.0f, outR = 0.0f;
    for (int k = 0; k <= 28; ++k)
    {
        const auto value = (float) k;
        reverseBuffer.processSample(value, value, outL, outR);
    }

    // k == 28 is grain A's third unity-gain midpoint (triggered at
    // samplesWritten == 25, age == 4 -> absolute write index 20, physical
    // slot 20 % 16 == 4), which by then holds sample 20's value, not
    // sample 4's.
    CHECK_THAT((double) outL, WithinAbs(20.0, 1.0e-5));
    CHECK_THAT((double) outR, WithinAbs(20.0, 1.0e-5));
}

TEST_CASE("Octave-up voice resolves to the latest write at a wrapped slot, not a stale one", "[reverseBuffer]")
{
    // Same tiny (16-sample) capacity as the 1x wraparound test above, but
    // the octave-up voice's 2x lookback lands on a *different* wrapped
    // physical slot than the 1x voice does at the same instant (slot 0
    // instead of slot 4), independently exercising the same stale-vs-fresh
    // risk for the rate-2 index math.
    constexpr double tinySampleRate = 4.0; // capacity = ceil(4.0 * 4.0) = 16 samples
    ReverseModeC::ReverseBuffer reverseBuffer;
    reverseBuffer.prepare(tinySampleRate);
    reverseBuffer.setGrainLengthSeconds(2.0); // grainLength == 8 samples, hop == 4

    float outL = 0.0f, outR = 0.0f;
    float octaveUpL = 0.0f, octaveUpR = 0.0f;
    for (int k = 0; k <= 28; ++k)
    {
        const auto value = (float) k;
        reverseBuffer.processSample(value, value, outL, outR, &octaveUpL, &octaveUpR);
    }

    // k == 28, same instant as the 1x test: grain A triggered at
    // samplesWritten == 25, age == 4. Octave-up's rawIndex is
    // triggerPos - 1 - age*2 == 25 - 1 - 8 == 16, physical slot 16 % 16 ==
    // 0, which by now holds sample 16's value (the slot was first written
    // by sample 0, then overwritten by sample 16), not sample 0's.
    CHECK_THAT((double) octaveUpL, WithinAbs(16.0, 1.0e-5));
    CHECK_THAT((double) octaveUpR, WithinAbs(16.0, 1.0e-5));
}

TEST_CASE("Grain splice boundaries produce no click: output curvature stays near the input signal's own", "[reverseBuffer]")
{
    // A smooth, continuous test signal (not a constant or a ramp -- both of
    // those stay linear under any grain blend regardless of crossfade
    // correctness, so they can't expose a splice click). A sine wave's own
    // sample-to-sample curvature (second difference) has a known, tiny
    // analytic bound: for x(n) = A*sin(2*pi*f*n/fs), the second difference
    // is bounded by roughly A*(2*pi*f/fs)^2. An uncrossfaded (or
    // badly-crossfaded) splice between two unrelated grain readbacks
    // produces a near-discontinuous jump whose second difference is orders
    // of magnitude larger than that bound, so a generous multiple of it
    // (20x) cleanly separates "smooth signal" from "audible click" without
    // being tuned to this specific implementation.
    constexpr double testSampleRate = 1000.0;
    // Chosen so the sine's period (1000/23 ~= 43.48 samples) does not divide
    // evenly into the grain length or hop below -- an earlier attempt at
    // 25Hz (exact period 40) happened to put grain splice boundaries ~200
    // samples (~5 periods) apart, which coincidentally nearly phase-aligned
    // the two sides of the splice and masked a real click. A non-aligned
    // frequency is needed for this test to actually exercise crossfade
    // correctness.
    constexpr double frequencyHz = 23.0;
    constexpr double amplitude = 1.0;
    constexpr double grainLengthSeconds = 0.1; // grainLength == 100 samples, hop == 50

    ReverseModeC::ReverseBuffer reverseBuffer;
    reverseBuffer.prepare(testSampleRate);
    reverseBuffer.setGrainLengthSeconds(grainLengthSeconds);

    constexpr int numGrainCycles = 20;
    constexpr int grainLengthSamples = 100;
    constexpr int totalSamples = numGrainCycles * grainLengthSamples;
    constexpr int warmupSamples = 2 * grainLengthSamples;

    const auto angularStep = 2.0 * pi * frequencyHz / testSampleRate;
    const auto naturalCurvatureBound = amplitude * angularStep * angularStep;
    const auto clickThreshold = 20.0 * naturalCurvatureBound;

    std::vector<float> output(totalSamples);
    for (int n = 0; n < totalSamples; ++n)
    {
        const auto in = (float) (amplitude * std::sin(angularStep * (double) n));
        float outR = 0.0f;
        reverseBuffer.processSample(in, in, output[(size_t) n], outR);
    }

    double maxSecondDifference = 0.0;
    for (int n = warmupSamples + 1; n < totalSamples - 1; ++n)
    {
        const auto secondDifference = std::abs((double) output[(size_t) (n + 1)] - 2.0 * (double) output[(size_t) n]
                                                + (double) output[(size_t) (n - 1)]);
        maxSecondDifference = std::max(maxSecondDifference, secondDifference);
    }

    INFO("natural curvature bound: " << naturalCurvatureBound << ", click threshold: " << clickThreshold);
    CHECK(maxSecondDifference < clickThreshold);
}

TEST_CASE("Octave-up grain splice boundaries produce no click, same grain continuity as the 1x voice", "[reverseBuffer]")
{
    // Same setup and reasoning as the 1x splice-click test above, but for
    // the octave-up voice. Because the octave-up voice steps through the
    // recorded signal at 2x the rate, each output sample advances through
    // the original sine by twice the phase an equivalent 1x readback would
    // -- so its own natural (click-free) curvature is that of a sine at
    // double the angular step, not the original frequency. The bound below
    // is scaled accordingly, and the same generous 20x multiple separates a
    // genuine splice click from the voice's own expected curvature.
    constexpr double testSampleRate = 1000.0;
    constexpr double frequencyHz = 23.0;
    constexpr double amplitude = 1.0;
    constexpr double grainLengthSeconds = 0.1; // grainLength == 100 samples, hop == 50

    ReverseModeC::ReverseBuffer reverseBuffer;
    reverseBuffer.prepare(testSampleRate);
    reverseBuffer.setGrainLengthSeconds(grainLengthSeconds);

    constexpr int numGrainCycles = 20;
    constexpr int grainLengthSamples = 100;
    constexpr int totalSamples = numGrainCycles * grainLengthSamples;
    constexpr int warmupSamples = 2 * grainLengthSamples;

    const auto angularStep = 2.0 * pi * frequencyHz / testSampleRate;
    const auto octaveUpAngularStep = 2.0 * angularStep;
    const auto naturalCurvatureBound = amplitude * octaveUpAngularStep * octaveUpAngularStep;
    const auto clickThreshold = 20.0 * naturalCurvatureBound;

    std::vector<float> octaveUpOutput(totalSamples);
    for (int n = 0; n < totalSamples; ++n)
    {
        const auto in = (float) (amplitude * std::sin(angularStep * (double) n));
        float outL = 0.0f, outR = 0.0f, octaveUpR = 0.0f;
        reverseBuffer.processSample(in, in, outL, outR, &octaveUpOutput[(size_t) n], &octaveUpR);
    }

    double maxSecondDifference = 0.0;
    for (int n = warmupSamples + 1; n < totalSamples - 1; ++n)
    {
        const auto secondDifference = std::abs((double) octaveUpOutput[(size_t) (n + 1)] - 2.0 * (double) octaveUpOutput[(size_t) n]
                                                + (double) octaveUpOutput[(size_t) (n - 1)]);
        maxSecondDifference = std::max(maxSecondDifference, secondDifference);
    }

    INFO("natural curvature bound: " << naturalCurvatureBound << ", click threshold: " << clickThreshold);
    CHECK(maxSecondDifference < clickThreshold);
}

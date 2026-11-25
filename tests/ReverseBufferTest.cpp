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

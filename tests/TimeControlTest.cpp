#include <catch2/catch_test_macros.hpp>

#include "Parameters.h"
#include "PluginProcessor.h"

#include <cmath>
#include <cstdlib>
#include <new>

namespace
{
constexpr double sampleRate = 44100.0;
constexpr int blockSize = 512;

juce::AudioBuffer<float> rampBuffer(int numChannels, int numSamples)
{
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample(ch, i, 0.1f + 0.0005f * (float) i);
    return buffer;
}

// Counts heap allocations made while tracking is enabled, so a test can
// bracket exactly the processBlock calls under test -- not the (legitimate,
// off-audio-thread) allocations prepareToPlay makes -- and assert the count
// doesn't move.
namespace AllocationTracking
{
thread_local bool enabled = false;
thread_local std::size_t count = 0;
}

struct ScopedAllocationTracking
{
    ScopedAllocationTracking() { AllocationTracking::count = 0; AllocationTracking::enabled = true; }
    ~ScopedAllocationTracking() { AllocationTracking::enabled = false; }
};
}

void* operator new(std::size_t size)
{
    if (AllocationTracking::enabled)
        ++AllocationTracking::count;
    if (auto* p = std::malloc(size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

TEST_CASE("Time continuously controls the active grain length read from the buffer, at both preset-like and "
          "in-between values",
          "[timeControl]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    // Time is smoothed (20ms ramp, ~882 samples at this sample rate), so
    // give it enough blocks to fully settle to its new target before
    // reading the resulting grain length back.
    auto setTimeAndSettle = [&](float seconds)
    {
        *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::time) = seconds;
        for (int i = 0; i < 4; ++i)
        {
            auto block = rampBuffer(2, blockSize);
            processor.processBlock(block, midi);
        }
    };

    setTimeAndSettle(1.0f);
    CHECK(processor.getActiveGrainLengthSamples() == 44100);

    setTimeAndSettle(4.0f);
    CHECK(processor.getActiveGrainLengthSamples() == 176400);

    // An in-between value that was never one of the old 1s/2s/4s presets --
    // confirms the mapping is genuinely continuous, not snapping to the
    // nearest former preset.
    setTimeAndSettle(2.5f);
    CHECK(processor.getActiveGrainLengthSamples() == 110250);
}

TEST_CASE("Sweeping Time continuously while audio plays produces no clicks: output curvature stays near the input "
          "signal's own",
          "[timeControl]")
{
    // Same reasoning as the ReverseBuffer splice-boundary click tests: a
    // sine wave's own sample-to-sample curvature (second difference) has a
    // known, tiny analytic bound, so a genuine discontinuity -- here, from
    // the grain length being updated in a coarse, block-wise jump instead
    // of smoothly per-sample -- stands out as a second difference far
    // larger than that bound.
    constexpr double testSampleRate = 1000.0;
    constexpr int testBlockSize = 512;
    constexpr double frequencyHz = 17.0;
    constexpr double amplitude = 1.0;

    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(testSampleRate, testBlockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 1.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance) = 0.0f; // pure Reverse voice
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::time) = 0.2f;

    long long sampleCounter = 0;
    auto sineBlock = [&]
    {
        juce::AudioBuffer<float> buffer(2, testBlockSize);
        for (int i = 0; i < testBlockSize; ++i)
        {
            const auto in = (float) (amplitude * std::sin(2.0 * M_PI * frequencyHz * (double) sampleCounter
                                                            / testSampleRate));
            buffer.setSample(0, i, in);
            buffer.setSample(1, i, in);
            ++sampleCounter;
        }
        return buffer;
    };

    // Let Mix/Balance settle and give the reverse buffer real sine history
    // before measuring, and before the Time sweep itself begins.
    for (int i = 0; i < 6; ++i)
    {
        auto warmup = sineBlock();
        processor.processBlock(warmup, midi);
    }

    // A slow, continuous sweep: the host nudges the Time target a little
    // further every block, which is exactly how automation drives a
    // parameter in a real host (a new target per block, smoothed in
    // between) -- not a single instantaneous jump.
    constexpr int numSweepBlocks = 40;
    constexpr float startSeconds = 0.2f;
    constexpr float endSeconds = 0.6f;

    std::vector<float> output;
    output.reserve((size_t) numSweepBlocks * testBlockSize);

    for (int b = 0; b < numSweepBlocks; ++b)
    {
        const auto t = (float) b / (float) (numSweepBlocks - 1);
        *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::time) = startSeconds + t * (endSeconds - startSeconds);

        auto block = sineBlock();
        processor.processBlock(block, midi);

        for (int i = 0; i < testBlockSize; ++i)
            output.push_back(block.getSample(0, i));
    }

    const auto angularStep = 2.0 * M_PI * frequencyHz / testSampleRate;
    const auto naturalCurvatureBound = amplitude * angularStep * angularStep;
    const auto clickThreshold = 20.0 * naturalCurvatureBound;

    double maxSecondDifference = 0.0;
    for (size_t n = 1; n + 1 < output.size(); ++n)
    {
        const auto secondDifference = std::abs((double) output[n + 1] - 2.0 * (double) output[n] + (double) output[n - 1]);
        maxSecondDifference = std::max(maxSecondDifference, secondDifference);
    }

    INFO("natural curvature bound: " << naturalCurvatureBound << ", click threshold: " << clickThreshold);
    CHECK(maxSecondDifference < clickThreshold);
}

TEST_CASE("Sweeping Time continuously mid-stream does not allocate on the audio thread", "[timeControl]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    auto warmup = rampBuffer(2, blockSize);
    processor.processBlock(warmup, midi);

    auto blockA = rampBuffer(2, blockSize);
    auto blockB = rampBuffer(2, blockSize);
    auto blockC = rampBuffer(2, blockSize);
    auto* timeParam = processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::time);

    std::size_t allocationsDuringProcessing = 0;
    {
        ScopedAllocationTracking tracking;

        *timeParam = 0.37f;
        processor.processBlock(blockA, midi);

        *timeParam = 3.91f;
        processor.processBlock(blockB, midi);

        *timeParam = 1.23f;
        processor.processBlock(blockC, midi);

        allocationsDuringProcessing = AllocationTracking::count;
    }
    CHECK(allocationsDuringProcessing == 0);
}

namespace
{
constexpr double voiceTestSampleRate = 1000.0;

// Captures one block of output at a given fixed Balance (selecting Forward,
// Reverse, or Octave-up in isolation, as in ProcessBlockTest.cpp) and Time.
// Identically-primed instances differing only in Time isolate exactly what
// changing Time does to each voice's output.
juce::AudioBuffer<float> capturedOutputAtBalanceAndTime(float balance, float timeSeconds)
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(voiceTestSampleRate, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::time) = timeSeconds;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 1.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance) = balance;

    for (int i = 0; i < 6; ++i)
    {
        auto warmup = rampBuffer(2, blockSize);
        processor.processBlock(warmup, midi);
    }

    auto buffer = rampBuffer(2, blockSize);
    processor.processBlock(buffer, midi);
    return buffer;
}

bool anySampleDiffers(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
            if (std::abs(a.getSample(ch, i) - b.getSample(ch, i)) > 1.0e-4f)
                return true;
    return false;
}
}

TEST_CASE("Reverse and Octave-up voices respond to Time; Forward does not, since it never reads the grain buffer",
          "[timeControl]")
{
    constexpr float shortTime = 0.3f;
    constexpr float longTime = 0.9f;

    // Reverse (Balance == 0): a different grain length changes which
    // earlier samples get read back, so the output must differ.
    CHECK(anySampleDiffers(capturedOutputAtBalanceAndTime(0.0f, shortTime),
                            capturedOutputAtBalanceAndTime(0.0f, longTime)));

    // Octave-up (Balance == +1): same requirement, independently, since it
    // reads through a separate code path (2x rate) that could ignore Time
    // even if Reverse correctly used it.
    CHECK(anySampleDiffers(capturedOutputAtBalanceAndTime(1.0f, shortTime),
                            capturedOutputAtBalanceAndTime(1.0f, longTime)));

    // Forward (Balance == -1) is the dry signal and never consults the
    // grain buffer at all, so it must be unaffected by Time.
    CHECK_FALSE(anySampleDiffers(capturedOutputAtBalanceAndTime(-1.0f, shortTime),
                                  capturedOutputAtBalanceAndTime(-1.0f, longTime)));
}

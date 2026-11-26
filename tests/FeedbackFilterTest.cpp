#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Parameters.h"
#include "PluginProcessor.h"

#include <cmath>

using Catch::Matchers::WithinAbs;

namespace
{
// A low sample rate so Time resolves to a short, fast-to-fill grain -- see
// the identical rationale in ProcessBlockTest.cpp.
constexpr double sampleRate = 1000.0;
constexpr int blockSize = 512;

juce::AudioBuffer<float> rampBuffer(int numChannels, int numSamples)
{
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample(ch, i, 0.1f + 0.0005f * (float) i);
    return buffer;
}

// Builds a processor primed with real buffer history, pure-Reverse Balance,
// fully wet Mix, and the given Feedback, then returns its output for one
// more block of ramp input. Mirrors capturedOutputAtBalance in
// ProcessBlockTest.cpp.
juce::AudioBuffer<float> capturedOutputAtFeedback(float feedback)
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::time) = 1.0f; // 1000-sample grain
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 1.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance) = 0.0f; // pure Reverse
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::feedback) = feedback;

    for (int i = 0; i < 6; ++i)
    {
        auto warmup = rampBuffer(2, blockSize);
        processor.processBlock(warmup, midi);
    }

    auto buffer = rampBuffer(2, blockSize);
    processor.processBlock(buffer, midi);
    return buffer;
}
}

TEST_CASE("Feedback recirculates the reverse voice's own output back into the buffer, changing the result",
          "[feedback]")
{
    const auto noFeedback = capturedOutputAtFeedback(0.0f);
    const auto fullFeedback = capturedOutputAtFeedback(1.0f);

    bool anySampleDiffers = false;
    for (int ch = 0; ch < noFeedback.getNumChannels() && !anySampleDiffers; ++ch)
        for (int i = 0; i < noFeedback.getNumSamples(); ++i)
            if (std::abs(noFeedback.getSample(ch, i) - fullFeedback.getSample(ch, i)) > 1.0e-4f)
            {
                anySampleDiffers = true;
                break;
            }

    CHECK(anySampleDiffers);
}

TEST_CASE("Feedback at maximum with loud sustained input stays bounded instead of growing without limit",
          "[feedback]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::time) = 1.0f; // 1000-sample grain
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 1.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance) = 0.0f; // pure Reverse
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::feedback) = 1.0f; // maxed out

    juce::AudioBuffer<float> loudBlock(2, blockSize);
    loudBlock.clear();
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i)
            loudBlock.setSample(ch, i, 0.9f);

    // Many blocks: a runaway feedback loop would diverge (or produce
    // NaN/Inf) well before this many passes around the loop.
    float maxAbsSample = 0.0f;
    for (int block = 0; block < 100; ++block)
    {
        auto input = loudBlock;
        processor.processBlock(input, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
            {
                const auto sample = input.getSample(ch, i);
                REQUIRE(std::isfinite(sample));
                maxAbsSample = std::max(maxAbsSample, std::abs(sample));
            }
    }

    // tanh saturation bounds the recirculated signal to (-1, 1) regardless
    // of how many times it loops back through the buffer, so the Reverse
    // voice (which only ever reads back saturated history) can't exceed
    // roughly unity even when summed with the original 0.9 input.
    CHECK(maxAbsSample < 2.0f);
}

namespace
{
// RMS level of a block, used to compare how much a filter setting
// attenuates a given test tone relative to another setting.
double rmsLevel(const juce::AudioBuffer<float>& buffer, int channel)
{
    double sumSquares = 0.0;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto sample = (double) buffer.getSample(channel, i);
        sumSquares += sample * sample;
    }
    return std::sqrt(sumSquares / buffer.getNumSamples());
}

// A high-frequency tone (well above audible bass, near the top of a
// 44.1kHz-rate signal) that a lowpass morph should visibly attenuate.
juce::AudioBuffer<float> highFrequencyToneBuffer(int numSamples, double toneSampleRate)
{
    constexpr double pi = 3.14159265358979323846;
    juce::AudioBuffer<float> buffer(2, numSamples);
    constexpr double toneHz = 9000.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample(ch, i, (float) std::sin(2.0 * pi * toneHz * (double) i / toneSampleRate));
    return buffer;
}

// Runs a processor with Balance pinned to pure Forward (-1) and Mix fully
// wet (1), so the output is exactly the filtered dry signal -- isolating
// the Filter seam from the reverse-buffer/feedback machinery entirely.
juce::AudioBuffer<float> filteredOutput(float filterParam, const juce::AudioBuffer<float>& input)
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(44100.0, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 1.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance) = -1.0f; // pure Forward
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::filter) = filterParam;

    // Let Mix/Balance/Filter smoothers settle before measuring.
    for (int i = 0; i < 4; ++i)
    {
        auto warmup = input;
        processor.processBlock(warmup, midi);
    }

    auto buffer = input;
    processor.processBlock(buffer, midi);
    return buffer;
}
}

TEST_CASE("Filter moved fully right (lowpass) attenuates a high-frequency tone relative to the centre position",
          "[filter]")
{
    const auto tone = highFrequencyToneBuffer(blockSize, 44100.0);

    const auto centreOutput = filteredOutput(0.5f, tone);
    const auto lowpassOutput = filteredOutput(1.0f, tone);

    const auto centreLevel = rmsLevel(centreOutput, 0);
    const auto lowpassLevel = rmsLevel(lowpassOutput, 0);

    // Centre is defined as no filtering: the high-frequency tone passes
    // through essentially unattenuated.
    CHECK_THAT(centreLevel, WithinAbs(rmsLevel(tone, 0), 1.0e-3));

    // Fully right is a lowpass: the same high-frequency tone should come
    // out much quieter.
    CHECK(lowpassLevel < centreLevel * 0.5);
}

namespace
{
// A low-frequency tone that a highpass morph should visibly attenuate.
juce::AudioBuffer<float> lowFrequencyToneBuffer(int numSamples, double toneSampleRate)
{
    constexpr double pi = 3.14159265358979323846;
    juce::AudioBuffer<float> buffer(2, numSamples);
    constexpr double toneHz = 80.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample(ch, i, (float) std::sin(2.0 * pi * toneHz * (double) i / toneSampleRate));
    return buffer;
}
}

TEST_CASE("Filter moved fully left (highpass) attenuates a low-frequency tone relative to the centre position",
          "[filter]")
{
    const auto tone = lowFrequencyToneBuffer(blockSize, 44100.0);

    const auto centreOutput = filteredOutput(0.5f, tone);
    const auto highpassOutput = filteredOutput(0.0f, tone);

    const auto centreLevel = rmsLevel(centreOutput, 0);
    const auto highpassLevel = rmsLevel(highpassOutput, 0);

    // Centre is defined as no filtering: the low-frequency tone passes
    // through essentially unattenuated.
    CHECK_THAT(centreLevel, WithinAbs(rmsLevel(tone, 0), 1.0e-3));

    // Fully left is a highpass: the same low-frequency tone should come out
    // much quieter.
    CHECK(highpassLevel < centreLevel * 0.5);
}

TEST_CASE("Feedback's target change converges gradually, not in a single-sample jump", "[feedback]")
{
    // Mirrors the equivalent Mix case in SmoothingTest.cpp, applied to the
    // Feedback parameter this ticket wires into the DSP for the first time.
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(44100.0, blockSize);
    juce::MidiBuffer midi;

    REQUIRE_THAT((double) processor.getSmoothedParameterValue(ReverseModeC::ParamIDs::feedback),
                 WithinAbs(0.3, 1.0e-6));

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::feedback) = 1.0f;

    auto block = rampBuffer(2, blockSize);
    processor.processBlock(block, midi);
    const auto afterOneBlock = processor.getSmoothedParameterValue(ReverseModeC::ParamIDs::feedback);

    // One 512-sample block (~11.6ms) is shorter than the 20ms smoothing
    // ramp, so the value must have moved toward the target but not reached
    // it -- a gradual ramp, not an instant jump.
    CHECK(afterOneBlock > 0.3);
    CHECK(afterOneBlock < 1.0);

    for (int i = 0; i < 4; ++i)
    {
        block = rampBuffer(2, blockSize);
        processor.processBlock(block, midi);
    }
    CHECK_THAT((double) processor.getSmoothedParameterValue(ReverseModeC::ParamIDs::feedback),
               WithinAbs(1.0, 1.0e-3));
}

TEST_CASE("Filter's target change converges gradually, not in a single-sample jump", "[filter]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(44100.0, blockSize);
    juce::MidiBuffer midi;

    REQUIRE_THAT((double) processor.getSmoothedParameterValue(ReverseModeC::ParamIDs::filter),
                 WithinAbs(0.5, 1.0e-6));

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::filter) = 0.0f;

    auto block = rampBuffer(2, blockSize);
    processor.processBlock(block, midi);
    const auto afterOneBlock = processor.getSmoothedParameterValue(ReverseModeC::ParamIDs::filter);

    CHECK(afterOneBlock < 0.5);
    CHECK(afterOneBlock > 0.0);

    for (int i = 0; i < 4; ++i)
    {
        block = rampBuffer(2, blockSize);
        processor.processBlock(block, midi);
    }
    CHECK_THAT((double) processor.getSmoothedParameterValue(ReverseModeC::ParamIDs::filter),
               WithinAbs(0.0, 1.0e-3));
}

TEST_CASE("Filter morphs smoothly through the centre as it sweeps, with no abrupt jump in the output",
          "[filter]")
{
    // The lowpass and highpass branches use different StateVariableTPTFilter
    // types internally; this guards against a discontinuity where crossing
    // 0.5 switches branch abruptly instead of crossfading through zero
    // weight. Isolates Filter via pure-Forward Balance, as the other Filter
    // seam tests do.
    const auto tone = highFrequencyToneBuffer(blockSize, 44100.0);

    constexpr float step = 0.01f;
    float previousSample = 0.0f;
    bool first = true;

    for (float morph = 0.5f - step; morph <= 0.5f + step + 1.0e-6f; morph += step)
    {
        const auto output = filteredOutput(morph, tone);
        const auto sample = output.getSample(0, 0);

        if (!first)
            CHECK(std::abs(sample - previousSample) < 0.05f);

        previousSample = sample;
        first = false;
    }
}

namespace
{
juce::AudioBuffer<float> toneBufferAt(int numSamples, double toneSampleRate, double toneHz)
{
    constexpr double pi = 3.14159265358979323846;
    juce::AudioBuffer<float> buffer(2, numSamples);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample(ch, i, (float) std::sin(2.0 * pi * toneHz * (double) i / toneSampleRate));
    return buffer;
}

// Settings for all 5 of this ticket's wired controls (Mod Depth/Mod Rate
// stay unwired until ticket 05). Defaults to the processor's own APVTS
// defaults, so a caller only needs to name what it's changing.
struct ControlSettings
{
    float mix = 0.5f;
    float time = 1.0f; // 1000-sample grain at integrationSampleRate
    float balance = 0.0f; // pure Reverse
    float feedback = 0.3f;
    float filter = 0.5f; // centre: no filtering
};

constexpr double integrationSampleRate = 1000.0;
constexpr double integrationToneHz = 400.0; // above the clamped lowpass cutoff at this sample rate

juce::AudioBuffer<float> capturedOutputWithControls(const ControlSettings& settings)
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(integrationSampleRate, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = settings.mix;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::time) = settings.time;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance) = settings.balance;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::feedback) = settings.feedback;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::filter) = settings.filter;

    for (int i = 0; i < 6; ++i)
    {
        auto warmup = toneBufferAt(blockSize, integrationSampleRate, integrationToneHz);
        processor.processBlock(warmup, midi);
    }

    auto buffer = toneBufferAt(blockSize, integrationSampleRate, integrationToneHz);
    processor.processBlock(buffer, midi);
    return buffer;
}

bool buffersDiffer(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
            if (std::abs(a.getSample(ch, i) - b.getSample(ch, i)) > 1.0e-4f)
                return true;
    return false;
}
}

TEST_CASE("Each of Mix, Time, Feedback, Balance and Filter audibly changes the output from a shared baseline",
          "[integration]")
{
    const ControlSettings baseline;
    const auto baselineOutput = capturedOutputWithControls(baseline);

    auto mixChanged = baseline;
    mixChanged.mix = 0.0f;
    CHECK(buffersDiffer(baselineOutput, capturedOutputWithControls(mixChanged)));

    auto timeChanged = baseline;
    timeChanged.time = 2.0f;
    CHECK(buffersDiffer(baselineOutput, capturedOutputWithControls(timeChanged)));

    auto balanceChanged = baseline;
    balanceChanged.balance = -1.0f;
    CHECK(buffersDiffer(baselineOutput, capturedOutputWithControls(balanceChanged)));

    auto feedbackChanged = baseline;
    feedbackChanged.feedback = 1.0f;
    CHECK(buffersDiffer(baselineOutput, capturedOutputWithControls(feedbackChanged)));

    auto filterChanged = baseline;
    filterChanged.filter = 1.0f;
    CHECK(buffersDiffer(baselineOutput, capturedOutputWithControls(filterChanged)));
}

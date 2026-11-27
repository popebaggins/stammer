#include <catch2/catch_test_macros.hpp>

#include "Parameters.h"
#include "PluginProcessor.h"

#include <cmath>
#include <memory>

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

bool anySampleDiffers(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, float epsilon = 1.0e-4f)
{
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
            if (std::abs(a.getSample(ch, i) - b.getSample(ch, i)) > epsilon)
                return true;
    return false;
}
}

TEST_CASE("Raising Mod Depth modulates the output via a post-mix delay, even when Mix is fully dry", "[vibrato]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    // Fully dry and pure Forward, so without the vibrato stage the output
    // would be an exact passthrough of the input (as covered by
    // ProcessBlockTest.cpp). Mod Depth/Rate are the only thing that should
    // make this test's output differ from the input.
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 0.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::modDepth) = 1.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::modRate) = 5.0f;

    // Let Mix, Mod Depth and Mod Rate settle before measuring.
    for (int i = 0; i < 4; ++i)
    {
        auto warmup = rampBuffer(2, blockSize);
        processor.processBlock(warmup, midi);
    }

    const auto input = rampBuffer(2, blockSize);
    auto buffer = input;
    processor.processBlock(buffer, midi);

    CHECK(anySampleDiffers(buffer, input));
}

namespace
{
// Runs a processor with Mix fully dry and the given Mod Depth/Rate, warming
// up the smoothers first, then returns its output for one more block of
// ramp input.
juce::AudioBuffer<float> capturedOutputAtModSettings(float modDepth, float modRate)
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 0.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::modDepth) = modDepth;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::modRate) = modRate;

    for (int i = 0; i < 4; ++i)
    {
        auto warmup = rampBuffer(2, blockSize);
        processor.processBlock(warmup, midi);
    }

    auto buffer = rampBuffer(2, blockSize);
    processor.processBlock(buffer, midi);
    return buffer;
}
}

TEST_CASE("Mod Rate changes the vibrato's effect independently of Mod Depth", "[vibrato]")
{
    // Same (maximal) depth, two very different rates: if Mod Rate were
    // ignored, both LFOs would trace the same phase at the same instants
    // (both start at phase 0 in prepareToPlay) and these would be
    // identical, not just both-non-dry.
    const auto slow = capturedOutputAtModSettings(1.0f, 0.2f);
    const auto fast = capturedOutputAtModSettings(1.0f, 9.0f);

    CHECK(anySampleDiffers(slow, fast));
}

TEST_CASE("Sweeping Mod Depth mid-stream ramps smoothly instead of jumping in a single sample", "[vibrato]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 0.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::modRate) = 5.0f;

    constexpr float constantLevel = 0.5f;
    auto constantBlock = [](float level)
    {
        juce::AudioBuffer<float> b(2, blockSize);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                b.setSample(ch, i, level);
        return b;
    };

    for (int i = 0; i < 4; ++i)
    {
        auto warmup = constantBlock(constantLevel);
        processor.processBlock(warmup, midi);
    }

    // Jump Mod Depth from its settled 0 straight to maximum, then check no
    // single sample-to-sample step in the output is larger than a smoothed
    // ramp (rather than an instant jump) could produce.
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::modDepth) = 1.0f;

    auto buffer = constantBlock(constantLevel);
    processor.processBlock(buffer, midi);

    float previous = buffer.getSample(0, 0);
    for (int i = 1; i < buffer.getNumSamples(); ++i)
    {
        const auto sample = buffer.getSample(0, i);
        INFO("sample " << i);
        CHECK(std::abs(sample - previous) < 0.05f);
        previous = sample;
    }
}

namespace
{
class FixedTempoPlayHead : public juce::AudioPlayHead
{
public:
    explicit FixedTempoPlayHead(double bpmIn) : bpm(bpmIn) {}

    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
    {
        juce::AudioPlayHead::PositionInfo info;
        info.setBpm(bpm);
        return info;
    }

private:
    double bpm;
};

// Runs a processor with Mix fully dry, maximal Mod Depth, and the given
// Mod Sync/Rate/host-tempo settings, warming up the smoothers first, then
// returns its output for one more block of ramp input. playHeadBpm < 0
// means no playhead is attached at all.
juce::AudioBuffer<float> capturedOutputAtSyncSettings(bool modSync, float modRate, double playHeadBpm)
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    std::unique_ptr<FixedTempoPlayHead> playHead;
    if (playHeadBpm >= 0.0)
    {
        playHead = std::make_unique<FixedTempoPlayHead>(playHeadBpm);
        processor.setPlayHead(playHead.get());
    }

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 0.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::modDepth) = 1.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::modRate) = modRate;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::modSync) = modSync ? 1.0f : 0.0f;

    for (int i = 0; i < 4; ++i)
    {
        auto warmup = rampBuffer(2, blockSize);
        processor.processBlock(warmup, midi);
    }

    auto buffer = rampBuffer(2, blockSize);
    processor.processBlock(buffer, midi);
    return buffer;
}
}

TEST_CASE("Mod Sync locks the LFO to a different rate than Free mode's direct Hz value", "[vibrato]")
{
    // Free mode treats Mod Rate as 2 Hz directly. Sync mode (no host
    // playhead, so it falls back to 120 BPM) treats it as 2 LFO cycles per
    // beat -- 4 Hz at 120 BPM -- a different effective rate.
    const auto freeOutput = capturedOutputAtSyncSettings(false, 2.0f, -1.0);
    const auto syncOutput = capturedOutputAtSyncSettings(true, 2.0f, -1.0);

    CHECK(anySampleDiffers(freeOutput, syncOutput));
}

TEST_CASE("Mod Sync reads the actual tempo from the host playhead, not just a static fallback", "[vibrato]")
{
    const auto slowTempo = capturedOutputAtSyncSettings(true, 2.0f, 60.0);
    const auto fastTempo = capturedOutputAtSyncSettings(true, 2.0f, 180.0);

    CHECK(anySampleDiffers(slowTempo, fastTempo));
}

TEST_CASE("Mod Sync with no host playhead falls back to a default tempo instead of crashing or going silent",
          "[vibrato]")
{
    const auto input = rampBuffer(2, blockSize);
    const auto output = capturedOutputAtSyncSettings(true, 2.0f, -1.0);

    for (int ch = 0; ch < output.getNumChannels(); ++ch)
        for (int i = 0; i < output.getNumSamples(); ++i)
            REQUIRE(std::isfinite(output.getSample(ch, i)));

    CHECK(anySampleDiffers(output, input));
}

namespace
{
// Settings for all 7 primary controls plus both toggles, so this test can
// vary exactly one away from a shared baseline at a time. Defaults to the
// processor's own APVTS defaults, except Mod Depth (raised off its 0
// default so Mod Rate and Mod Sync have something to modulate).
struct FullControlSettings
{
    float mix = 0.5f;
    float time = 1.0f; // 1000-sample grain at integrationSampleRate
    float feedback = 0.3f;
    float balance = 0.0f; // pure Reverse
    float filter = 0.5f; // centre: no filtering
    float modDepth = 0.5f;
    float modRate = 2.0f;
    bool modSync = false;
    bool bypass = false;
};

constexpr double integrationSampleRate = 1000.0;

juce::AudioBuffer<float> capturedOutputWithFullControls(const FullControlSettings& settings)
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(integrationSampleRate, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = settings.mix;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::time) = settings.time;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::feedback) = settings.feedback;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance) = settings.balance;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::filter) = settings.filter;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::modDepth) = settings.modDepth;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::modRate) = settings.modRate;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::modSync) = settings.modSync ? 1.0f : 0.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::bypass) = settings.bypass ? 1.0f : 0.0f;

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

TEST_CASE("All 7 primary controls and both toggles audibly change the output from a shared baseline",
          "[integration]")
{
    const FullControlSettings baseline;
    const auto baselineOutput = capturedOutputWithFullControls(baseline);

    auto mixChanged = baseline;
    mixChanged.mix = 0.0f;
    CHECK(anySampleDiffers(baselineOutput, capturedOutputWithFullControls(mixChanged)));

    auto timeChanged = baseline;
    timeChanged.time = 2.0f;
    CHECK(anySampleDiffers(baselineOutput, capturedOutputWithFullControls(timeChanged)));

    auto feedbackChanged = baseline;
    feedbackChanged.feedback = 1.0f;
    CHECK(anySampleDiffers(baselineOutput, capturedOutputWithFullControls(feedbackChanged)));

    auto balanceChanged = baseline;
    balanceChanged.balance = -1.0f;
    CHECK(anySampleDiffers(baselineOutput, capturedOutputWithFullControls(balanceChanged)));

    auto filterChanged = baseline;
    filterChanged.filter = 1.0f;
    CHECK(anySampleDiffers(baselineOutput, capturedOutputWithFullControls(filterChanged)));

    auto modDepthChanged = baseline;
    modDepthChanged.modDepth = 1.0f;
    CHECK(anySampleDiffers(baselineOutput, capturedOutputWithFullControls(modDepthChanged)));

    auto modRateChanged = baseline;
    modRateChanged.modRate = 9.0f;
    CHECK(anySampleDiffers(baselineOutput, capturedOutputWithFullControls(modRateChanged)));

    auto modSyncChanged = baseline;
    modSyncChanged.modSync = true;
    CHECK(anySampleDiffers(baselineOutput, capturedOutputWithFullControls(modSyncChanged)));

    auto bypassChanged = baseline;
    bypassChanged.bypass = true;
    CHECK(anySampleDiffers(baselineOutput, capturedOutputWithFullControls(bypassChanged)));
}

TEST_CASE("The post-mix vibrato stage reports no added processing latency", "[vibrato]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    // It's a real-time modulated delay, not a look-ahead one, so it should
    // never need to report PDC latency to the host -- comfortably under
    // the ticket's 10ms ceiling (which at 44.1kHz is ~441 samples).
    CHECK(processor.getLatencySamples() == 0);
}

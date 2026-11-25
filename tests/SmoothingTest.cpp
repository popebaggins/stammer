#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Parameters.h"
#include "PluginProcessor.h"

using Catch::Matchers::WithinAbs;

namespace
{
constexpr double sampleRate = 44100.0;
constexpr int blockSize = 512;

juce::AudioBuffer<float> silentBlock(int numSamples)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    buffer.clear();
    return buffer;
}
}

TEST_CASE("A smoothed parameter's target change converges gradually, not in a single-sample jump", "[smoothing]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    REQUIRE_THAT((double) processor.getSmoothedParameterValue(ReverseModeC::ParamIDs::mix), WithinAbs(0.5, 1.0e-6));

    // Jump the host-automated target far from its default.
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 1.0f;

    auto block = silentBlock(blockSize);
    processor.processBlock(block, midi);
    const auto afterOneBlock = processor.getSmoothedParameterValue(ReverseModeC::ParamIDs::mix);

    // One 512-sample block (~11.6ms) is shorter than the 20ms smoothing
    // ramp, so the value must have moved toward the target but not reached
    // it -- a gradual ramp, not an instant jump.
    CHECK(afterOneBlock > 0.5);
    CHECK(afterOneBlock < 1.0);

    // Enough further blocks for the 20ms ramp (~882 samples) to complete.
    for (int i = 0; i < 4; ++i)
    {
        block = silentBlock(blockSize);
        processor.processBlock(block, midi);
    }
    CHECK_THAT((double) processor.getSmoothedParameterValue(ReverseModeC::ParamIDs::mix), WithinAbs(1.0, 1.0e-3));
}

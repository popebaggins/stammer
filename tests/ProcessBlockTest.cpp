#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Parameters.h"
#include "PluginProcessor.h"

#include <cmath>

using Catch::Matchers::WithinAbs;

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
}

TEST_CASE("Audio passes through unchanged when Mix is fully dry, regardless of the reversed signal", "[processBlock]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 0.0f;

    // Let the Mix smoother settle fully to 0 (default is 0.5; the smoothing
    // ramp is 20ms / ~882 samples) before measuring.
    for (int i = 0; i < 4; ++i)
    {
        auto warmup = rampBuffer(2, blockSize);
        processor.processBlock(warmup, midi);
    }

    const auto input = rampBuffer(2, blockSize);
    auto buffer = input;
    processor.processBlock(buffer, midi);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            INFO("channel " << ch << " sample " << i);
            CHECK_THAT((double) buffer.getSample(ch, i), WithinAbs((double) input.getSample(ch, i), 1.0e-4));
        }
}

TEST_CASE("Engaging Bypass mutes the output with a smoothed fade, not a click", "[processBlock]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    // Pin Mix fully dry so this test isolates Bypass behaviour from the
    // Forward/Reverse blend (covered separately by the blend seam tests).
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 0.0f;

    constexpr float constantLevel = 0.5f;
    auto constantBlock = [](float level)
    {
        juce::AudioBuffer<float> b(2, blockSize);
        b.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                b.setSample(ch, i, level);
        return b;
    };

    // Let the Mix smoother settle to 0 before measuring.
    for (int i = 0; i < 4; ++i)
    {
        auto warmup = constantBlock(constantLevel);
        processor.processBlock(warmup, midi);
    }

    // Settle at the steady, non-bypassed state first.
    auto settle = constantBlock(constantLevel);
    processor.processBlock(settle, midi);
    const auto lastEngagedSample = settle.getSample(0, settle.getNumSamples() - 1);
    CHECK_THAT((double) lastEngagedSample, WithinAbs((double) constantLevel, 1.0e-6));

    // Engage Bypass, then feed the same constant input.
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::bypass) = 1.0f;
    auto bypassed = constantBlock(constantLevel);
    processor.processBlock(bypassed, midi);

    // No step discontinuity right at the bypass moment.
    CHECK_THAT((double) bypassed.getSample(0, 0), WithinAbs((double) lastEngagedSample, 0.05));

    // Fully muted once the 30ms ramp (~1323 samples) has had time to
    // complete -- three more blocks is comfortably past that.
    juce::AudioBuffer<float> stillBypassed;
    for (int block = 0; block < 3; ++block)
    {
        stillBypassed = constantBlock(constantLevel);
        processor.processBlock(stillBypassed, midi);
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        INFO("channel " << ch);
        CHECK_THAT((double) stillBypassed.getSample(ch, stillBypassed.getNumSamples() - 1), WithinAbs(0.0, 1.0e-3));
    }
}

TEST_CASE("Balance at full Forward reproduces the dry input exactly, even once the reverse buffer has real history",
          "[processBlock]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 1.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance) = -1.0f;

    // Let Mix and Balance smoothers settle, and prime the reverse buffer
    // with several blocks of real history so a bug that ignored Balance
    // (always blending in some Reverse) would be exposed.
    for (int i = 0; i < 6; ++i)
    {
        auto warmup = rampBuffer(2, blockSize);
        processor.processBlock(warmup, midi);
    }

    const auto input = rampBuffer(2, blockSize);
    auto buffer = input;
    processor.processBlock(buffer, midi);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            INFO("channel " << ch << " sample " << i);
            CHECK_THAT((double) buffer.getSample(ch, i), WithinAbs((double) input.getSample(ch, i), 1.0e-4));
        }
}

TEST_CASE("Balance at full Reverse produces audibly different, reversed audio instead of the dry input", "[processBlock]")
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 1.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance) = 1.0f;

    // Prime the reverse buffer with a non-constant (ramp) signal so a
    // correctly-wired Reverse voice has real, distinguishable history to
    // play back, then let Mix/Balance settle.
    for (int i = 0; i < 6; ++i)
    {
        auto warmup = rampBuffer(2, blockSize);
        processor.processBlock(warmup, midi);
    }

    const auto input = rampBuffer(2, blockSize);
    auto buffer = input;
    processor.processBlock(buffer, midi);

    bool anySampleDiffers = false;
    for (int ch = 0; ch < buffer.getNumChannels() && !anySampleDiffers; ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (std::abs(buffer.getSample(ch, i) - input.getSample(ch, i)) > 1.0e-4f)
            {
                anySampleDiffers = true;
                break;
            }

    CHECK(anySampleDiffers);
}

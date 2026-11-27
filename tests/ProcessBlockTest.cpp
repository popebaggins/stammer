#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Blend.h"
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

TEST_CASE("Balance at centre (pure Reverse) produces audibly different, reversed audio instead of the dry input",
          "[processBlock]")
{
    // Ticket 03 moved pure Reverse from Balance == +1 (the old 2-way
    // convention) to Balance == 0 (the centre of the new 3-way
    // Forward/Reverse/Octave-up crossfade).
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 1.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance) = 0.0f;

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

namespace
{
// A low sample rate so the Time value set explicitly below (1.0s)
// resolves to a short, fast-to-fill 1000-sample grain. At the default 44.1kHz
// a grain is tens of thousands of samples, far longer than a few blocks of
// warmup can fill with real signal -- both the Reverse voice (reads back up
// to one grain) and especially the Octave-up voice (reads back up to two
// grains, at 2x rate) would still be reading mostly pre-roll silence rather
// than real history, making them look identical for the wrong reason.
constexpr double octaveTestSampleRate = 1000.0;

// Runs an identically-primed processor to full Mix/Balance settling and
// returns its output for one more block of ramp input. Feeding every
// instance the exact same input sequence makes their internal Reverse and
// Octave-up buffer state bit-identical (that state depends only on the
// input samples written, never on Balance/Mix), so outputs captured this
// way at different fixed Balance values are valid ground truth for each
// voice in isolation -- not values hand-derived from the blend formula.
juce::AudioBuffer<float> capturedOutputAtBalance(float balance)
{
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(octaveTestSampleRate, blockSize);
    juce::MidiBuffer midi;

    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::time) = 1.0f; // 1000-sample grain
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix) = 1.0f;
    *processor.apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance) = balance;

    // 6 blocks (3072 samples) of real history: comfortably more than the
    // 2000-sample deepest lookback the Octave-up voice needs (2x the
    // 1000-sample grain), and comfortably inside the 4000-sample buffer
    // capacity at this sample rate, so no wraparound complicates the result.
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

TEST_CASE("Balance at full Octave-up produces audio distinct from the Reverse voice, not an identical copy of it",
          "[processBlock]")
{
    // Catches an identity bug where the "octave-up" voice is wired up but
    // accidentally reads at the same 1x rate as Reverse (e.g. the rate
    // multiplier never reaches the reader), which would make this voice
    // indistinguishable from plain Reverse instead of a separate, audibly
    // pitched-up one.
    const auto reverseOnly = capturedOutputAtBalance(0.0f);
    const auto octaveUpOnly = capturedOutputAtBalance(1.0f);

    bool anySampleDiffers = false;
    for (int ch = 0; ch < reverseOnly.getNumChannels() && !anySampleDiffers; ++ch)
        for (int i = 0; i < reverseOnly.getNumSamples(); ++i)
            if (std::abs(reverseOnly.getSample(ch, i) - octaveUpOnly.getSample(ch, i)) > 1.0e-4f)
            {
                anySampleDiffers = true;
                break;
            }

    CHECK(anySampleDiffers);
}

TEST_CASE("Sweeping Balance from -1 through 0 to +1 smoothly blends Forward, Reverse, and Octave-up in turn",
          "[processBlock]")
{
    const auto forwardOnly = capturedOutputAtBalance(-1.0f);
    const auto reverseOnly = capturedOutputAtBalance(0.0f);
    const auto octaveUpOnly = capturedOutputAtBalance(1.0f);
    const auto forwardReverseBlend = capturedOutputAtBalance(-0.5f);
    const auto reverseOctaveUpBlend = capturedOutputAtBalance(0.5f);

    const auto rampInput = rampBuffer(2, blockSize);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i)
        {
            INFO("channel " << ch << " sample " << i);

            // Balance == -1 is Forward alone: exactly the dry input.
            CHECK_THAT((double) forwardOnly.getSample(ch, i), WithinAbs((double) rampInput.getSample(ch, i), 1.0e-4));

            // Balance == -0.5 / +0.5 are checked against the production
            // blendBalance curve applied to the three per-voice references
            // measured above -- ground truth for voice wiring (are the
            // right three signals reaching the blend at all?), independent
            // of the curve's own shape (covered separately, by-ear tuning
            // included, in BlendTest.cpp).
            const auto expectedForwardReverse = ReverseModeC::blendBalance(forwardOnly.getSample(ch, i),
                                                                            reverseOnly.getSample(ch, i),
                                                                            octaveUpOnly.getSample(ch, i),
                                                                            -0.5f);
            CHECK_THAT((double) forwardReverseBlend.getSample(ch, i), WithinAbs((double) expectedForwardReverse, 1.0e-4));

            const auto expectedReverseOctaveUp = ReverseModeC::blendBalance(forwardOnly.getSample(ch, i),
                                                                             reverseOnly.getSample(ch, i),
                                                                             octaveUpOnly.getSample(ch, i),
                                                                             0.5f);
            CHECK_THAT((double) reverseOctaveUpBlend.getSample(ch, i), WithinAbs((double) expectedReverseOctaveUp, 1.0e-4));
        }
}

TEST_CASE("Balance curve tuning adds no processing latency", "[processBlock]")
{
    // The Balance curve is a pure per-sample function with no buffering of
    // its own, so retuning it must never change the plugin's reported
    // latency -- comfortably under the ticket's 10ms ceiling (which at
    // 44.1kHz is ~441 samples).
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    CHECK(processor.getLatencySamples() == 0);
}

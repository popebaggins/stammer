#pragma once

#include "Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>

class ReverseModeCAudioProcessor : public juce::AudioProcessor
{
public:
    ReverseModeCAudioProcessor();
    ~ReverseModeCAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "ReverseModeC", ReverseModeC::createParameterLayout() };

    // Current value of a smoothed knob parameter (one of
    // ReverseModeC::smoothedParamIDs), as of the last processBlock call.
    // Exposed publicly since future DSP, metering and these tests all need
    // to observe the smoothed value, not just the raw host target.
    float getSmoothedParameterValue(const juce::String& paramID) const;

private:
    std::map<juce::String, juce::SmoothedValue<float>> smoothedValues;
    std::map<juce::String, std::atomic<float>*> smoothedParamSources;

    // No DSP exists yet (ticket 01 is scaffolding only): the signal path is a
    // transparent pass-through. Bypass is still meaningful in this interim
    // state -- it mutes the output -- so it is implemented as a smoothed
    // gain (1 = normal, 0 = bypassed) rather than a hard switch, to avoid
    // clicks. When real DSP lands, this same gain will be applied after the
    // wet/dry signal instead of after a bare pass-through.
    juce::SmoothedValue<float> bypassGain;
    std::atomic<float>* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverseModeCAudioProcessor)
};

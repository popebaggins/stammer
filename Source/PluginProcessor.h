#pragma once

#include "Parameters.h"
#include "ReverseBuffer.h"

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

    // The reverse reader's current grain/window length, in samples, as of
    // the last processBlock call. Exposed publicly so tests can confirm the
    // Buffer Length preset (1s/2s/4s) reaches the DSP without reallocation.
    int getActiveGrainLengthSamples() const { return reverseBuffer.getGrainLengthSamples(); }

private:
    std::map<juce::String, juce::SmoothedValue<float>> smoothedValues;
    std::map<juce::String, std::atomic<float>*> smoothedParamSources;

    // smoothedParamIDs minus Mix and Balance (those two are consumed
    // per-sample in processBlock instead). Built once in the constructor so
    // the audio thread never constructs a juce::String from a C-string
    // literal to compare IDs -- that allocates on the heap.
    std::vector<juce::String> blockWiseSmoothedParamIDs;

    // Cached directly from smoothedValues in prepareToPlay, so processBlock
    // never looks a juce::String key up in that map by a C-string literal --
    // std::map::at(const char*) constructs (and heap-allocates) a temporary
    // juce::String to compare against on every call.
    juce::SmoothedValue<float>* mixSmoothed = nullptr;
    juce::SmoothedValue<float>* balanceSmoothed = nullptr;

    ReverseModeC::ReverseBuffer reverseBuffer;
    std::atomic<float>* bufferLengthParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* balanceParam = nullptr;

    // Bypass is a smoothed gain (1 = normal, 0 = bypassed) applied after the
    // blended wet/dry signal, rather than a hard switch, to avoid clicks.
    juce::SmoothedValue<float> bypassGain;
    std::atomic<float>* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverseModeCAudioProcessor)
};

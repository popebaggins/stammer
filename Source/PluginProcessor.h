#pragma once

#include "MorphingFilter.h"
#include "Parameters.h"
#include "ReverseBuffer.h"
#include "VibratoDelay.h"

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

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "ReverseModeC", ReverseModeC::createParameterLayout() };

    // Current value of a smoothed knob parameter (one of
    // ReverseModeC::smoothedParamIDs), as of the last processBlock call.
    // Exposed publicly since future DSP, metering and these tests all need
    // to observe the smoothed value, not just the raw host target.
    float getSmoothedParameterValue(const juce::String& paramID) const;

    // The reverse reader's current grain/window length, in samples, as of
    // the last processBlock call. Exposed publicly so tests can confirm the
    // Time control reaches the DSP without reallocation.
    int getActiveGrainLengthSamples() const { return reverseBuffer.getGrainLengthSamples(); }

private:
    std::map<juce::String, juce::SmoothedValue<float>> smoothedValues;
    std::map<juce::String, std::atomic<float>*> smoothedParamSources;

    // smoothedParamIDs minus Mix, Time, Balance, Feedback, Filter, Mod Depth
    // and Mod Rate (those seven are consumed per-sample in processBlock
    // instead). Built once in the constructor so the audio thread never
    // constructs a juce::String from a C-string literal to compare IDs --
    // that allocates on the heap.
    std::vector<juce::String> blockWiseSmoothedParamIDs;

    // Cached directly from smoothedValues in prepareToPlay, so processBlock
    // never looks a juce::String key up in that map by a C-string literal --
    // std::map::at(const char*) constructs (and heap-allocates) a temporary
    // juce::String to compare against on every call.
    juce::SmoothedValue<float>* mixSmoothed = nullptr;
    juce::SmoothedValue<float>* balanceSmoothed = nullptr;

    // Time is consumed per-sample too: the active grain length must track
    // it smoothly at audio rate, not jump once per block, or a continuous
    // sweep clicks at every block boundary.
    juce::SmoothedValue<float>* timeSmoothed = nullptr;

    // Feedback is consumed per-sample: it scales the saturated recirculation
    // added to the buffer write on every sample, so a block-rate jump would
    // step the regenerated signal once per block instead of ramping it.
    juce::SmoothedValue<float>* feedbackSmoothed = nullptr;

    // Filter morphs per-sample too, for the same reason: a block-rate jump
    // would step the cutoff once per block instead of sweeping it smoothly.
    juce::SmoothedValue<float>* filterSmoothed = nullptr;

    // Mod Depth and Mod Rate drive the vibrato LFO at audio rate (its
    // frequency and the delay excursion it modulates), so both are
    // consumed per-sample too, for the same reason as Time/Feedback/Filter.
    juce::SmoothedValue<float>* modDepthSmoothed = nullptr;
    juce::SmoothedValue<float>* modRateSmoothed = nullptr;

    ReverseModeC::ReverseBuffer reverseBuffer;
    ReverseModeC::MorphingFilter morphingFilter;
    ReverseModeC::VibratoDelay vibratoDelay;
    std::atomic<float>* timeParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* balanceParam = nullptr;
    std::atomic<float>* feedbackParam = nullptr;
    std::atomic<float>* filterParam = nullptr;
    std::atomic<float>* modDepthParam = nullptr;
    std::atomic<float>* modRateParam = nullptr;

    // Mod Sync/Free is a boolean toggle (true = tempo-locked to the host,
    // false = Free Hz), read directly rather than smoothed, matching how
    // Bypass is read elsewhere in this class.
    std::atomic<float>* modSyncParam = nullptr;

    // The reverse voice's own output from the previous sample, saturated
    // and fed back into this sample's buffer write. Deliberately the raw
    // Reverse voice, not the Balance/Mix-blended monitor signal: keeping the
    // feedback path independent of Balance and Mix preserves the existing
    // invariant (see ProcessBlockTest.cpp) that the reverse buffer's
    // recorded content depends only on the input written, not on how the
    // user is currently monitoring it.
    float feedbackHistoryLeft = 0.0f;
    float feedbackHistoryRight = 0.0f;

    // Bypass is a smoothed gain (1 = normal, 0 = bypassed) applied after the
    // blended wet/dry signal, rather than a hard switch, to avoid clicks.
    juce::SmoothedValue<float> bypassGain;
    std::atomic<float>* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverseModeCAudioProcessor)
};

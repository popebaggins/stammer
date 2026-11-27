#include "Blend.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

ReverseModeCAudioProcessor::ReverseModeCAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    bypassParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::bypass);
    timeParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::time);
    mixParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix);
    balanceParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance);
    feedbackParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::feedback);
    filterParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::filter);
    modDepthParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::modDepth);
    modRateParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::modRate);
    modSyncParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::modSync);

    for (const auto& id : ReverseModeC::smoothedParamIDs)
    {
        smoothedParamSources[id] = apvts.getRawParameterValue(id);
        if (id != ReverseModeC::ParamIDs::mix && id != ReverseModeC::ParamIDs::balance
            && id != ReverseModeC::ParamIDs::time && id != ReverseModeC::ParamIDs::feedback
            && id != ReverseModeC::ParamIDs::filter && id != ReverseModeC::ParamIDs::modDepth
            && id != ReverseModeC::ParamIDs::modRate)
            blockWiseSmoothedParamIDs.push_back(id);
    }
}

void ReverseModeCAudioProcessor::prepareToPlay(double sampleRate, int)
{
    bypassGain.reset(sampleRate, 0.03);
    bypassGain.setCurrentAndTargetValue(*bypassParam > 0.5f ? 0.0f : 1.0f);

    for (const auto& [id, source] : smoothedParamSources)
    {
        auto& smoothed = smoothedValues[id];
        smoothed.reset(sampleRate, 0.02);
        smoothed.setCurrentAndTargetValue(*source);
    }

    reverseBuffer.prepare(sampleRate);
    reverseBuffer.setGrainLengthSeconds(*timeParam);
    morphingFilter.prepare(sampleRate, 2);
    vibratoDelay.prepare(sampleRate, 2);

    mixSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::mix);
    balanceSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::balance);
    timeSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::time);
    feedbackSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::feedback);
    filterSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::filter);
    modDepthSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::modDepth);
    modRateSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::modRate);

    feedbackHistoryLeft = 0.0f;
    feedbackHistoryRight = 0.0f;
}

float ReverseModeCAudioProcessor::getSmoothedParameterValue(const juce::String& paramID) const
{
    return smoothedValues.at(paramID).getCurrentValue();
}

void ReverseModeCAudioProcessor::releaseResources()
{
}

bool ReverseModeCAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ReverseModeCAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    bypassGain.setTargetValue(*bypassParam > 0.5f ? 0.0f : 1.0f);

    const auto numSamples = buffer.getNumSamples();

    mixSmoothed->setTargetValue(*mixParam);
    balanceSmoothed->setTargetValue(*balanceParam);
    timeSmoothed->setTargetValue(*timeParam);
    feedbackSmoothed->setTargetValue(*feedbackParam);
    filterSmoothed->setTargetValue(*filterParam);
    modDepthSmoothed->setTargetValue(*modDepthParam);
    modRateSmoothed->setTargetValue(*modRateParam);

    // Host tempo, read once per block (it doesn't change at audio rate).
    // Falls back to 120 BPM when no host playhead is attached, or it
    // doesn't report a tempo, so Mod Sync always has something to lock to.
    double bpm = 120.0;
    if (auto* currentPlayHead = getPlayHead())
        if (auto position = currentPlayHead->getPosition())
            if (auto reportedBpm = position->getBpm())
                bpm = *reportedBpm;
    const auto beatsPerSecond = (float) (bpm / 60.0);
    const auto modSyncEngaged = *modSyncParam > 0.5f;

    // The remaining smoothed params have no per-sample DSP consuming them
    // yet, so advance each of those once per block rather than
    // sample-by-sample.
    for (const auto& id : blockWiseSmoothedParamIDs)
    {
        auto& smoothed = smoothedValues.at(id);
        smoothed.setTargetValue(*smoothedParamSources.at(id));
        smoothed.skip(numSamples - 1);
        smoothed.getNextValue();
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto forwardL = left[i];
        const auto forwardR = right[i];

        reverseBuffer.setGrainLengthSeconds(timeSmoothed->getNextValue());

        const auto feedback = feedbackSmoothed->getNextValue();
        const auto writeInputL = forwardL + std::tanh(feedback * feedbackHistoryLeft);
        const auto writeInputR = forwardR + std::tanh(feedback * feedbackHistoryRight);

        float reverseL, reverseR, octaveUpL, octaveUpR;
        reverseBuffer.processSample(writeInputL, writeInputR, reverseL, reverseR, &octaveUpL, &octaveUpR);

        feedbackHistoryLeft = reverseL;
        feedbackHistoryRight = reverseR;

        const auto balance = balanceSmoothed->getNextValue();
        const auto wetL = ReverseModeC::blendBalance(forwardL, reverseL, octaveUpL, balance);
        const auto wetR = ReverseModeC::blendBalance(forwardR, reverseR, octaveUpR, balance);

        const auto filterMorph = filterSmoothed->getNextValue();
        const auto filteredWetL = morphingFilter.processSample(0, wetL, filterMorph);
        const auto filteredWetR = morphingFilter.processSample(1, wetR, filterMorph);

        const auto mix = mixSmoothed->getNextValue();
        const auto mixedL = ReverseModeC::blendMix(forwardL, filteredWetL, mix);
        const auto mixedR = ReverseModeC::blendMix(forwardR, filteredWetR, mix);

        // Post-mix vibrato: a separate short delay line, modulated by a
        // sine LFO, applied after Balance and Mix have already blended the
        // voices. Mod Sync ties the LFO's speed to the host tempo (Mod
        // Rate becomes "LFO cycles per beat") instead of a free Hz value.
        const auto modDepth = modDepthSmoothed->getNextValue();
        const auto modRateValue = modRateSmoothed->getNextValue();
        const auto lfoRateHz = modSyncEngaged ? beatsPerSecond * modRateValue : modRateValue;
        const auto lfoValue = vibratoDelay.advance(lfoRateHz);
        const auto vibratoL = vibratoDelay.processSample(0, mixedL, lfoValue, modDepth);
        const auto vibratoR = vibratoDelay.processSample(1, mixedR, lfoValue, modDepth);

        const auto gain = bypassGain.getNextValue();

        left[i] = vibratoL * gain;
        right[i] = vibratoR * gain;
    }
}

juce::AudioProcessorEditor* ReverseModeCAudioProcessor::createEditor()
{
    return new ReverseModeCAudioProcessorEditor(*this);
}

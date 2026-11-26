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

    for (const auto& id : ReverseModeC::smoothedParamIDs)
    {
        smoothedParamSources[id] = apvts.getRawParameterValue(id);
        if (id != ReverseModeC::ParamIDs::mix && id != ReverseModeC::ParamIDs::balance
            && id != ReverseModeC::ParamIDs::time && id != ReverseModeC::ParamIDs::feedback
            && id != ReverseModeC::ParamIDs::filter)
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

    mixSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::mix);
    balanceSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::balance);
    timeSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::time);
    feedbackSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::feedback);
    filterSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::filter);

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
        const auto gain = bypassGain.getNextValue();

        left[i] = ReverseModeC::blendMix(forwardL, filteredWetL, mix) * gain;
        right[i] = ReverseModeC::blendMix(forwardR, filteredWetR, mix) * gain;
    }
}

juce::AudioProcessorEditor* ReverseModeCAudioProcessor::createEditor()
{
    return new ReverseModeCAudioProcessorEditor(*this);
}

#include "Blend.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
// Buffer Length preset index (0/1/2, matching ReverseModeC::bufferLengthChoices
// "1s"/"2s"/"4s") to the grain/window length in seconds.
constexpr float bufferLengthSecondsByIndex[] = { 1.0f, 2.0f, 4.0f };

float bufferLengthSecondsForRawValue(float rawChoiceValue)
{
    constexpr auto numChoices = (int) (sizeof(bufferLengthSecondsByIndex) / sizeof(bufferLengthSecondsByIndex[0]));
    const auto index = juce::jlimit(0, numChoices - 1, (int) std::lround(rawChoiceValue));
    return bufferLengthSecondsByIndex[index];
}
}

ReverseModeCAudioProcessor::ReverseModeCAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    bypassParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::bypass);
    bufferLengthParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::bufferLength);
    mixParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::mix);
    balanceParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::balance);

    for (const auto& id : ReverseModeC::smoothedParamIDs)
    {
        smoothedParamSources[id] = apvts.getRawParameterValue(id);
        if (id != ReverseModeC::ParamIDs::mix && id != ReverseModeC::ParamIDs::balance)
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
    reverseBuffer.setGrainLengthSeconds(bufferLengthSecondsForRawValue(*bufferLengthParam));

    mixSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::mix);
    balanceSmoothed = &smoothedValues.at(ReverseModeC::ParamIDs::balance);
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
    reverseBuffer.setGrainLengthSeconds(bufferLengthSecondsForRawValue(*bufferLengthParam));

    const auto numSamples = buffer.getNumSamples();

    mixSmoothed->setTargetValue(*mixParam);
    balanceSmoothed->setTargetValue(*balanceParam);

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

        float reverseL, reverseR;
        reverseBuffer.processSample(forwardL, forwardR, reverseL, reverseR);

        const auto balance = balanceSmoothed->getNextValue();
        const auto wetL = ReverseModeC::blendBalance(forwardL, reverseL, balance);
        const auto wetR = ReverseModeC::blendBalance(forwardR, reverseR, balance);

        const auto mix = mixSmoothed->getNextValue();
        const auto gain = bypassGain.getNextValue();

        left[i] = ReverseModeC::blendMix(forwardL, wetL, mix) * gain;
        right[i] = ReverseModeC::blendMix(forwardR, wetR, mix) * gain;
    }
}

juce::AudioProcessorEditor* ReverseModeCAudioProcessor::createEditor()
{
    return new ReverseModeCAudioProcessorEditor(*this);
}

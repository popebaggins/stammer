#include "PluginProcessor.h"
#include "PluginEditor.h"

ReverseModeCAudioProcessor::ReverseModeCAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    bypassParam = apvts.getRawParameterValue(ReverseModeC::ParamIDs::bypass);

    for (const auto& id : ReverseModeC::smoothedParamIDs)
        smoothedParamSources[id] = apvts.getRawParameterValue(id);
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

    // No per-sample DSP consumes these yet, so advance each smoother once
    // per block rather than sample-by-sample.
    for (auto& [id, smoothed] : smoothedValues)
    {
        smoothed.setTargetValue(*smoothedParamSources.at(id));
        smoothed.skip(numSamples - 1);
        smoothed.getNextValue();
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = bypassGain.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, i, buffer.getSample(ch, i) * gain);
    }
}

juce::AudioProcessorEditor* ReverseModeCAudioProcessor::createEditor()
{
    return new ReverseModeCAudioProcessorEditor(*this);
}

#include "VibratoDelay.h"

#include <cmath>

namespace ReverseModeC
{

void VibratoDelay::prepare(double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;

    const juce::dsp::ProcessSpec spec { sampleRate, 0, (juce::uint32) numChannels };

    lfo.prepare(spec);
    lfo.reset();

    // A few samples of headroom above the maximum excursion so the
    // fractional read position never has to clamp against the buffer edge.
    delayLine.setMaximumDelayInSamples((int) std::ceil(maxDepthMs * 0.001 * sampleRate) + 4);
    delayLine.prepare(spec);
    delayLine.reset();
}

float VibratoDelay::advance(float rateHz)
{
    lfo.setFrequency(rateHz);
    return lfo.processSample(0.0f);
}

float VibratoDelay::processSample(int channel, float input, float lfoValue, float depth)
{
    delayLine.pushSample(channel, input);

    // Sweeps between 0 and depth * maxDepthMs, rather than oscillating
    // around a fixed non-zero base delay, so that depth == 0 is an exact
    // passthrough (delay == 0) instead of a flat, inaudible-but-present
    // time shift.
    const auto delayRangeSamples = depth * maxDepthMs * 0.001f * (float) sampleRate;
    const auto delaySamples = delayRangeSamples * 0.5f * (1.0f + lfoValue);

    return delayLine.popSample(channel, delaySamples);
}

} // namespace ReverseModeC

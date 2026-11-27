#pragma once

#include <juce_dsp/juce_dsp.h>

namespace ReverseModeC
{

// A short delay line applied after all voice blending (Forward/Reverse/
// Octave-up mixed by Balance, then dry/wet mixed by Mix), modulated by a
// sine LFO to produce a vibrato/chorus sweep. The LFO phase is shared
// across channels: advance() must be called exactly once per sample (not
// once per channel), and its result passed into processSample() for every
// channel of that same sample.
class VibratoDelay
{
public:
    // Maximum delay excursion at full Mod Depth. Comfortably under the
    // ticket's 10ms latency ceiling, even though this stage reports no
    // processing latency to the host at all -- it's a real-time
    // modulation, not a look-ahead one.
    static constexpr float maxDepthMs = 8.0f;

    // Allocates the delay line for the given sample rate and channel
    // count. Call once, off the audio thread, before processing.
    void prepare(double sampleRate, int numChannels);

    // Advances the shared sine LFO by one sample at rateHz and returns its
    // current value in [-1, 1]. Call exactly once per sample.
    float advance(float rateHz);

    // Delays one channel's sample by an amount that sweeps between 0 and
    // depth * maxDepthMs according to lfoValue (the most recent advance()
    // result). depth == 0 always yields a delay of exactly 0 samples, so
    // this stage is a transparent passthrough until Mod Depth is raised.
    float processSample(int channel, float input, float lfoValue, float depth);

private:
    juce::dsp::Oscillator<float> lfo { [](float x) { return std::sin(x); } };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;
    double sampleRate = 44100.0;
};

} // namespace ReverseModeC

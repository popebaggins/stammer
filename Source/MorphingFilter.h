#pragma once

#include <juce_dsp/juce_dsp.h>

namespace ReverseModeC
{

// A single morphing control that blends between highpass and lowpass
// filtering through a dead zone of no filtering at all. morph in [0, 1]:
// 0.5 is an exact pass-through, moving left (< 0.5) morphs into an
// increasingly aggressive highpass, moving right (> 0.5) morphs into an
// increasingly aggressive lowpass.
class MorphingFilter
{
public:
    // Allocates per-channel filter state for the given sample rate and
    // channel count. Call once, off the audio thread, before processing.
    void prepare(double sampleRate, int numChannels);

    // Filters one sample on the given channel according to the morph
    // position. Call once per sample, per channel, in order.
    float processSample(int channel, float input, float morph);

private:
    juce::dsp::StateVariableTPTFilter<float> filter;
    double sampleRate = 44100.0;
};

} // namespace ReverseModeC

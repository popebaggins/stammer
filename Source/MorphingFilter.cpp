#include "MorphingFilter.h"

#include <algorithm>

namespace ReverseModeC
{

namespace
{
// Lowpass cutoff morphs from maxLowpassHz (at the centre, where it has no
// audible effect) down to minLowpassHz (fully right).
constexpr float minLowpassHz = 300.0f;
constexpr float maxLowpassHz = 18000.0f;

// Highpass cutoff morphs from minHighpassHz (at the centre, where it has no
// audible effect) up to maxHighpassHz (fully left).
constexpr float minHighpassHz = 20.0f;
constexpr float maxHighpassHz = 2000.0f;
}

void MorphingFilter::prepare(double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, 0, (juce::uint32) numChannels };
    filter.prepare(spec);
    filter.reset();
}

float MorphingFilter::processSample(int channel, float input, float morph)
{
    // StateVariableTPTFilter requires a cutoff strictly below Nyquist. The
    // musical ranges above are tuned for typical audio sample rates; clamp
    // them down for any lower rate (e.g. a low test sample rate) rather than
    // handing the filter an invalid cutoff.
    const auto maxCutoffHz = (float) (sampleRate * 0.49);

    if (morph >= 0.5f)
    {
        // 0 at the centre (no filtering), 1 at fully right (strongest lowpass).
        const auto t = (morph - 0.5f) * 2.0f;
        const auto cutoff = std::min(maxLowpassHz - t * (maxLowpassHz - minLowpassHz), maxCutoffHz);

        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency(std::max(cutoff, 1.0f));

        const auto filtered = filter.processSample(channel, input);
        return input + (filtered - input) * t;
    }

    // 0 at the centre (no filtering), 1 at fully left (strongest highpass).
    const auto t = (0.5f - morph) * 2.0f;
    const auto cutoff = std::min(minHighpassHz + t * (maxHighpassHz - minHighpassHz), maxCutoffHz);

    filter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    filter.setCutoffFrequency(std::max(cutoff, 1.0f));

    const auto filtered = filter.processSample(channel, input);
    return input + (filtered - input) * t;
}

} // namespace ReverseModeC

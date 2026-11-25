#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ReverseModeC
{

// Parameter IDs are part of saved state and host automation: once released,
// append new ones, never rename or reorder existing ones.
namespace ParamIDs
{
inline constexpr auto mix = "mix";
inline constexpr auto bufferLength = "bufferLength";
inline constexpr auto feedback = "feedback";
inline constexpr auto balance = "balance";
inline constexpr auto filter = "filter";
inline constexpr auto modDepth = "modDepth";
inline constexpr auto modRate = "modRate";
inline constexpr auto modSync = "modSync";
inline constexpr auto bypass = "bypass";
} // namespace ParamIDs

// The parameter IDs that carry a continuously-variable (smoothed) value, as
// opposed to the discrete Buffer Length choice and the two boolean toggles.
// Shared between the parameter layout and the audio-thread smoothing setup
// so the two can't drift apart.
inline const juce::StringArray smoothedParamIDs { ParamIDs::mix,      ParamIDs::feedback, ParamIDs::balance,
                                                   ParamIDs::filter,  ParamIDs::modDepth, ParamIDs::modRate };

inline const juce::StringArray bufferLengthChoices { "1s", "2s", "4s" };

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace ReverseModeC

#include "PluginProcessor.h"

// Kept separate from PluginProcessor.cpp so the test executable can compile
// the processor without linking juce_audio_plugin_client.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverseModeCAudioProcessor();
}

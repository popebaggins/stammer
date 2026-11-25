#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class ReverseModeCAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReverseModeCAudioProcessorEditor(ReverseModeCAudioProcessor&);
    ~ReverseModeCAudioProcessorEditor() override = default;

    void resized() override;

private:
    ReverseModeCAudioProcessor& processor;

    struct SliderWithAttachment
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    SliderWithAttachment mixSlider, feedbackSlider, balanceSlider, filterSlider, modDepthSlider, modRateSlider;

    juce::ComboBox bufferLengthBox;
    juce::Label bufferLengthLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bufferLengthAttachment;

    juce::ToggleButton modSyncButton { "Mod Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modSyncAttachment;

    juce::ToggleButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverseModeCAudioProcessorEditor)
};

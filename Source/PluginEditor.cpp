#include "PluginEditor.h"

namespace
{
constexpr int sliderWidth = 90;
constexpr int sliderHeight = 100;
constexpr int rowHeight = 110;
}

ReverseModeCAudioProcessorEditor::ReverseModeCAudioProcessorEditor(ReverseModeCAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), processor(processorToEdit)
{
    auto setUpSlider = [this](SliderWithAttachment& s, const juce::String& paramID, const juce::String& labelText)
    {
        addAndMakeVisible(s.slider);
        s.label.setText(labelText, juce::dontSendNotification);
        s.label.setJustificationType(juce::Justification::centred);
        s.label.attachToComponent(&s.slider, false);
        addAndMakeVisible(s.label);
        s.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, paramID, s.slider);
    };

    setUpSlider(mixSlider, ReverseModeC::ParamIDs::mix, "Mix");
    setUpSlider(feedbackSlider, ReverseModeC::ParamIDs::feedback, "Feedback");
    setUpSlider(balanceSlider, ReverseModeC::ParamIDs::balance, "Balance");
    setUpSlider(filterSlider, ReverseModeC::ParamIDs::filter, "Filter");
    setUpSlider(modDepthSlider, ReverseModeC::ParamIDs::modDepth, "Mod Depth");
    setUpSlider(modRateSlider, ReverseModeC::ParamIDs::modRate, "Mod Rate");

    bufferLengthBox.addItemList(ReverseModeC::bufferLengthChoices, 1);
    addAndMakeVisible(bufferLengthBox);
    bufferLengthLabel.setText("Buffer Length", juce::dontSendNotification);
    bufferLengthLabel.attachToComponent(&bufferLengthBox, false);
    addAndMakeVisible(bufferLengthLabel);
    bufferLengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ReverseModeC::ParamIDs::bufferLength, bufferLengthBox);

    addAndMakeVisible(modSyncButton);
    modSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, ReverseModeC::ParamIDs::modSync, modSyncButton);

    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, ReverseModeC::ParamIDs::bypass, bypassButton);

    setSize(7 * sliderWidth + 20, 2 * rowHeight + 40);
}

void ReverseModeCAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(20); // room for the attached labels above each control

    auto knobRow = area.removeFromTop(sliderHeight);
    for (auto* s : { &mixSlider, &feedbackSlider, &balanceSlider, &filterSlider, &modDepthSlider, &modRateSlider })
        s->slider.setBounds(knobRow.removeFromLeft(sliderWidth));

    area.removeFromTop(20);
    auto toggleRow = area.removeFromTop(rowHeight - 20);
    bufferLengthBox.setBounds(toggleRow.removeFromLeft(sliderWidth).withHeight(24));
    modSyncButton.setBounds(toggleRow.removeFromLeft(sliderWidth).withHeight(24));
    bypassButton.setBounds(toggleRow.removeFromLeft(sliderWidth).withHeight(24));
}

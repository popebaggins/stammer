#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <optional>

namespace ReverseModeC
{

// Resolves a request path from the WebBrowserComponent's ResourceProvider
// (e.g. "/", "/index.js") to the matching built WebView UI asset, embedded
// at compile time as binary resources from gui/dist (see CMakeLists.txt).
// A free function rather than a Component method so it's testable without
// constructing a WebBrowserComponent.
std::optional<juce::WebBrowserComponent::Resource> getWebUIResource(const juce::String& url);

} // namespace ReverseModeC

class ReverseModeCAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReverseModeCAudioProcessorEditor(ReverseModeCAudioProcessor&);
    ~ReverseModeCAudioProcessorEditor() override = default;

    void resized() override;

    // Lets the host's own UI (e.g. a right-click "assign automation/MIDI"
    // context menu) know which parameter is under the mouse inside the
    // WebView, via WebControlParameterIndexReceiver below.
    int getControlParameterIndex(Component&) override;

private:
    ReverseModeCAudioProcessor& processor;

    // One relay per parameter: each makes a named JS object available
    // inside the WebView (Juce.getSliderState/getToggleState with the same
    // name on the frontend), kept in sync with the audio parameter by the
    // matching *ParameterAttachment below. Declared before webView, since
    // Options::withOptionsFrom(...) in webView's initialiser reads them and
    // members initialise in declaration order.
    juce::WebSliderRelay mixRelay { "mix" };
    juce::WebSliderRelay timeRelay { "time" };
    juce::WebSliderRelay feedbackRelay { "feedback" };
    juce::WebSliderRelay balanceRelay { "balance" };
    juce::WebSliderRelay filterRelay { "filter" };
    juce::WebSliderRelay modDepthRelay { "modDepth" };
    juce::WebSliderRelay modRateRelay { "modRate" };

    juce::WebToggleButtonRelay modSyncRelay { "modSync" };
    juce::WebToggleButtonRelay bypassRelay { "bypass" };

    juce::WebControlParameterIndexReceiver controlParameterIndexReceiver;

    // Restricts navigation to our own embedded page: native integration
    // (window.__JUCE__.backend) is only safe to expose to content we
    // control, so this refuses to follow a link to anywhere else.
    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad(const juce::String& newURL) override
        {
            return newURL == juce::WebBrowserComponent::getResourceProviderRoot();
        }
    };

    SinglePageBrowser webView { juce::WebBrowserComponent::Options {}
                                     .withNativeIntegrationEnabled()
                                     .withOptionsFrom(mixRelay)
                                     .withOptionsFrom(timeRelay)
                                     .withOptionsFrom(feedbackRelay)
                                     .withOptionsFrom(balanceRelay)
                                     .withOptionsFrom(filterRelay)
                                     .withOptionsFrom(modDepthRelay)
                                     .withOptionsFrom(modRateRelay)
                                     .withOptionsFrom(modSyncRelay)
                                     .withOptionsFrom(bypassRelay)
                                     .withOptionsFrom(controlParameterIndexReceiver)
                                     .withResourceProvider(ReverseModeC::getWebUIResource) };

    juce::WebSliderParameterAttachment mixAttachment;
    juce::WebSliderParameterAttachment timeAttachment;
    juce::WebSliderParameterAttachment feedbackAttachment;
    juce::WebSliderParameterAttachment balanceAttachment;
    juce::WebSliderParameterAttachment filterAttachment;
    juce::WebSliderParameterAttachment modDepthAttachment;
    juce::WebSliderParameterAttachment modRateAttachment;

    juce::WebToggleButtonParameterAttachment modSyncAttachment;
    juce::WebToggleButtonParameterAttachment bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverseModeCAudioProcessorEditor)
};

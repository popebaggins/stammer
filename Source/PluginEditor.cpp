#include "PluginEditor.h"

#include <BinaryData.h>

#include <cstring>

namespace ReverseModeC
{

namespace
{
    struct NamedAsset
    {
        const char* data;
        int size;
        const char* mimeType;
    };

    NamedAsset indexHtml() { return { BinaryData::index_html, BinaryData::index_htmlSize, "text/html" }; }
}

std::optional<juce::WebBrowserComponent::Resource> getWebUIResource(const juce::String& url)
{
    // "/" is the request the ResourceProvider receives when the browser is
    // sent to WebBrowserComponent::getResourceProviderRoot() (see webView's
    // construction below).
    const auto path = url == "/" ? juce::String("index.html") : url.fromFirstOccurrenceOf("/", false, false);

    std::optional<NamedAsset> asset;

    if (path == "index.js")
        asset = NamedAsset { BinaryData::index_js, BinaryData::index_jsSize, "text/javascript" };
    else if (path == "index.css")
        asset = NamedAsset { BinaryData::index_css, BinaryData::index_cssSize, "text/css" };
    else if (path == "index.html" || !path.contains("."))
        // This is a single-page app with no client-side router, so any path
        // that isn't a known built asset -- including a deep link with no
        // file extension -- falls back to the app shell, the same way a
        // router's catch-all route would. A path that does name a specific,
        // unrecognised asset extension (e.g. a stray "/favicon.ico") is left
        // as a genuine missing resource below instead.
        asset = indexHtml();

    if (!asset.has_value())
        return std::nullopt;

    const auto size = asset->size;
    std::vector<std::byte> bytes((size_t) size);
    const auto* data = asset->data;
    const auto* mimeType = asset->mimeType;
    std::memcpy(bytes.data(), data, (size_t) size);
    return juce::WebBrowserComponent::Resource { std::move(bytes), juce::String(mimeType) };
}

} // namespace ReverseModeC

namespace
{
constexpr int defaultWidth = 640;
constexpr int defaultHeight = 320;
constexpr int minWidth = 480;
constexpr int minHeight = 240;
}

ReverseModeCAudioProcessorEditor::ReverseModeCAudioProcessorEditor(ReverseModeCAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), processor(processorToEdit),
      mixAttachment(*processor.apvts.getParameter(ReverseModeC::ParamIDs::mix), mixRelay, processor.apvts.undoManager),
      timeAttachment(
          *processor.apvts.getParameter(ReverseModeC::ParamIDs::time), timeRelay, processor.apvts.undoManager),
      feedbackAttachment(*processor.apvts.getParameter(ReverseModeC::ParamIDs::feedback), feedbackRelay,
          processor.apvts.undoManager),
      balanceAttachment(*processor.apvts.getParameter(ReverseModeC::ParamIDs::balance), balanceRelay,
          processor.apvts.undoManager),
      filterAttachment(
          *processor.apvts.getParameter(ReverseModeC::ParamIDs::filter), filterRelay, processor.apvts.undoManager),
      modDepthAttachment(*processor.apvts.getParameter(ReverseModeC::ParamIDs::modDepth), modDepthRelay,
          processor.apvts.undoManager),
      modRateAttachment(*processor.apvts.getParameter(ReverseModeC::ParamIDs::modRate), modRateRelay,
          processor.apvts.undoManager),
      modSyncAttachment(
          *processor.apvts.getParameter(ReverseModeC::ParamIDs::modSync), modSyncRelay, processor.apvts.undoManager),
      bypassAttachment(
          *processor.apvts.getParameter(ReverseModeC::ParamIDs::bypass), bypassRelay, processor.apvts.undoManager)
{
    addAndMakeVisible(webView);
    webView.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable(true, true);
    if (auto* sizeConstrainer = getConstrainer())
        sizeConstrainer->setSizeLimits(minWidth, minHeight, 2000, 1200);

    setSize(defaultWidth, defaultHeight);
}

void ReverseModeCAudioProcessorEditor::resized()
{
    webView.setBounds(getLocalBounds());
}

int ReverseModeCAudioProcessorEditor::getControlParameterIndex(Component&)
{
    return controlParameterIndexReceiver.getControlParameterIndex();
}

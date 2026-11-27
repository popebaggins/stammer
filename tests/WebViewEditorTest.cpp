#include <catch2/catch_test_macros.hpp>

#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cstring>

namespace
{
juce::String bytesToString(const std::vector<std::byte>& bytes)
{
    return juce::String::fromUTF8(reinterpret_cast<const char*>(bytes.data()), (int) bytes.size());
}
}

// These cover the WebView communication harness (seam 1: "WebView loads and
// communicates with AudioProcessor") at the ResourceProvider level, which is
// the part of the bridge that's testable without spinning up a real
// WebKitGTK browser and event loop. getWebUIResource is a free function
// precisely so it can be exercised here on its own.
TEST_CASE("The WebView resource provider serves the built UI's index.html at the resource-provider root", "[webview]")
{
    const auto resource = ReverseModeC::getWebUIResource("/");
    REQUIRE(resource.has_value());
    CHECK(resource->mimeType == "text/html");
    CHECK(resource->data.size() > 0);

    const auto text = bytesToString(resource->data);
    CHECK(text.contains("<div id=\"root\">"));
    CHECK(text.contains("index.js"));
}

TEST_CASE("The WebView resource provider serves the built UI's JS bundle with the right mime type", "[webview]")
{
    const auto resource = ReverseModeC::getWebUIResource("/index.js");
    REQUIRE(resource.has_value());
    CHECK(resource->mimeType == "text/javascript");
    CHECK(resource->data.size() > 0);
}

TEST_CASE("The WebView resource provider serves the built UI's stylesheet with the right mime type", "[webview]")
{
    const auto resource = ReverseModeC::getWebUIResource("/index.css");
    REQUIRE(resource.has_value());
    CHECK(resource->mimeType == "text/css");
    CHECK(resource->data.size() > 0);
}

TEST_CASE("The WebView resource provider has no client-side router, so an unknown deep path still resolves to "
          "index.html",
    "[webview]")
{
    // The React app is a single page; any path other than a known built
    // asset should still resolve to index.html rather than 404, matching
    // what a browser would do navigating straight to a nested URL.
    const auto resource = ReverseModeC::getWebUIResource("/some/unknown/path");
    REQUIRE(resource.has_value());
    CHECK(resource->mimeType == "text/html");
}

TEST_CASE("The WebView resource provider returns no resource for a path with an unrecognised extension",
    "[webview]")
{
    const auto resource = ReverseModeC::getWebUIResource("/index.unknownext");
    CHECK_FALSE(resource.has_value());
}

TEST_CASE("The editor constructs with a WebView wired to all 9 parameters and can be destroyed cleanly", "[webview]")
{
    // Catches: a relay/attachment left out for one of the 9 parameters
    // (construction would throw or assert, since
    // Web*ParameterAttachment's constructor dereferences
    // apvts.getParameter(id)), and any ordering bug between the relays and
    // the WebBrowserComponent (webView's Options::withOptionsFrom calls
    // read already-constructed relay members -- see PluginEditor.h).
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    REQUIRE_NOTHROW([&]
    {
        ReverseModeCAudioProcessorEditor editor(processor);
    }());
}

TEST_CASE("The editor reports no control under the mouse until the WebView frontend says otherwise", "[webview]")
{
    // Sanity check on WebControlParameterIndexReceiver wiring: it starts at
    // -1 (JUCE's documented "no control" value) and nothing in editor
    // construction should change that before the frontend's
    // ControlParameterIndexUpdater ever reports a real index.
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);
    ReverseModeCAudioProcessorEditor editor(processor);

    CHECK(editor.getControlParameterIndex(editor) == -1);
}

TEST_CASE("The editor opens at a sensible default size and stays resizable down to a usable minimum", "[webview]")
{
    // Acceptance criterion: "UI resizes/displays correctly in both Reaper
    // and Bitwig Studio" starts here -- the editor must actually report
    // itself as resizable with sane bounds, which is what a host asks it
    // for when deciding how its own window chrome behaves.
    ReverseModeCAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);
    ReverseModeCAudioProcessorEditor editor(processor);

    CHECK(editor.getWidth() > 0);
    CHECK(editor.getHeight() > 0);
    CHECK(editor.isResizable());
}

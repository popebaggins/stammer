#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Parameters.h"
#include "PluginProcessor.h"

using Catch::Matchers::WithinAbs;

// A DAW calls getStateInformation() when saving a project/preset and
// setStateInformation() when reloading one. If a plugin's parameter values
// don't round-trip through those calls, every saved session silently
// reverts to default parameter values on reload.
TEST_CASE("Parameter values round-trip through getStateInformation/setStateInformation", "[state]")
{
    ReverseModeCAudioProcessor processor;

    // Push every parameter to a distinctive non-default value.
    processor.apvts.getParameter(ReverseModeC::ParamIDs::mix)->setValueNotifyingHost(0.1f);
    processor.apvts.getParameter(ReverseModeC::ParamIDs::time)->setValueNotifyingHost(0.9f);
    processor.apvts.getParameter(ReverseModeC::ParamIDs::feedback)->setValueNotifyingHost(0.8f);
    processor.apvts.getParameter(ReverseModeC::ParamIDs::balance)->setValueNotifyingHost(0.75f);
    processor.apvts.getParameter(ReverseModeC::ParamIDs::filter)->setValueNotifyingHost(0.2f);
    processor.apvts.getParameter(ReverseModeC::ParamIDs::modDepth)->setValueNotifyingHost(0.65f);
    processor.apvts.getParameter(ReverseModeC::ParamIDs::modRate)->setValueNotifyingHost(0.4f);
    processor.apvts.getParameter(ReverseModeC::ParamIDs::modSync)->setValueNotifyingHost(1.0f);
    processor.apvts.getParameter(ReverseModeC::ParamIDs::bypass)->setValueNotifyingHost(1.0f);

    juce::MemoryBlock savedState;
    processor.getStateInformation(savedState);

    REQUIRE(savedState.getSize() > 0);

    ReverseModeCAudioProcessor restored;
    restored.setStateInformation(savedState.getData(), (int) savedState.getSize());

    for (const auto& id : { ReverseModeC::ParamIDs::mix, ReverseModeC::ParamIDs::time,
             ReverseModeC::ParamIDs::feedback, ReverseModeC::ParamIDs::balance, ReverseModeC::ParamIDs::filter,
             ReverseModeC::ParamIDs::modDepth, ReverseModeC::ParamIDs::modRate, ReverseModeC::ParamIDs::modSync,
             ReverseModeC::ParamIDs::bypass })
    {
        INFO("parameter " << id);
        auto* original = processor.apvts.getParameter(id);
        auto* reloaded = restored.apvts.getParameter(id);
        CHECK_THAT((double) reloaded->getValue(), WithinAbs((double) original->getValue(), 1.0e-6));
    }
}

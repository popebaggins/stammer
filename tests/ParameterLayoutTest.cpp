#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Parameters.h"
#include "PluginProcessor.h"

using Catch::Matchers::WithinAbs;

namespace
{
template <typename ParamType>
const ParamType& requireParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    auto* param = apvts.getParameter(id);
    REQUIRE(param != nullptr);
    auto* typed = dynamic_cast<ParamType*>(param);
    REQUIRE(typed != nullptr);
    return *typed;
}
}

TEST_CASE("The six continuous knobs exist with their documented ranges and defaults", "[parameters]")
{
    ReverseModeCAudioProcessor processor;

    struct Expected
    {
        juce::String id;
        float rangeStart, rangeEnd, defaultValue;
    };

    const Expected expectations[] = {
        { ReverseModeC::ParamIDs::mix, 0.0f, 1.0f, 0.5f },
        { ReverseModeC::ParamIDs::feedback, 0.0f, 1.0f, 0.3f },
        { ReverseModeC::ParamIDs::balance, -1.0f, 1.0f, 0.0f },
        { ReverseModeC::ParamIDs::filter, 0.0f, 1.0f, 0.5f },
        { ReverseModeC::ParamIDs::modDepth, 0.0f, 1.0f, 0.0f },
        { ReverseModeC::ParamIDs::modRate, 0.1f, 10.0f, 2.0f },
    };

    for (const auto& expected : expectations)
    {
        INFO("parameter " << expected.id);
        const auto& param = requireParam<juce::AudioParameterFloat>(processor.apvts, expected.id);
        CHECK_THAT((double) param.range.start, WithinAbs((double) expected.rangeStart, 1.0e-6));
        CHECK_THAT((double) param.range.end, WithinAbs((double) expected.rangeEnd, 1.0e-6));
        CHECK_THAT((double) param.get(), WithinAbs((double) expected.defaultValue, 1.0e-6));
    }
}

TEST_CASE("Buffer Length is a discrete 1s/2s/4s choice, defaulting to 2s", "[parameters]")
{
    ReverseModeCAudioProcessor processor;

    const auto& param = requireParam<juce::AudioParameterChoice>(processor.apvts, ReverseModeC::ParamIDs::bufferLength);
    CHECK(param.choices == ReverseModeC::bufferLengthChoices);
    CHECK(param.getIndex() == 1);
}

TEST_CASE("Mod Sync/Free and Bypass are boolean toggles, both off by default", "[parameters]")
{
    ReverseModeCAudioProcessor processor;

    const auto& modSync = requireParam<juce::AudioParameterBool>(processor.apvts, ReverseModeC::ParamIDs::modSync);
    CHECK(modSync.get() == false);

    const auto& bypass = requireParam<juce::AudioParameterBool>(processor.apvts, ReverseModeC::ParamIDs::bypass);
    CHECK(bypass.get() == false);
}

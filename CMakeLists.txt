cmake_minimum_required(VERSION 3.22)

project(ReverseModeC VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
include(CTest)

# JUCE is fetched at configure time rather than vendored, to keep the repo
# lean. Pin to a specific tag for reproducible builds (satisfies the 8.1+
# requirement from ticket 01).
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ReverseModeC
    COMPANY_NAME "Reverse Mode C Project"
    BUNDLE_ID "com.reversemodecproject.reversemodec"
    PLUGIN_MANUFACTURER_CODE Rmcp
    PLUGIN_CODE Rvc1
    FORMATS VST3
    PRODUCT_NAME "Reverse Mode C"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES "Fx"
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(ReverseModeC
    PRIVATE
        Source/Parameters.cpp
        Source/ReverseBuffer.cpp
        Source/MorphingFilter.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/PluginMain.cpp)

target_compile_definitions(ReverseModeC
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ReverseModeC
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(REVERSE_MODE_C_BUILD_TESTS "Build the Catch2 test suite" ON)
if(REVERSE_MODE_C_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.16.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)
    add_subdirectory(tests)
endif()

add_executable(reverse_mode_c_tests
    ParameterLayoutTest.cpp
    ProcessBlockTest.cpp
    SmoothingTest.cpp
    ReverseBufferTest.cpp
    BlendTest.cpp
    TimeControlTest.cpp
    FeedbackFilterTest.cpp
    VibratoTest.cpp
    WebViewEditorTest.cpp
    StateSaveRestoreTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/Parameters.cpp
    ${CMAKE_SOURCE_DIR}/Source/ReverseBuffer.cpp
    ${CMAKE_SOURCE_DIR}/Source/MorphingFilter.cpp
    ${CMAKE_SOURCE_DIR}/Source/VibratoDelay.cpp
    ${CMAKE_SOURCE_DIR}/Source/PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/Source/PluginEditor.cpp)

target_include_directories(reverse_mode_c_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(reverse_mode_c_tests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JucePlugin_Name="ReverseModeC"
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0)

target_link_libraries(reverse_mode_c_tests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::pkgconfig_JUCE_BROWSER_LINUX_DEPS
    ReverseModeCGuiAssets
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

include(Catch)
catch_discover_tests(reverse_mode_c_tests)

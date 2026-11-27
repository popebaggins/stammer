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
    NEEDS_WEB_BROWSER TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(ReverseModeC
    PRIVATE
        Source/Parameters.cpp
        Source/ReverseBuffer.cpp
        Source/MorphingFilter.cpp
        Source/VibratoDelay.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/PluginMain.cpp)

target_compile_definitions(ReverseModeC
    PUBLIC
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

# WebView UI (ticket 07): the React/TypeScript frontend under gui/ is built
# by npm/esbuild and embedded as binary resources, served to the
# juce::WebBrowserComponent by Source/PluginEditor.cpp's ResourceProvider.
# See gui/README.md.
find_program(NPM_EXECUTABLE npm)
if(NOT NPM_EXECUTABLE)
    message(FATAL_ERROR "npm not found: Node.js is required to build the WebView UI (see gui/README.md).")
endif()

set(REVERSE_MODE_C_GUI_DIR ${CMAKE_SOURCE_DIR}/gui)
set(REVERSE_MODE_C_GUI_DIST ${REVERSE_MODE_C_GUI_DIR}/dist)
set(REVERSE_MODE_C_GUI_INSTALL_STAMP ${REVERSE_MODE_C_GUI_DIR}/node_modules/.install-stamp)

add_custom_command(
    OUTPUT ${REVERSE_MODE_C_GUI_INSTALL_STAMP}
    COMMAND ${NPM_EXECUTABLE} ci
    COMMAND ${CMAKE_COMMAND} -E touch ${REVERSE_MODE_C_GUI_INSTALL_STAMP}
    WORKING_DIRECTORY ${REVERSE_MODE_C_GUI_DIR}
    DEPENDS ${REVERSE_MODE_C_GUI_DIR}/package.json ${REVERSE_MODE_C_GUI_DIR}/package-lock.json
    COMMENT "Installing WebView UI npm dependencies (gui/)")

add_custom_command(
    OUTPUT ${REVERSE_MODE_C_GUI_DIST}/index.html ${REVERSE_MODE_C_GUI_DIST}/index.js ${REVERSE_MODE_C_GUI_DIST}/index.css
    COMMAND ${NPM_EXECUTABLE} run build
    WORKING_DIRECTORY ${REVERSE_MODE_C_GUI_DIR}
    DEPENDS
        ${REVERSE_MODE_C_GUI_INSTALL_STAMP}
        ${REVERSE_MODE_C_GUI_DIR}/build.mjs
        ${REVERSE_MODE_C_GUI_DIR}/tsconfig.json
        ${REVERSE_MODE_C_GUI_DIR}/public/index.html
        ${REVERSE_MODE_C_GUI_DIR}/src/index.tsx
        ${REVERSE_MODE_C_GUI_DIR}/src/App.tsx
        ${REVERSE_MODE_C_GUI_DIR}/src/App.css
        ${REVERSE_MODE_C_GUI_DIR}/src/index.css
        ${REVERSE_MODE_C_GUI_DIR}/src/components/ParamSlider.tsx
        ${REVERSE_MODE_C_GUI_DIR}/src/components/ParamToggle.tsx
    COMMENT "Building WebView UI (gui/ React app -> gui/dist)")

juce_add_binary_data(ReverseModeCGuiAssets
    SOURCES
        ${REVERSE_MODE_C_GUI_DIST}/index.html
        ${REVERSE_MODE_C_GUI_DIST}/index.js
        ${REVERSE_MODE_C_GUI_DIST}/index.css)

target_link_libraries(ReverseModeC
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        ReverseModeCGuiAssets
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

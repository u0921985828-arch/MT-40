cmake_minimum_required(VERSION 3.22)

project(MoogVASynth VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# JUCE
#
# By default JUCE is pulled in with FetchContent so the project is self
# contained.  If you already have a local JUCE checkout, point JUCE_DIR at it
# (-DJUCE_DIR=/path/to/JUCE) to skip the download.
# -----------------------------------------------------------------------------
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(MoogVASynth
    COMPANY_NAME            "MoogVA"
    PLUGIN_MANUFACTURER_CODE Mova
    PLUGIN_CODE             Mvas
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "MoogVA Synth"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(MoogVASynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/SynthVoice.cpp
    Source/UI/LookAndFeelMoog.cpp)

target_include_directories(MoogVASynth PRIVATE Source)

target_compile_definitions(MoogVASynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MoogVASynth PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

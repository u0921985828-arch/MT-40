cmake_minimum_required(VERSION 3.22)

project(CasioMT40 VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Casio MT-40 VST/AU emulator
#
# Real-time DSP synthesis of the 1981 Casio MT-40 (Casiotone MT-40). No PCM
# sample playback is used anywhere: every sound generator is synthesized from
# the DSP logic described in docs/ARCHITECTURE.md.
#
# JUCE is pulled in via FetchContent so the project is self-contained. If you
# already have a local JUCE checkout, pass -DJUCE_DIR=/path/to/JUCE to reuse it.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# --- Acquire JUCE -----------------------------------------------------------
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(STATUS "Using local JUCE at ${JUCE_DIR}")
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    message(STATUS "Fetching JUCE via FetchContent")
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Plugin target ----------------------------------------------------------
juce_add_plugin(CasioMT40
    COMPANY_NAME              "OpenEmu DSP"
    PLUGIN_MANUFACTURER_CODE  Oedp
    PLUGIN_CODE               Mt40
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "Casio MT-40"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE)

target_sources(CasioMT40 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/dsp/DCOVoice.cpp
    Source/dsp/VoiceAllocator.cpp
    Source/dsp/TwinTResonator.cpp
    Source/dsp/SnareDrum.cpp
    Source/dsp/HiHat.cpp
    Source/dsp/SlengTengBass.cpp
    Source/dsp/RhythmEngine.cpp
    Source/dsp/CasioChord.cpp
    Source/dsp/Presets.cpp)

target_compile_definitions(CasioMT40 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_include_directories(CasioMT40 PRIVATE Source)

target_link_libraries(CasioMT40 PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_processors
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

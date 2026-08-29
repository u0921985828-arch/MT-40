cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
#  Phenotype — VST3/AU diploid granular cross-synthesis plugin.
#
#  JUCE 8, C++20, native WebView UI (no Projucer). The React/WebGL frontend is
#  built separately (see ui/) and its dist/ output is embedded as binary data
#  so the plugin ships as a single self-contained binary.
#
#  Configure the frontend first:
#      cd ui && npm ci && npm run build
#  then:
#      cmake -B build -DCMAKE_BUILD_TYPE=Release
#      cmake --build build --parallel
# ---------------------------------------------------------------------------

project(Phenotype VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# --- Acquire JUCE 8 ---------------------------------------------------------
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(STATUS "Phenotype: using local JUCE at ${JUCE_DIR}")
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    message(STATUS "Phenotype: fetching JUCE 8 via FetchContent")
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Plugin target ----------------------------------------------------------
juce_add_plugin(Phenotype
    COMPANY_NAME              "Botanica DSP"
    PLUGIN_MANUFACTURER_CODE  Btnc
    PLUGIN_CODE               Phnt
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "Phenotype"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    NEEDS_WEBVIEW2            TRUE
    COPY_PLUGIN_AFTER_BUILD   TRUE)

target_sources(Phenotype PRIVATE
    Source/PluginProcessor.cpp
    Source/PhenotypeWebEditor.cpp
    Source/dsp/GranularEngine.cpp)

target_compile_definitions(Phenotype PUBLIC
    JUCE_WEB_BROWSER=1            # native WebView UI
    JUCE_USE_CURL=0
    JUCE_USE_WIN_WEBVIEW2=1       # Windows backend
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_include_directories(Phenotype PRIVATE Source)

# --- Embed the built React/WebGL frontend -----------------------------------
# The frontend build (Vite) emits a single-file bundle into ui/dist. We glob
# whatever it produced so hashing/asset names need not be hard-coded.
set(PHENOTYPE_UI_DIST "${CMAKE_CURRENT_SOURCE_DIR}/ui/dist")
file(GLOB PHENOTYPE_UI_FILES CONFIGURE_DEPENDS
     "${PHENOTYPE_UI_DIST}/*.html"
     "${PHENOTYPE_UI_DIST}/*.js"
     "${PHENOTYPE_UI_DIST}/*.css")

if(NOT PHENOTYPE_UI_FILES)
    message(WARNING
        "Phenotype: ui/dist is empty. Build the frontend first: "
        "(cd ui && npm ci && npm run build). Embedding a placeholder page.")
    set(PHENOTYPE_UI_FILES "${CMAKE_CURRENT_SOURCE_DIR}/ui/fallback.html")
endif()

juce_add_binary_data(PhenotypeUI SOURCES ${PHENOTYPE_UI_FILES})

# BinaryData objects must be PIC when linked into the shared plugin format.
set_target_properties(PhenotypeUI PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(Phenotype PRIVATE
    PhenotypeUI
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_dsp
    juce::juce_audio_processors
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# On Linux the WebView backend needs GTK3 + WebKitGTK headers/libs. Attach them
# to the target explicitly so juce_gui_extra's browser code finds gtk/gtk.h
# (Ubuntu 24.04 ships webkit2gtk-4.1).
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PH_WEBKIT IMPORTED_TARGET webkit2gtk-4.1)
    if(NOT PH_WEBKIT_FOUND)
        pkg_check_modules(PH_WEBKIT IMPORTED_TARGET webkit2gtk-4.0)
    endif()
    if(PH_WEBKIT_FOUND)
        target_link_libraries(Phenotype PRIVATE PkgConfig::PH_WEBKIT)
    endif()
endif()

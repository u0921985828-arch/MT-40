cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
#  Phenotype DSP tests — standalone, JUCE-independent, network-free.
#
#      cmake -B build-tests phenotype/tests
#      cmake --build build-tests
#      ctest --test-dir build-tests --output-on-failure
# ---------------------------------------------------------------------------

project(PhenotypeDspTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_executable(dsp_tests
    dsp_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Source/dsp/GranularEngine.cpp)

target_include_directories(dsp_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Source/dsp)

if(NOT MSVC)
    target_compile_options(dsp_tests PRIVATE -Wall -Wextra -Werror -O2)
endif()

add_test(NAME dsp_core COMMAND dsp_tests)

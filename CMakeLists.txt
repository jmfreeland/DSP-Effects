cmake_minimum_required(VERSION 3.20)
project(DSPEffects VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DSP_EFFECTS_BUILD_PLUGIN "Build the JUCE VST3/AU plugin wrapper (fetches JUCE)" OFF)

add_library(dsp INTERFACE)
target_include_directories(dsp INTERFACE dsp/include)
target_compile_features(dsp INTERFACE cxx_std_20)

add_subdirectory(host)

if(DSP_EFFECTS_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()

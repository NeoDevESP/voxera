cmake_minimum_required(VERSION 3.22)

project(VOXERA VERSION 0.8.0 LANGUAGES C CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Set before any target is declared, or the architecture and deployment target
# do not reach them. One universal binary runs on both Apple Silicon and Intel,
# which matters because the receiver may have either.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8
)
FetchContent_MakeAvailable(JUCE)

# Rubber Band 4.0.0 gives us the LiveShifter API. It has no CMake build
# requirement here because we compile its official single translation unit.
FetchContent_Declare(
    rubberband
    GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG 1d95888bec3ae0a17c0c4af791810d5a63f6bc35
)

FetchContent_MakeAvailable(rubberband)

juce_add_plugin(VOXERA
    COMPANY_NAME "Trevor Deey Labs"
    BUNDLE_ID "com.trevordeey.voxera"
    PLUGIN_MANUFACTURER_CODE Tdly
    PLUGIN_CODE Vx01
    # AU is macOS-only and JUCE drops it elsewhere, but without it Logic Pro and
    # GarageBand cannot see the plugin at all: neither host loads VST3.
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "VOXERA"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(VOXERA)

target_sources(VOXERA PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/DSP/AutoGain.h
    Source/DSP/Biquad.h
    Source/DSP/Metering.h
    Source/DSP/Saturator.h
    Source/DSP/SmartEQ.h
    Source/DSP/SpectralEngine.h
    Source/DSP/FractionalDelay.h
    Source/DSP/DuckingDelay.h
    Source/DSP/FDNReverb.h
    Source/DSP/SpatialEngine.h
    Source/DSP/YinPitchDetector.h
    Source/DSP/PitchQuantizer.h
    Source/DSP/PitchIntelligence.h
    Source/DSP/VoiceProfileEngine.h
    Source/DSP/PitchEngine.h
    Source/DSP/Limiter.h
    Source/DSP/Punch.h
    Source/DSP/Exciter.h
    Source/DSP/Gate.h
    Source/DSP/Optical.h
    Source/DSP/SoftClip.h
    Source/DSP/Character.h
    Source/DSP/AutoMix.h
    ${rubberband_SOURCE_DIR}/single/RubberBandSingle.cpp
)

target_include_directories(VOXERA PRIVATE
    ${rubberband_SOURCE_DIR}
)

target_compile_definitions(VOXERA
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        RUBBERBAND_STATIC=1
        NOMINMAX   # prevent Windows min/max macros from breaking std::min/std::max in RubberBand
)

target_link_libraries(VOXERA
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

option(VOXERA_BUILD_TESTS "Build VOXERA regression tests" OFF)
if(VOXERA_BUILD_TESTS)
    enable_testing()
    add_executable(VOXERA_AlgorithmTests Tests/algorithm_tests.cpp)
    add_executable(VOXERA_RealtimeTests Tests/realtime_tests.cpp)
    add_test(NAME pitch_control COMMAND VOXERA_AlgorithmTests)
    add_test(NAME realtime_utilities COMMAND VOXERA_RealtimeTests)
    add_executable(VOXERA_DetectorBenchmark Tests/detector_benchmark.cpp)
    target_include_directories(VOXERA_DetectorBenchmark PRIVATE Tests/headers ${rubberband_SOURCE_DIR})
    target_link_libraries(VOXERA_DetectorBenchmark PRIVATE VOXERA juce::juce_audio_utils juce::juce_dsp)
    add_test(NAME detector_benchmark COMMAND VOXERA_DetectorBenchmark)
    foreach(testName IN ITEMS DSP Integration)
        string(TOLOWER ${testName} sourceName)
        add_executable(VOXERA_${testName}Tests Tests/${sourceName}_tests.cpp)
        target_include_directories(VOXERA_${testName}Tests PRIVATE Tests/headers ${rubberband_SOURCE_DIR})
        target_compile_definitions(VOXERA_${testName}Tests PRIVATE JucePlugin_Name="VOXERA")
        target_link_libraries(VOXERA_${testName}Tests PRIVATE VOXERA juce::juce_audio_utils juce::juce_dsp)
        add_test(NAME ${sourceName} COMMAND VOXERA_${testName}Tests "${CMAKE_CURRENT_BINARY_DIR}/test-previews")
    endforeach()
endif()

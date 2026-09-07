cmake_minimum_required(VERSION 3.22)

project(VOXERA VERSION 1.0.0 LANGUAGES C CXX)

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

# RTNeural runs the trained network. Its default Eigen backend is vendored in
# the repository, so no submodules are needed; xsimd is a submodule and is left
# alone deliberately, since fetching it would make the build depend on a second
# host for no gain at the network sizes involved here.
set(RTNEURAL_EIGEN ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    RTNeural
    GIT_REPOSITORY https://github.com/jatinchowdhury18/RTNeural.git
    GIT_TAG 95c3c0f987a6fe903e7eec71e797405dbed7caf7
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(RTNeural)

# Component inspector, for development only. Two layout faults in this project
# were found by rendering the editor to a PNG and looking at it; this shows the
# same information live. It is off by default and must never enter a release
# binary, which is what the option is for rather than a comment asking nicely.
option(VOXERA_UI_INSPECTOR "Build with the Melatonin component inspector" OFF)
if(VOXERA_UI_INSPECTOR)
    # SOURCE_DIR is set explicitly because juce_add_module requires the folder
    # to carry the module's own name, and FetchContent would otherwise check it
    # out as "melatonin_inspector-src" and fail during configure.
    FetchContent_Declare(
        melatonin_inspector
        GIT_REPOSITORY https://github.com/sudara/melatonin_inspector.git
        GIT_TAG main
        SOURCE_DIR ${CMAKE_BINARY_DIR}/melatonin_inspector
    )
    FetchContent_MakeAvailable(melatonin_inspector)
endif()

juce_add_plugin(VOXERA
    COMPANY_NAME "Trevor Deey Labs"
    # The unique id a host uses is derived from these codes, so every build that
    # should appear separately in the rack needs its own. Vx01 is the frozen
    # 0.8, Vx10 and Vx20 the development builds, and this is Vx30. Reusing a
    # code would make the host update that entry instead of adding a plugin, and
    # would leave two files claiming one identity.
    BUNDLE_ID "com.trevordeey.voxera10"
    PLUGIN_MANUFACTURER_CODE Tdly
    PLUGIN_CODE Vx30
    # AU is macOS-only and JUCE drops it elsewhere, but without it Logic Pro and
    # GarageBand cannot see the plugin at all: neither host loads VST3.
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "VOXERA 1.0"
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
    Source/DSP/Upward.h
    Source/DSP/VocalLock.h
    Source/DSP/Chop.h
    Source/DSP/Modulation.h
    Source/DSP/SoftClip.h
    Source/DSP/Character.h
    Source/DSP/NeuralStage.h
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
        VOXERA_WITH_INSPECTOR=$<BOOL:${VOXERA_UI_INSPECTOR}>
)

if(APPLE)
    # Rubber Band's single translation unit defines HAVE_VDSP on Apple platforms
    # and uses Accelerate's FFT there, while every other platform gets its
    # built-in one. So this dependency exists only on macOS, and its absence is
    # invisible until a link actually happens there.
    target_link_libraries(VOXERA PUBLIC "-framework Accelerate")
endif()

target_link_libraries(VOXERA
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        # Private, so RTNeural's own compile options and Eigen's definitions stay
        # inside this target instead of reaching everything that links it. The
        # test targets include NeuralStage.h through PluginProcessor.h, so they
        # link RTNeural themselves rather than inheriting it from here.
        RTNeural
        $<$<BOOL:${VOXERA_UI_INSPECTOR}>:melatonin_inspector>
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
    target_link_libraries(VOXERA_DetectorBenchmark PRIVATE VOXERA RTNeural juce::juce_audio_utils juce::juce_dsp)
    add_test(NAME detector_benchmark COMMAND VOXERA_DetectorBenchmark)
    foreach(testName IN ITEMS DSP Integration)
        string(TOLOWER ${testName} sourceName)
        add_executable(VOXERA_${testName}Tests Tests/${sourceName}_tests.cpp)
        target_include_directories(VOXERA_${testName}Tests PRIVATE Tests/headers ${rubberband_SOURCE_DIR})
        target_compile_definitions(VOXERA_${testName}Tests PRIVATE JucePlugin_Name="VOXERA")
        target_link_libraries(VOXERA_${testName}Tests PRIVATE VOXERA RTNeural juce::juce_audio_utils juce::juce_dsp)
        add_test(NAME ${sourceName} COMMAND VOXERA_${testName}Tests "${CMAKE_CURRENT_BINARY_DIR}/test-previews")
    endforeach()
endif()

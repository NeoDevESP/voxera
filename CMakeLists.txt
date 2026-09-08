cmake_minimum_required(VERSION 3.22)

project(VOXERA VERSION 1.5.0 LANGUAGES C CXX)

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

# The official NAM inference core, MIT licensed.
#
# Hand-writing the .nam loader was a mistake, and a measurable one: the weight
# layout was read wrongly for months and the model that came out was not the one
# in the file. This is the implementation every other NAM plugin uses, and it
# reads the format by definition rather than by my reading of it.
#
# It also unlocks the library. A survey of the first forty-one models returned
# for "preamp" on TONE3000 found forty-one WaveNets and no LSTMs at all, so an
# LSTM-only stage is one that never loads anything anyone would download.
#
# It shares Eigen with RTNeural, which is already vendored above, so this adds
# no new third-party maths dependency.
FetchContent_Declare(
    NeuralAmpModelerCore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG 1f42f88535884450104b8711d7595019afa0495b   # v0.5.4
    # Fetched but deliberately not configured. Its own CMakeLists exists only to
    # build command-line tools we have no use for, and those pass GCC-style
    # warning flags that MSVC rejects outright, so adding it fails the Windows
    # build for the sake of two programs nobody here runs. Pointing this at a
    # directory that holds no CMakeLists tells FetchContent to download the
    # sources and stop, which is all we want: the translation units are compiled
    # into the plugin target below.
    SOURCE_SUBDIR  no-build-here
)
FetchContent_MakeAvailable(NeuralAmpModelerCore)

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
    # 0.8; Vx10 and Vx20 were development builds; Vx30 is the 1.0; this is the
    # 1.5. Reusing a code would make the host update that entry instead of
    # adding a plugin, and would leave two files claiming one identity.
    BUNDLE_ID "com.trevordeey.voxera15"
    PLUGIN_MANUFACTURER_CODE Tdly
    PLUGIN_CODE Vx40
    # AU is macOS-only and JUCE drops it elsewhere, but without it Logic Pro and
    # GarageBand cannot see the plugin at all: neither host loads VST3.
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "VOXERA 1.5"
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
    Source/DSP/ColourCompressor.h
    Source/DSP/VoiceMatch.h
    Source/DSP/AutoMix.h
    ${rubberband_SOURCE_DIR}/single/RubberBandSingle.cpp

)


# The NAM core as its own archive, linked whole.
#
# It ships no library target, so its translation units have to be compiled
# somewhere; the reason they are not simply added to the plugin is that the core
# registers each architecture's parser from a namespace-scope object inside the
# file that implements it. Nothing in this project refers to those files by
# name, so the linker discards them from the archive along with their
# registrations, and every real capture is then refused at load with "no config
# parser registered" — a message that reads like a corrupt file rather than a
# link-time decision. WHOLE_ARCHIVE keeps them.
#
# Referencing the parser functions directly would also keep them, and was tried
# first. It does not work here: the core vendors nlohmann 3.12 while RTNeural
# vendors 3.11, those versions use different inline namespaces, and a function
# taking a json argument therefore has a different mangled name on each side of
# the boundary.
add_library(voxera_nam_core STATIC
    ${neuralampmodelercore_SOURCE_DIR}/NAM/activations.cpp
    ${neuralampmodelercore_SOURCE_DIR}/NAM/container.cpp
    ${neuralampmodelercore_SOURCE_DIR}/NAM/conv1d.cpp
    ${neuralampmodelercore_SOURCE_DIR}/NAM/convnet.cpp
    ${neuralampmodelercore_SOURCE_DIR}/NAM/dsp.cpp
    ${neuralampmodelercore_SOURCE_DIR}/NAM/get_dsp.cpp
    ${neuralampmodelercore_SOURCE_DIR}/NAM/linear.cpp
    ${neuralampmodelercore_SOURCE_DIR}/NAM/lstm.cpp
    ${neuralampmodelercore_SOURCE_DIR}/NAM/ring_buffer.cpp
    ${neuralampmodelercore_SOURCE_DIR}/NAM/util.cpp
    ${neuralampmodelercore_SOURCE_DIR}/NAM/wavenet/model.cpp
    ${neuralampmodelercore_SOURCE_DIR}/NAM/wavenet/a2_fast.cpp
    ${neuralampmodelercore_SOURCE_DIR}/NAM/wavenet/slimmable.cpp
)
target_include_directories(voxera_nam_core PUBLIC
    ${neuralampmodelercore_SOURCE_DIR}
    ${neuralampmodelercore_SOURCE_DIR}/Dependencies/eigen
    ${neuralampmodelercore_SOURCE_DIR}/Dependencies/nlohmann
)
target_compile_definitions(voxera_nam_core PUBLIC NAM_SAMPLE_FLOAT=1)
set_target_properties(voxera_nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)


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
        # The core is double-precision by default. A vocal chain that is float
        # everywhere else would otherwise convert every sample twice per block
        # for no audible return.
        NAM_SAMPLE_FLOAT=1
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
        # Whole, and PUBLIC so the instruction reaches the link of the VST3 and
        # the tests rather than stopping at this static library.
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,voxera_nam_core>"
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
    # Offline renderer: the whole chain applied to a real recording.
    #
    # Built with the tests rather than beside them because it is the same kind
    # of thing — an instrument for finding out what the code does — and because
    # it must never end up inside anything that ships. Every measurement in this
    # project until now has been taken on synthetic material, which cannot
    # answer what the chain does to a voice.
    add_executable(VOXERA_Render Tools/Render.cpp)
    target_include_directories(VOXERA_Render PRIVATE Tests/headers ${rubberband_SOURCE_DIR})
    target_compile_definitions(VOXERA_Render PRIVATE JucePlugin_Name="VOXERA" NAM_SAMPLE_FLOAT=1)
    target_link_libraries(VOXERA_Render PRIVATE VOXERA RTNeural juce::juce_audio_utils juce::juce_dsp)

    add_executable(VOXERA_DetectorBenchmark Tests/detector_benchmark.cpp)
    target_include_directories(VOXERA_DetectorBenchmark PRIVATE Tests/headers ${rubberband_SOURCE_DIR})
    target_link_libraries(VOXERA_DetectorBenchmark PRIVATE VOXERA RTNeural juce::juce_audio_utils juce::juce_dsp)
    add_test(NAME detector_benchmark COMMAND VOXERA_DetectorBenchmark)
    foreach(testName IN ITEMS DSP Integration)
        string(TOLOWER ${testName} sourceName)
        add_executable(VOXERA_${testName}Tests Tests/${sourceName}_tests.cpp)
        target_include_directories(VOXERA_${testName}Tests PRIVATE Tests/headers ${rubberband_SOURCE_DIR})
        target_compile_definitions(VOXERA_${testName}Tests PRIVATE JucePlugin_Name="VOXERA" NAM_SAMPLE_FLOAT=1)
        target_link_libraries(VOXERA_${testName}Tests PRIVATE VOXERA RTNeural juce::juce_audio_utils juce::juce_dsp)
        add_test(NAME ${sourceName} COMMAND VOXERA_${testName}Tests "${CMAKE_CURRENT_BINARY_DIR}/test-previews")
    endforeach()
endif()

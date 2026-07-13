cmake_minimum_required(VERSION 3.24)

project(OpenFADFlipShift VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OPENFAD_ENABLE_LTO "Enable link-time optimisation for the plugin target" OFF)

include(FetchContent)

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_ENABLE_MODULE_SOURCE_GROUPS ON CACHE BOOL "" FORCE)

if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(OpenFADFlipShift
    COMPANY_NAME "UnpureBloom"
    PRODUCT_NAME "openFAD FlipShift"
    PLUGIN_MANUFACTURER_CODE UnPB
    PLUGIN_CODE Flps
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
)

target_sources(OpenFADFlipShift
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Parameters.cpp
        Source/DSP/FlipShiftEngine.cpp
        Source/DSP/SpectralModes.cpp
)

target_include_directories(OpenFADFlipShift
    PRIVATE
        Source
)

target_compile_definitions(OpenFADFlipShift
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(OpenFADFlipShift
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(OPENFAD_ENABLE_LTO)
    target_link_libraries(OpenFADFlipShift PUBLIC juce::juce_recommended_lto_flags)
endif()

juce_add_console_app(FlipShiftDSPTests
    PRODUCT_NAME "FlipShift DSP Tests"
)

target_sources(FlipShiftDSPTests
    PRIVATE
        Tests/DSPTests.cpp
        Source/DSP/FlipShiftEngine.cpp
        Source/DSP/SpectralModes.cpp
)

target_include_directories(FlipShiftDSPTests
    PRIVATE
        Source
)

target_compile_definitions(FlipShiftDSPTests
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(FlipShiftDSPTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

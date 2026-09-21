cmake_minimum_required(VERSION 3.24)

project(OpenFADFlipShift VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/utf-8)
endif()

option(OPENFAD_ENABLE_LTO "Enable link-time optimisation for the plugin target" OFF)
option(OPENFAD_BUILD_AUV3 "Build the AUv3 app extension and its standalone container on Apple platforms" ON)

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
        GIT_TAG 8.0.12
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if(NOT OPENFAD_BUILD_AUV3)
        message(FATAL_ERROR "iOS builds require OPENFAD_BUILD_AUV3=ON")
    endif()

    set(OPENFAD_PLUGIN_FORMATS AUv3 Standalone)
elseif(APPLE AND OPENFAD_BUILD_AUV3)
    set(OPENFAD_PLUGIN_FORMATS VST3 AU AUv3 Standalone)
elseif(APPLE)
    set(OPENFAD_PLUGIN_FORMATS VST3 AU)
else()
    set(OPENFAD_PLUGIN_FORMATS VST3)
endif()

if("AUv3" IN_LIST OPENFAD_PLUGIN_FORMATS AND NOT CMAKE_GENERATOR STREQUAL "Xcode")
    message(FATAL_ERROR "AUv3 builds require the Xcode CMake generator")
endif()

juce_add_plugin(OpenFADFlipShift
    COMPANY_NAME "UnpureBloom"
    PRODUCT_NAME "openFAD FlipShift"
    BUNDLE_ID "com.unpurebloom.openfad.flipshift"
    PLUGIN_MANUFACTURER_CODE UnPB
    PLUGIN_CODE Flps
    FORMATS ${OPENFAD_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEBVIEW2 TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "openFAD FlipShift requires audio input for standalone processing."
    TARGETED_DEVICE_FAMILY "1,2"
    AU_SANDBOX_SAFE TRUE
    VST3_CATEGORIES Fx
)

if(TARGET OpenFADFlipShift_AUv3)
    set_target_properties(OpenFADFlipShift OpenFADFlipShift_AUv3 PROPERTIES
        XCODE_ATTRIBUTE_APPLICATION_EXTENSION_API_ONLY YES
    )
endif()

juce_add_binary_data(OpenFADFlipShiftWebUI
    SOURCES
        WebUI/index.html
        WebUI/styles.css
        WebUI/app.js
        WebUI/i18n.js
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
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        $<$<PLATFORM_ID:Windows>:JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1>
)

target_link_libraries(OpenFADFlipShift
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        OpenFADFlipShiftWebUI
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

add_test(NAME FlipShiftDSPTests COMMAND FlipShiftDSPTests)

if(WIN32)
    juce_add_console_app(FlipShiftGUIIntegrationTests
        PRODUCT_NAME "FlipShift GUI Integration Tests"
        NEEDS_WEB_BROWSER TRUE
        NEEDS_WEBVIEW2 TRUE
    )

    target_sources(FlipShiftGUIIntegrationTests
        PRIVATE
            Tests/GUIIntegrationTests.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
            Source/Parameters.cpp
            Source/DSP/FlipShiftEngine.cpp
            Source/DSP/SpectralModes.cpp
    )

    target_include_directories(FlipShiftGUIIntegrationTests
        PRIVATE
            Source
    )

    target_compile_definitions(FlipShiftGUIIntegrationTests
        PRIVATE
            JucePlugin_Name="openFAD FlipShift"
            JUCE_MODAL_LOOPS_PERMITTED=1
            JUCE_WEB_BROWSER=1
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    )

    target_link_libraries(FlipShiftGUIIntegrationTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_extra
            OpenFADFlipShiftWebUI
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_test(NAME FlipShiftGUIIntegrationTests COMMAND FlipShiftGUIIntegrationTests)
    set_tests_properties(FlipShiftGUIIntegrationTests PROPERTIES
        RUN_SERIAL TRUE
        TIMEOUT 30
    )
endif()

cmake_minimum_required(VERSION 3.22)
project(SampleFetch VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.4
    GIT_SHALLOW ON)
FetchContent_Declare(SoundTouch
    GIT_REPOSITORY https://codeberg.org/soundtouch/soundtouch.git
    GIT_TAG 2.3.3
    GIT_SHALLOW ON)
set(SOUNDSTRETCH OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5) # SoundTouch asks for CMake 3.1, which CMake 4 refuses
FetchContent_MakeAvailable(JUCE SoundTouch)

juce_add_plugin(SampleFetch
    COMPANY_NAME "Merlinryr"
    PRODUCT_NAME "SampleFetch"
    PLUGIN_MANUFACTURER_CODE Mrln
    PLUGIN_CODE SpFt
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Tools
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SampleFetch PRIVATE
    Source/Processor.cpp
    Source/Editor.cpp
    Source/Fetcher.cpp
    Source/Analysis.cpp)

target_compile_definitions(SampleFetch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SampleFetch
    PRIVATE juce::juce_audio_utils juce::juce_dsp SoundTouch
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(SAMPLEFETCH_TESTS "Build the test app" OFF)
if(SAMPLEFETCH_TESTS)
    juce_add_console_app(SampleFetchTests)
    target_sources(SampleFetchTests PRIVATE tests/Tests.cpp Source/Fetcher.cpp Source/Analysis.cpp)
    target_compile_definitions(SampleFetchTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SampleFetchTests PRIVATE juce::juce_audio_formats juce::juce_dsp juce::juce_events SoundTouch
        juce::juce_recommended_config_flags)
endif()

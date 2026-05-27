cmake_minimum_required(VERSION 3.22)
project(UMBRA VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_gui_app(UMBRA
    PRODUCT_NAME        "UMBRA"
    VERSION             "1.0.0"
    BUNDLE_ID           "com.ampsim.umbra"
    ICON_BIG            ""
    MICROPHONE_PERMISSION_ENABLED   TRUE
    MICROPHONE_PERMISSION_TEXT      "UMBRA needs access to your audio input to process guitar."
)

juce_generate_juce_header(UMBRA)

target_sources(UMBRA PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/DSP/AmpProcessor.cpp
)

target_include_directories(UMBRA PRIVATE Source)

target_compile_definitions(UMBRA PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:UMBRA,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:UMBRA,JUCE_VERSION>"
)

target_link_libraries(UMBRA PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

cmake_minimum_required(VERSION 3.22)
project(UMBRA VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── RTNeural (neural amp captures) ────────────────────────────────────────
# Fetches RTNeural from GitHub the first time cmake runs.
# Re-run: cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Release
include(FetchContent)
FetchContent_Declare(RTNeural
    GIT_REPOSITORY https://github.com/jatinchowdhury18/RTNeural.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES "modules/json"   # nlohmann/json for model loading
)
set(RTNEURAL_XSIMD  OFF CACHE BOOL "" FORCE)
set(RTNEURAL_EIGEN  OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(RTNeural)
set(UMBRA_HAS_RTNEURAL TRUE)

# ── JUCE ──────────────────────────────────────────────────────────────────
add_subdirectory(JUCE)

# ── Shared sources (used by standalone + plugin) ───────────────────────────
set(UMBRA_SHARED_SOURCES
    Source/DSP/AmpProcessor.cpp
    Source/UI/EqPanel.cpp
    Source/UI/SharedComponents.cpp
)

# ── Standalone app ────────────────────────────────────────────────────────
juce_add_gui_app(UMBRA
    PRODUCT_NAME        "UMBRA"
    VERSION             "1.0.0"
    BUNDLE_ID           "com.ampsim.umbra"
    ICON_BIG            "${CMAKE_SOURCE_DIR}/docs/icon.png"
    MICROPHONE_PERMISSION_ENABLED  TRUE
    MICROPHONE_PERMISSION_TEXT     "UMBRA needs access to your audio input."
)

juce_generate_juce_header(UMBRA)

target_sources(UMBRA PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    ${UMBRA_SHARED_SOURCES}
)

target_include_directories(UMBRA PRIVATE Source Source/UI Source/DSP)

target_compile_definitions(UMBRA PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:UMBRA,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:UMBRA,JUCE_VERSION>"
    $<$<BOOL:${UMBRA_HAS_RTNEURAL}>:UMBRA_HAS_RTNEURAL=1>
)

target_link_libraries(UMBRA PRIVATE
    RTNeural
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

# ── VST3 / AU plugin ──────────────────────────────────────────────────────
juce_add_plugin(UMBRA_Plugin
    PRODUCT_NAME        "UMBRA"
    VERSION             "1.0.0"
    BUNDLE_ID           "com.ampsim.umbra.plugin"
    FORMATS             AU VST3
    IS_SYNTH            FALSE
    NEEDS_MIDI_INPUT    FALSE
    NEEDS_MIDI_OUTPUT   FALSE
    IS_MIDI_EFFECT      FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE  Ampl
    PLUGIN_CODE               Umbr
    VST3_CATEGORIES           "Fx|Distortion"
    AU_MAIN_TYPE              kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "UMBRA needs access to your audio input."
)

juce_generate_juce_header(UMBRA_Plugin)

target_sources(UMBRA_Plugin PRIVATE
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.cpp
    ${UMBRA_SHARED_SOURCES}
)

target_include_directories(UMBRA_Plugin PRIVATE Source Source/UI Source/DSP)

target_compile_definitions(UMBRA_Plugin PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    $<$<BOOL:${UMBRA_HAS_RTNEURAL}>:UMBRA_HAS_RTNEURAL=1>
)

target_link_libraries(UMBRA_Plugin PRIVATE
    RTNeural
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

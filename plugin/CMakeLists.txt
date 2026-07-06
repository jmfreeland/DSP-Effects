include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.14
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(LexiconHallPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxh1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Hall"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconHallPlugin PRIVATE
    source/PluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
)

target_include_directories(LexiconHallPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconHallPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconHallPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconPlatePlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxp1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Plate"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconPlatePlugin PRIVATE
    source/PlatePluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
)

target_include_directories(LexiconPlatePlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconPlatePlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconPlatePlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconChamberPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxc1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Chamber"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconChamberPlugin PRIVATE
    source/ChamberPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
)

target_include_directories(LexiconChamberPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconChamberPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconChamberPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconInfinitePlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxi1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Infinite"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconInfinitePlugin PRIVATE
    source/InfinitePluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
)

target_include_directories(LexiconInfinitePlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconInfinitePlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconInfinitePlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconInversePlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxv1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Inverse"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconInversePlugin PRIVATE
    source/InversePluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
)

target_include_directories(LexiconInversePlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconInversePlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconInversePlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideDiatonicShiftPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evd1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Diatonic Shift"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideDiatonicShiftPlugin PRIVATE
    source/DiatonicShiftPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
)

target_include_directories(EventideDiatonicShiftPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideDiatonicShiftPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideDiatonicShiftPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

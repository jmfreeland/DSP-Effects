include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.14
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# CLAP support for the Loom Browser plugin only (see that target's own
# clap_juce_extensions_plugin() call near the end of this file) - JUCE
# has no native CLAP format, so this pulls in the unofficial
# free-audio/clap-juce-extensions wrapper. Not shallow: it needs its own
# clap/clap-helpers submodules recursively cloned, which FetchContent
# handles automatically for a full (non-shallow) clone.
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG 16e9d4ca7b1e86c76e04584b2c08e85a764bcda8
)
FetchContent_MakeAvailable(clap-juce-extensions)

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
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
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
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
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
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
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
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
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
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
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

juce_add_plugin(LexiconGlideHallPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxg1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Glide>Hall"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconGlideHallPlugin PRIVATE
    source/GlideHallPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LexiconGlideHallPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconGlideHallPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconGlideHallPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconChorusRvbPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxo1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Chorus+Rvb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconChorusRvbPlugin PRIVATE
    source/ChorusRvbPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LexiconChorusRvbPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconChorusRvbPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconChorusRvbPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconMBandRvbPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxm1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon M-Band+Rvb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconMBandRvbPlugin PRIVATE
    source/MBandRvbPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LexiconMBandRvbPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconMBandRvbPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconMBandRvbPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconRes1PlatePlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxr1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Res1>Plate"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconRes1PlatePlugin PRIVATE
    source/Res1PlatePluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LexiconRes1PlatePlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconRes1PlatePlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconRes1PlatePlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconRes2PlatePlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxr2
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Res2>Plate"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconRes2PlatePlugin PRIVATE
    source/Res2PlatePluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LexiconRes2PlatePlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconRes2PlatePlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconRes2PlatePlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconQuadHallPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxq1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Quad>Hall"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconQuadHallPlugin PRIVATE
    source/QuadHallPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LexiconQuadHallPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconQuadHallPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconQuadHallPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconDualChmbPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxd1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Dual-Chmb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconDualChmbPlugin PRIVATE
    source/DualChmbPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LexiconDualChmbPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconDualChmbPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconDualChmbPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconDualPltPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxd2
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Dual-Plt"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconDualPltPlugin PRIVATE
    source/DualPltPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LexiconDualPltPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconDualPltPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconDualPltPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconDualInvPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxd3
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Dual-Inv"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconDualInvPlugin PRIVATE
    source/DualInvPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LexiconDualInvPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconDualInvPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconDualInvPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconStereoChmbPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxs1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Stereo-Chmb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconStereoChmbPlugin PRIVATE
    source/StereoChmbPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LexiconStereoChmbPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconStereoChmbPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconStereoChmbPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconVSOChmbPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxs2
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon VSO-Chmb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconVSOChmbPlugin PRIVATE
    source/VSOChmbPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LexiconVSOChmbPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconVSOChmbPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconVSOChmbPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(LexiconPitchCorrectPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lxc2
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Lexicon Pitch Correct"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LexiconPitchCorrectPlugin PRIVATE
    source/PitchCorrectPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LexiconPitchCorrectPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(LexiconPitchCorrectPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LexiconPitchCorrectPlugin
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
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
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

juce_add_plugin(EventideLayeredShiftPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evd2
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Layered Shift"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideLayeredShiftPlugin PRIVATE
    source/LayeredShiftPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideLayeredShiftPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideLayeredShiftPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideLayeredShiftPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideDualShiftPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evd3
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Dual Shift"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideDualShiftPlugin PRIVATE
    source/DualShiftPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideDualShiftPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideDualShiftPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideDualShiftPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideStereoShiftPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evd4
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Stereo Shift"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideStereoShiftPlugin PRIVATE
    source/StereoShiftPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideStereoShiftPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideStereoShiftPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideStereoShiftPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideReverseShiftPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evd5
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Reverse Shift"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideReverseShiftPlugin PRIVATE
    source/ReverseShiftPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideReverseShiftPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideReverseShiftPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideReverseShiftPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideSweptCombsPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evd6
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Swept Combs"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideSweptCombsPlugin PRIVATE
    source/SweptCombsPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideSweptCombsPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideSweptCombsPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideSweptCombsPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideSweptReverbPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evd7
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Swept Reverb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideSweptReverbPlugin PRIVATE
    source/SweptReverbPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideSweptReverbPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideSweptReverbPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideSweptReverbPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideReverbFactoryPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evd8
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Reverb Factory"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideReverbFactoryPlugin PRIVATE
    source/ReverbFactoryPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideReverbFactoryPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideReverbFactoryPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideReverbFactoryPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideUltraTapPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evd9
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Ultra-Tap"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideUltraTapPlugin PRIVATE
    source/UltraTapPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideUltraTapPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideUltraTapPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideUltraTapPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideLongDigiplexPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evda
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Long Digiplex"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideLongDigiplexPlugin PRIVATE
    source/LongDigiplexPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideLongDigiplexPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideLongDigiplexPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideLongDigiplexPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideDualDigiplexPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evdb
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Dual Digiplex"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideDualDigiplexPlugin PRIVATE
    source/DualDigiplexPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideDualDigiplexPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideDualDigiplexPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideDualDigiplexPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventidePatchFactoryPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evdc
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Patch Factory"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventidePatchFactoryPlugin PRIVATE
    source/PatchFactoryPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventidePatchFactoryPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventidePatchFactoryPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventidePatchFactoryPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideStutterPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evdd
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Stutter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideStutterPlugin PRIVATE
    source/StutterPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideStutterPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideStutterPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideStutterPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideTimesqueezePlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evde
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Timesqueeze"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideTimesqueezePlugin PRIVATE
    source/TimesqueezePluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideTimesqueezePlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideTimesqueezePlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideTimesqueezePlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideDenseRoomPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evdf
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Dense Room"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideDenseRoomPlugin PRIVATE
    source/DenseRoomPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideDenseRoomPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideDenseRoomPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideDenseRoomPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideVocoderPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evdg
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Vocoder"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideVocoderPlugin PRIVATE
    source/VocoderPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideVocoderPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideVocoderPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideVocoderPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideMultiShiftPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evdh
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Multi-Shift"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideMultiShiftPlugin PRIVATE
    source/MultiShiftPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideMultiShiftPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideMultiShiftPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideMultiShiftPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideBandDelayPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evdi
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Band Delay"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideBandDelayPlugin PRIVATE
    source/BandDelayPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideBandDelayPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideBandDelayPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideBandDelayPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideStringModellerPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evdj
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide String Modeller"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideStringModellerPlugin PRIVATE
    source/StringModellerPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideStringModellerPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideStringModellerPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideStringModellerPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventidePhaserPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evdk
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Phaser"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventidePhaserPlugin PRIVATE
    source/PhaserPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventidePhaserPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventidePhaserPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventidePhaserPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideStudioSamplerPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evdl
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Studio Sampler"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideStudioSamplerPlugin PRIVATE
    source/StudioSamplerPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideStudioSamplerPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideStudioSamplerPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideStudioSamplerPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideModFactoryOnePlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evdm
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Mod Factory One"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideModFactoryOnePlugin PRIVATE
    source/ModFactoryOnePluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideModFactoryOnePlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideModFactoryOnePlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideModFactoryOnePlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_add_plugin(EventideModFactoryTwoPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Evdn
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Eventide Mod Factory Two"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(EventideModFactoryTwoPlugin PRIVATE
    source/ModFactoryTwoPluginProcessor.cpp
    source/ArchitectureView.cpp
    source/LoomPluginEditor.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(EventideModFactoryTwoPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_compile_definitions(EventideModFactoryTwoPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EventideModFactoryTwoPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# The Loom browser plugin: a single instance that can switch between any
# of EngineRegistry's registered algorithms without the host seeing its
# parameter list change (see source/browser/LoomBrowserPluginProcessor.h).
# Proof-of-concept slice covering the five Lexicon reverb cores; adding
# an algorithm means adding its adapter and one line in EngineRegistry.h.
juce_add_plugin(LoomBrowserPlugin
    COMPANY_NAME "Loom"
    PLUGIN_MANUFACTURER_CODE Loom
    PLUGIN_CODE Lmbr
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Loom - Browser"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(LoomBrowserPlugin PRIVATE
    source/browser/LoomBrowserPluginProcessor.cpp
    source/browser/LoomBrowserPluginEditor.cpp
    source/browser/pcm80/Pcm80Archive.cpp
    source/ArchitectureView.cpp
    source/LoomParametersPanel.cpp
    source/LoomTheme.cpp
)

target_include_directories(LoomBrowserPlugin PRIVATE
    ${CMAKE_SOURCE_DIR}/dsp/include
    source
)

target_compile_definitions(LoomBrowserPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LoomBrowserPlugin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# CLAP build of the browser plugin only - see the FetchContent_Declare
# for clap-juce-extensions near the top of this file. Produces an
# additional LoomBrowserPlugin_CLAP target alongside the VST3/AU/
# Standalone ones already declared above.
clap_juce_extensions_plugin(TARGET LoomBrowserPlugin
    CLAP_ID "com.loom.browser"
    CLAP_FEATURES audio-effect multi-effects reverb delay chorus
)

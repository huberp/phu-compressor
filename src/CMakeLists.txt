# Semicolon-separated JUCE formats; set by the "formats-*" presets in CMakePresets.json.
if(APPLE)
    set(_phu_default_formats "VST3;AU")
else()
    set(_phu_default_formats "VST3")
endif()
set(PHU_PLUGIN_FORMATS "${_phu_default_formats}" CACHE STRING "JUCE plugin formats to build (e.g. \"VST3;AU\")")

juce_add_plugin(phu-compressor
    PLUGIN_MANUFACTURER_CODE Phub
    PLUGIN_CODE Pcmp
    FORMATS ${PHU_PLUGIN_FORMATS}
    PRODUCT_NAME "PHU COMPRESSOR"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(phu-compressor PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    OttCompressor.h
    PluginConstants.h
    VolumeDetector.h
    CompressorStage.h
    CompressorDisplay.cpp
    CompressorDisplay.h
)

target_link_libraries(phu-compressor
    PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
        CompressorLib
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Small shared helpers every reverb-core adapter uses to build its own
// namespaced parameter layout - the same floatParam()/voiceParamId()
// pair every single-algorithm PluginProcessor.cpp already has, plus
// prefixedId() for namespacing (see EngineAdapter.h's own doc comment
// on why every adapter's ids carry its id() prefix).
namespace loom::browser
{
inline juce::String prefixedId(const char* prefix, const char* id)
{
    return juce::String(prefix) + "_" + id;
}

// One voice's four parameter ids, e.g. "concertHall_voice0Delay".
inline juce::String voiceParamId(const char* prefix, int index, const char* suffix)
{
    return juce::String(prefix) + "_voice" + juce::String(index) + suffix;
}

inline std::unique_ptr<juce::AudioParameterFloat> floatParam(const juce::String& id,
                                                              const juce::String& name, float min,
                                                              float max, float defaultValue,
                                                              const char* label = nullptr,
                                                              float skew = 1.0f)
{
    juce::AudioParameterFloatAttributes attributes;
    if (label != nullptr)
    {
        attributes = attributes.withLabel(label);
    }
    auto interval = (max - min) / 10000.0f;
    return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ id, 1 }, name, juce::NormalisableRange<float>(min, max, interval, skew),
      defaultValue, attributes);
}

inline float paramValue(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    return apvts.getRawParameterValue(id)->load();
}
}

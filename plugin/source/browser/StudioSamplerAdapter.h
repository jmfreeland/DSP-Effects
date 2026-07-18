#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/StudioSamplerAlgorithm.h"
#include "dsp/schema/StudioSamplerSchema.h"

#include <array>
#include <span>

// Adapts dsp::graphs::StudioSamplerAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// StudioSamplerPluginProcessor.cpp's own layout/processBlock, namespaced
// under "studioSampler" (Algorithm 120 - 121 is the same program with
// Record Mode default flipped to stereo, not a distinct algorithm, so
// only one adapter is registered). Two independent per-channel
// SamplerVoices, each with Record/Stop/Play momentary triggers standing
// in for the missing MIDI note-on/interactive point-editing workflow.
namespace loom::browser
{
namespace studiosampler_detail
{
using dsp::SamplerVoice;

inline const juce::StringArray kShiftModeNames = { "Generic Sampler", "Constant Length" };
inline const juce::StringArray kTriggerModeNames = { "Off", "Audio" };

inline SamplerVoice::ShiftMode shiftModeFromIndex(int index)
{
    return index == 1 ? SamplerVoice::ShiftMode::kConstantLength : SamplerVoice::ShiftMode::kGenericSampler;
}

inline SamplerVoice::TriggerMode triggerModeFromIndex(int index)
{
    return index == 1 ? SamplerVoice::TriggerMode::kAudio : SamplerVoice::TriggerMode::kOff;
}
}

class StudioSamplerAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "studioSampler"; }
    const char* displayName() const override { return "Eventide Studio Sampler"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace studiosampler_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("studioSampler", suffix); };

        for (int i = 0; i < 2; ++i)
        {
            auto n = juce::String(i + 1);
            auto chId = [&](const char* base) { return pid((juce::String(base) + n).toRawUTF8()); };

            params.push_back(floatParam(chId("pitch"), "Pitch " + n, -3600.0f, 3600.0f, 0.0f, "ct"));
            params.push_back(floatParam(chId("time"), "Time " + n, 0.0f, 800.0f, 100.0f, "%"));
            params.push_back(floatParam(chId("attack"), "Attack " + n, 0.001f, 1.0f, 0.005f, "s"));
            params.push_back(floatParam(chId("release"), "Release " + n, 0.001f, 1.0f, 0.005f, "s"));
            params.push_back(floatParam(chId("start"), "Start " + n, 0.0f, 1.0f, 0.0f));
            params.push_back(floatParam(chId("end"), "End " + n, 0.0f, 1.0f, 1.0f));
            params.push_back(
              std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ chId("loop"), 1 }, "Loop " + n, false));
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
              juce::ParameterID{ chId("shiftMode"), 1 }, "Shift Mode " + n, kShiftModeNames, 0));
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
              juce::ParameterID{ chId("triggerMode"), 1 }, "Trigger Mode " + n, kTriggerModeNames, 0));
            params.push_back(floatParam(chId("threshold"), "Threshold " + n, 0.0f, 1.0f, 0.1f));
            params.push_back(
              std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ chId("record"), 1 }, "Record " + n, false));
            params.push_back(
              std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ chId("stop"), 1 }, "Stop " + n, false));
            params.push_back(
              std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ chId("play"), 1 }, "Play " + n, false));
        }
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 100.0f, 100.0f, "%"));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::studioSamplerSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::StudioSamplerAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using namespace studiosampler_detail;
        auto pid = [](const char* suffix) { return prefixedId("studioSampler", suffix); };

        for (int channel = 0; channel < 2; ++channel)
        {
            auto n = juce::String(channel + 1);
            auto chId = [&](const char* base) { return pid((juce::String(base) + n).toRawUTF8()); };
            auto v = [&](const char* base) { return paramValue(apvts, chId(base)); };

            engine_.setPitchCents(channel, v("pitch"));
            engine_.setTimePercent(channel, v("time"));
            engine_.setAttackSeconds(channel, v("attack"));
            engine_.setReleaseSeconds(channel, v("release"));
            engine_.setStartFraction(channel, v("start"));
            engine_.setEndFraction(channel, v("end"));
            engine_.setLoop(channel, v("loop") >= 0.5f);
            engine_.setShiftMode(channel, shiftModeFromIndex(static_cast<int>(v("shiftMode"))));
            engine_.setTriggerMode(channel, triggerModeFromIndex(static_cast<int>(v("triggerMode"))));
            engine_.setThreshold(channel, v("threshold"));

            auto* record = apvts.getParameter(chId("record"));
            if (record->getValue() >= 0.5f)
            {
                engine_.record(channel);
                record->setValueNotifyingHost(0.0f);
            }
            auto* stop = apvts.getParameter(chId("stop"));
            if (stop->getValue() >= 0.5f)
            {
                engine_.stop(channel);
                stop->setValueNotifyingHost(0.0f);
            }
            auto* play = apvts.getParameter(chId("play"));
            if (play->getValue() >= 0.5f)
            {
                engine_.play(channel);
                play->setValueNotifyingHost(0.0f);
            }
        }
        engine_.setMix(paramValue(apvts, pid("mix")));

        engine_.process(left, right);
    }

  private:
    dsp::graphs::StudioSamplerAlgorithm engine_;
};
}

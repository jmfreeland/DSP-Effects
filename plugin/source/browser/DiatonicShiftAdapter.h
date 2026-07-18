#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/DiatonicShiftAlgorithm.h"
#include "dsp/schema/DiatonicShiftSchema.h"

#include <span>

// Adapts dsp::graphs::DiatonicShiftAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// DiatonicShiftPluginProcessor.cpp's own layout/processBlock,
// namespaced under "diatonicShift". The first of the 24 Eventide H3000
// algorithms.
namespace loom::browser
{
namespace diatonicshift_detail
{
inline const juce::StringArray kKeyNames = { "C",  "C#", "D",  "D#", "E",  "F",
                                              "F#", "G",  "G#", "A",  "A#", "B" };
inline const juce::StringArray kScaleNames = { "Major", "Natural Minor", "Harmonic Minor", "Dorian",
                                                "Mixolydian", "Lydian" };
inline const juce::StringArray kVoiceNames = { "Octave Down",       "7th Down",         "6th Down",
                                                "5th Down",          "4th Down",         "3rd Down",
                                                "2nd Down",          "2nd Up",           "3rd Up",
                                                "4th Up",            "5th Up",           "6th Up",
                                                "7th Up",             "Octave Up",
                                                "Low Tonic Pedal",    "High Tonic Pedal",
                                                "Low Dominant Pedal", "High Dominant Pedal" };

inline dsp::Scale scaleFromIndex(int index)
{
    switch (index)
    {
        case 1:
            return dsp::Scale::kNaturalMinor;
        case 2:
            return dsp::Scale::kHarmonicMinor;
        case 3:
            return dsp::Scale::kDorian;
        case 4:
            return dsp::Scale::kMixolydian;
        case 5:
            return dsp::Scale::kLydian;
        case 0:
        default:
            return dsp::Scale::kMajor;
    }
}
}

class DiatonicShiftAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "diatonicShift"; }
    const char* displayName() const override { return "Eventide Diatonic Shift"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace diatonicshift_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("diatonicShift", suffix); };

        params.push_back(floatParam(pid("grain"), "Grain", 0.01f, 0.3f, 0.07f, "s"));
        params.push_back(floatParam(pid("delay"), "Delay", 0.0f, 1.0f, 0.05f, "s"));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("key"), 1 }, "Key", kKeyNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("scale"), 1 }, "Scale", kScaleNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("leftVoice"), 1 }, "Left Voice", kVoiceNames, 8));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("rightVoice"), 1 }, "Right Voice", kVoiceNames, 10));
        params.push_back(floatParam(pid("leftFeedback"), "Left Feedback", 0.0f, 0.99f, 0.0f));
        params.push_back(floatParam(pid("rightFeedback"), "Right Feedback", 0.0f, 0.99f, 0.0f));
        params.push_back(floatParam(pid("leftMix"), "Left Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("rightMix"), "Right Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("tune"), "Tune", -50.0f, 50.0f, 0.0f, "cents"));
        params.push_back(floatParam(pid("lowNoteHz"), "Low Note", 30.0f, 400.0f, 80.0f, "Hz"));
        params.push_back(floatParam(pid("highNoteHz"), "High Note", 200.0f, 1500.0f, 800.0f, "Hz"));
        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::diatonicShiftSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::DiatonicShiftAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using namespace diatonicshift_detail;
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("diatonicShift", suffix)); };

        engine_.setGrainSeconds(v("grain"));
        engine_.setDelaySeconds(v("delay"));
        engine_.setKey(static_cast<int>(v("key")));
        engine_.setScale(scaleFromIndex(static_cast<int>(v("scale"))));
        engine_.setLeftVoice(static_cast<dsp::HarmonicInterval>(static_cast<int>(v("leftVoice"))));
        engine_.setRightVoice(static_cast<dsp::HarmonicInterval>(static_cast<int>(v("rightVoice"))));
        engine_.setLeftFeedback(v("leftFeedback"));
        engine_.setRightFeedback(v("rightFeedback"));
        engine_.setLeftMix(v("leftMix"));
        engine_.setRightMix(v("rightMix"));
        engine_.setTuneCents(v("tune"));
        engine_.setFrequencyRange(v("lowNoteHz"), v("highNoteHz"));
        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));

        engine_.process(left, right);
    }

  private:
    dsp::graphs::DiatonicShiftAlgorithm engine_;
};
}

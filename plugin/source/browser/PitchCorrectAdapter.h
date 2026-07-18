#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/PitchCorrectAlgorithm.h"
#include "dsp/schema/PitchCorrectSchema.h"

#include <span>

// Adapts dsp::graphs::PitchCorrectAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// PitchCorrectPluginProcessor.cpp's own layout/processBlock, namespaced
// under "pitchCorrect". The last of the 17 Lexicon PCM81 algorithms.
namespace loom::browser
{
namespace pitchcorrect_detail
{
inline const juce::StringArray kTrackingNames { "Fastest", "Fast", "Moderate", "Slow", "Hold" };
}

class PitchCorrectAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "pitchCorrect"; }
    const char* displayName() const override { return "Lexicon Pitch Correct"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace pitchcorrect_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("pitchCorrect", suffix); };

        params.push_back(floatParam(pid("inLevelLeft"), "In Lvl L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Lvl R", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("delay"), "Delay", 0.0f, 0.1f, 0.01f, "s"));
        params.push_back(floatParam(pid("lowPitch"), "Low Pitch", 30.0f, 1000.0f, 80.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("highPitch"), "High Pitch", 30.0f, 2000.0f, 800.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("tuning"), "Tuning", 410.0f, 470.0f, 440.0f, "Hz"));
        params.push_back(floatParam(pid("correction"), "Correction", 0.0f, 1.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("tracking"), 1 }, "Tracking", kTrackingNames, 0));
        params.push_back(floatParam(pid("grain"), "Grain", 0.001f, 0.05f, 0.008f, "s"));
        params.push_back(floatParam(pid("shiftCents"), "Shift Cents", -100.0f, 100.0f, 0.0f));
        params.push_back(floatParam(pid("shiftSemitones"), "Shift Semitones", -24.0f, 24.0f, 0.0f));

        params.push_back(floatParam(pid("decay"), "Decay", 0.3f, 8.0f, 2.0f, "s", 0.5f));
        params.push_back(floatParam(pid("lowRatio"), "Low Ratio", 0.2f, 2.0f, 1.0f));
        params.push_back(floatParam(pid("crossover"), "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("damping"), "Damping", 0.0f, 1.0f, 0.4f));
        params.push_back(floatParam(pid("diffusion"), "Diffusion", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("size"), "Size", 0.0f, 1.0f, 0.5f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("link"), 1 }, "Link", false));
        params.push_back(floatParam(pid("shape"), "Shape", 0.0f, 1.0f, 0.3f));
        params.push_back(floatParam(pid("spread"), "Spread", 0.0f, 1.0f, 0.4f));
        params.push_back(floatParam(pid("rvbIn"), "Rvb In", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("rvbOut"), "Rvb Out", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("preDelay"), "Pre Delay", 0.0f, 0.93f, 0.0f, "s"));
        params.push_back(floatParam(pid("earlyReflectionLevelLeft"), "Early Reflections L", 0.0f, 1.0f, 0.2f));
        params.push_back(floatParam(pid("earlyReflectionLevelRight"), "Early Reflections R", 0.0f, 1.0f, 0.2f));
        params.push_back(
          floatParam(pid("earlyReflectionDelayLeft"), "Early Reflection Delay L", 0.0f, 1.2f, 0.03f, "s"));
        params.push_back(
          floatParam(pid("earlyReflectionDelayRight"), "Early Reflection Delay R", 0.0f, 1.2f, 0.03f, "s"));
        params.push_back(floatParam(pid("spin"), "Spin", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("ekoFeedbackLeft"), "Eko Fbk L", 0.0f, 0.95f, 0.2f));
        params.push_back(floatParam(pid("ekoFeedbackRight"), "Eko Fbk R", 0.0f, 0.95f, 0.2f));
        params.push_back(floatParam(pid("ekoDelayLeft"), "Eko Dly L", 0.0f, 1.2f, 0.06f, "s"));
        params.push_back(floatParam(pid("ekoDelayRight"), "Eko Dly R", 0.0f, 1.2f, 0.07f, "s"));

        params.push_back(floatParam(pid("fxMix"), "FX Mix", 0.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("fxWidth"), "FX Width", -360.0f, 360.0f, 0.0f, "deg"));
        params.push_back(floatParam(pid("hiCut"), "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("fxAdjust"), "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 1.0f));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::pitchCorrectSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::PitchCorrectAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("pitchCorrect", suffix)); };

        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));
        engine_.setDelaySeconds(v("delay"));
        engine_.setPitchRange(v("lowPitch"), v("highPitch"));
        engine_.setTuning(v("tuning"));
        engine_.setCorrection(v("correction"));
        engine_.setTracking(
          static_cast<dsp::graphs::PitchCorrectAlgorithm::Tracking>(static_cast<int>(v("tracking"))));
        engine_.setGrainSeconds(v("grain"));
        engine_.setShiftCents(v("shiftCents"));
        engine_.setShiftSemitones(static_cast<int>(v("shiftSemitones")));

        engine_.setDecaySeconds(v("decay"));
        engine_.setLowRatio(v("lowRatio"));
        engine_.setCrossoverFrequency(v("crossover"));
        engine_.setDamping(v("damping"));
        engine_.setDiffusion(v("diffusion"));
        engine_.setSize(v("size"));
        engine_.setLink(v("link") >= 0.5f);
        engine_.setShape(v("shape"));
        engine_.setSpread(v("spread"));
        engine_.setRvbIn(v("rvbIn"));
        engine_.setRvbOut(v("rvbOut"));
        engine_.setPreDelaySeconds(v("preDelay"));
        engine_.setEarlyReflectionLevel(v("earlyReflectionLevelLeft"), v("earlyReflectionLevelRight"));
        engine_.setEarlyReflectionDelaySeconds(v("earlyReflectionDelayLeft"), v("earlyReflectionDelayRight"));
        engine_.setSpin(v("spin"));
        engine_.setEkoFeedback(v("ekoFeedbackLeft"), v("ekoFeedbackRight"));
        engine_.setEkoDelaySeconds(v("ekoDelayLeft"), v("ekoDelayRight"));

        engine_.setFxMix(v("fxMix"));
        engine_.setFxWidth(v("fxWidth"));
        engine_.setHiCut(v("hiCut"));
        engine_.setFxAdjustDb(v("fxAdjust"));
        engine_.setMix(v("mix"));

        engine_.process(left, right);
    }

  private:
    dsp::graphs::PitchCorrectAlgorithm engine_;
};
}

#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/PhaserAlgorithm.h"
#include "dsp/schema/PhaserSchema.h"

#include <span>

// Adapts dsp::graphs::PhaserAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// PhaserPluginProcessor.cpp's own layout/processBlock, namespaced under
// "phaser" (Algorithm 119). Exposes all three sweep modes (LFO/Envelope/
// ADSR), the ADSR's own rates/thresholds, the Envelope Channel sidechain
// option, and a manual ADSR trigger button standing in for the manual's
// MIDI-only ADSR Trigger.
namespace loom::browser
{
namespace phaser_detail
{
using dsp::algorithms::Phaser;

inline const juce::StringArray kSweepModeNames = { "LFO", "Envelope", "ADSR" };

inline Phaser::SweepMode sweepModeFromIndex(int index)
{
    switch (index)
    {
        case 1:
            return Phaser::SweepMode::kEnvelope;
        case 2:
            return Phaser::SweepMode::kAdsr;
        case 0:
        default:
            return Phaser::SweepMode::kLfo;
    }
}
}

class PhaserAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "phaser"; }
    const char* displayName() const override { return "Eventide Phaser"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace phaser_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("phaser", suffix); };

        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 100.0f, 50.0f, "%"));
        params.push_back(floatParam(pid("feedback"), "Feedback", -100.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("sweepRate"), "Sweep Rate", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("envelopeDecayRate"), "Envelope Decay Rate", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("adsrRateScaler"), "ADSR Rate Scaler", 0.0f, 100.0f, 100.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("sweepMode"), 1 }, "Sweep Mode", kSweepModeNames, 0));
        params.push_back(floatParam(pid("sweepBottom"), "Sweep Bottom", 0.0f, 100.0f, 20.0f));
        params.push_back(floatParam(pid("sweepTop"), "Sweep Top", 0.0f, 100.0f, 60.0f));

        params.push_back(floatParam(pid("adsrAttackRate"), "ADSR Attack Rate", 0.0f, 100.0f, 60.0f));
        params.push_back(floatParam(pid("adsrDecayRate"), "ADSR Decay Rate", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("adsrSustainLevel"), "ADSR Sustain Level", 0.0f, 100.0f, 60.0f));
        params.push_back(floatParam(pid("adsrReleaseRate"), "ADSR Release Rate", 0.0f, 100.0f, 40.0f));
        params.push_back(floatParam(pid("adsrAttackThreshold"), "ADSR Attack Threshold", 0.0f, 100.0f, 30.0f));
        params.push_back(floatParam(pid("adsrReleaseThreshold"), "ADSR Release Threshold", 0.0f, 100.0f, 10.0f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ pid("envelopeChannel"), 1 }, "Envelope Channel = Right (sidechain)", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ pid("envelopeDecayShape"), 1 }, "Envelope Decay Exponential", true));

        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("trigger"), 1 }, "ADSR Trigger", false));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::phaserSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::PhaserAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using namespace phaser_detail;
        auto pid = [](const char* suffix) { return prefixedId("phaser", suffix); };
        auto v = [&](const char* suffix) { return paramValue(apvts, pid(suffix)); };

        engine_.setMix(v("mix"));
        engine_.setFeedback(v("feedback"));
        engine_.setSweepRate(v("sweepRate"));
        engine_.setEnvelopeDecayRate(v("envelopeDecayRate"));
        engine_.setAdsrRateScaler(v("adsrRateScaler"));
        engine_.setSweepMode(sweepModeFromIndex(static_cast<int>(v("sweepMode"))));
        engine_.setSweepBottom(v("sweepBottom"));
        engine_.setSweepTop(v("sweepTop"));
        engine_.setAdsrAttackRate(v("adsrAttackRate"));
        engine_.setAdsrDecayRate(v("adsrDecayRate"));
        engine_.setAdsrSustainLevel(v("adsrSustainLevel"));
        engine_.setAdsrReleaseRate(v("adsrReleaseRate"));
        engine_.setAdsrAttackThreshold(v("adsrAttackThreshold"));
        engine_.setAdsrReleaseThreshold(v("adsrReleaseThreshold"));
        engine_.setEnvelopeChannel(v("envelopeChannel") >= 0.5f);
        engine_.setEnvelopeDecayShapeExponential(v("envelopeDecayShape") >= 0.5f);

        auto* trigger = apvts.getParameter(pid("trigger"));
        if (trigger->getValue() >= 0.5f)
        {
            engine_.trigger();
            trigger->setValueNotifyingHost(0.0f);
        }

        engine_.process(left, right);
    }

  private:
    dsp::graphs::PhaserAlgorithm engine_;
};
}

#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/PatchFactoryAlgorithm.h"
#include "dsp/schema/PatchFactorySchema.h"

#include <span>

// Adapts dsp::graphs::PatchFactoryAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// PatchFactoryPluginProcessor.cpp's own layout/processBlock, namespaced
// under "patchFactory" (Algorithm 111). A genuine modular patch-bay: 13
// destination choices over 16 sources, plus every basic-element
// parameter, since the patch matrix is the whole point of this
// algorithm.
namespace loom::browser
{
namespace patchfactory_detail
{
using dsp::algorithms::PatchFactory;

// Order matches dsp::algorithms::PatchFactory::Source exactly, so the
// AudioParameterChoice index can be cast straight to the enum.
inline const juce::StringArray kSourceNames = { "Left Input", "Sum 1",       "Sum 2",       "Delay 1",
                                                 "Delay 2",    "Scaler 1",    "Scaler 2",    "Lowpass 1",
                                                 "Bandpass 1", "Highpass 1",  "Lowpass 2",   "Bandpass 2",
                                                 "Highpass 2", "Pitch Shift", "Noise Gen",   "Null Input" };
}

class PatchFactoryAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "patchFactory"; }
    const char* displayName() const override { return "Eventide Patch Factory"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace patchfactory_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("patchFactory", suffix); };
        auto sourceParam = [&](const char* suffix, const juce::String& name, PatchFactory::Source defaultSource) {
            return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ pid(suffix), 1 }, name,
                                                                  kSourceNames, static_cast<int>(defaultSource));
        };

        params.push_back(floatParam(pid("cutoff1"), "Cutoff 1", 0.0f, 7000.0f, 1000.0f, "Hz"));
        params.push_back(floatParam(pid("cutoff2"), "Cutoff 2", 0.0f, 7000.0f, 1000.0f, "Hz"));
        params.push_back(floatParam(pid("q1"), "Q 1", 0.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("q2"), "Q 2", 0.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("delay1"), "Delay 1", 0.0f, 0.5f, 0.15f, "s"));
        params.push_back(floatParam(pid("delay2"), "Delay 2", 0.0f, 0.5f, 0.25f, "s"));
        params.push_back(floatParam(pid("scale1"), "Scale 1", -100.0f, 100.0f, 30.0f, "%"));
        params.push_back(floatParam(pid("scale2"), "Scale 2", -100.0f, 100.0f, 70.0f, "%"));
        params.push_back(floatParam(pid("shiftCents"), "Shift", -4800.0f, 1200.0f, 0.0f, "cents"));
        params.push_back(floatParam(pid("pitchDelay"), "P Delay", 0.01f, 0.3f, 0.02f, "s"));
        params.push_back(floatParam(pid("leftMix"), "Left Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("rightMix"), "Right Mix", 0.0f, 1.0f, 0.5f));

        params.push_back(sourceParam("filter1In", "Filt 1 In", PatchFactory::Source::kDelay1));
        params.push_back(sourceParam("filter2In", "Filt 2 In", PatchFactory::Source::kPitchShift));
        params.push_back(sourceParam("delay1In", "Delay 1 In", PatchFactory::Source::kSum2));
        params.push_back(sourceParam("delay2In", "Delay 2 In", PatchFactory::Source::kLeftInput));
        params.push_back(sourceParam("scale1In", "Scale 1 In", PatchFactory::Source::kNoiseGen));
        params.push_back(sourceParam("scale2In", "Scale 2 In", PatchFactory::Source::kLeftInput));
        params.push_back(sourceParam("sum1aIn", "Sum 1a In", PatchFactory::Source::kScaler1));
        params.push_back(sourceParam("sum1bIn", "Sum 1b In", PatchFactory::Source::kNullInput));
        params.push_back(sourceParam("sum2aIn", "Sum 2a In", PatchFactory::Source::kScaler2));
        params.push_back(sourceParam("sum2bIn", "Sum 2b In", PatchFactory::Source::kNullInput));
        params.push_back(sourceParam("shiftIn", "Shift In", PatchFactory::Source::kLeftInput));
        params.push_back(sourceParam("lOutput", "L Output", PatchFactory::Source::kLowpass1));
        params.push_back(sourceParam("rOutput", "R Output", PatchFactory::Source::kLowpass2));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::patchFactorySchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::PatchFactoryAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using dsp::algorithms::PatchFactory;
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("patchFactory", suffix)); };
        auto patch = [&](PatchFactory::Destination dest, const char* suffix) {
            engine_.setPatch(dest, static_cast<PatchFactory::Source>(static_cast<int>(v(suffix))));
        };

        engine_.setCutoff1(v("cutoff1"));
        engine_.setCutoff2(v("cutoff2"));
        engine_.setQ1(v("q1"));
        engine_.setQ2(v("q2"));
        engine_.setDelay1Seconds(v("delay1"));
        engine_.setDelay2Seconds(v("delay2"));
        engine_.setScale1(v("scale1"));
        engine_.setScale2(v("scale2"));
        engine_.setShiftCents(v("shiftCents"));
        engine_.setPitchDelaySeconds(v("pitchDelay"));
        engine_.setLeftMix(v("leftMix"));
        engine_.setRightMix(v("rightMix"));

        patch(PatchFactory::Destination::kFilter1In, "filter1In");
        patch(PatchFactory::Destination::kFilter2In, "filter2In");
        patch(PatchFactory::Destination::kDelay1In, "delay1In");
        patch(PatchFactory::Destination::kDelay2In, "delay2In");
        patch(PatchFactory::Destination::kScale1In, "scale1In");
        patch(PatchFactory::Destination::kScale2In, "scale2In");
        patch(PatchFactory::Destination::kSum1aIn, "sum1aIn");
        patch(PatchFactory::Destination::kSum1bIn, "sum1bIn");
        patch(PatchFactory::Destination::kSum2aIn, "sum2aIn");
        patch(PatchFactory::Destination::kSum2bIn, "sum2bIn");
        patch(PatchFactory::Destination::kShiftIn, "shiftIn");
        patch(PatchFactory::Destination::kLOutput, "lOutput");
        patch(PatchFactory::Destination::kROutput, "rOutput");

        engine_.process(left, right);
    }

  private:
    dsp::graphs::PatchFactoryAlgorithm engine_;
};
}

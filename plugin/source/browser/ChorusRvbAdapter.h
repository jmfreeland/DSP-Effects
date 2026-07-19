#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/ChorusRvbAlgorithm.h"
#include "dsp/schema/ChorusRvbSchema.h"
#include "pcm80/Pcm80UnitConvert.h"

#include <cmath>
#include <functional>
#include <span>

// Adapts dsp::graphs::ChorusRvbAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// ChorusRvbPluginProcessor.cpp's own layout/processBlock, namespaced
// under "chorusRvb".
namespace loom::browser
{
class ChorusRvbAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "chorusRvb"; }
    const char* displayName() const override { return "Lexicon Chorus+Rvb"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("chorusRvb", suffix); };

        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inPanLeft"), "In Pan L", -1.0f, 1.0f, -1.0f));
        params.push_back(floatParam(pid("inPanRight"), "In Pan R", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("chorusHighCut"), "Chorus High Cut", 1000.0f, 20000.0f, 10000.0f, "Hz", 0.4f));

        params.push_back(floatParam(pid("decay"), "Decay", 0.3f, 8.0f, 2.2f, "s", 0.5f));
        params.push_back(floatParam(pid("lowRatio"), "Low Ratio", 0.2f, 2.0f, 1.0f));
        params.push_back(floatParam(pid("crossover"), "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("damping"), "Damping", 0.0f, 1.0f, 0.3f));
        params.push_back(floatParam(pid("diffusion"), "Diffusion", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("size"), "Size", 0.0f, 1.0f, 0.6f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("link"), 1 }, "Link", false));
        params.push_back(floatParam(pid("attack"), "Attack", 0.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("rvbOut"), "Rvb Out", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("preDelay"), "Pre Delay", 0.0f, 0.93f, 0.0f, "s"));
        params.push_back(floatParam(pid("earlyReflectionLevelLeft"), "Early Reflections L", 0.0f, 1.0f, 0.2f));
        params.push_back(floatParam(pid("earlyReflectionLevelRight"), "Early Reflections R", 0.0f, 1.0f, 0.2f));
        params.push_back(
          floatParam(pid("earlyReflectionDelayLeft"), "Early Reflection Delay L", 0.0f, 1.2f, 0.03f, "s"));
        params.push_back(
          floatParam(pid("earlyReflectionDelayRight"), "Early Reflection Delay R", 0.0f, 1.2f, 0.03f, "s"));
        params.push_back(floatParam(pid("ekoDelayLeft"), "Eko Delay L", 0.0f, 1.2f, 0.0f, "s"));
        params.push_back(floatParam(pid("ekoDelayRight"), "Eko Delay R", 0.0f, 1.2f, 0.0f, "s"));
        params.push_back(floatParam(pid("ekoFeedbackLeft"), "Eko Feedback L", -1.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("ekoFeedbackRight"), "Eko Feedback R", -1.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("spin"), "Spin", 0.0f, 1.0f, 0.5f));

        params.push_back(floatParam(pid("chorusMasterDepth"), "Chorus Master Depth", 0.0f, 200.0f, 100.0f, "%"));
        params.push_back(floatParam(pid("chorusMasterRate"), "Chorus Master Rate", 0.0f, 200.0f, 100.0f, "%"));

        static constexpr float kDefaultDelay[6] = { 0.02f, 0.035f, 0.05f, 0.025f, 0.04f, 0.055f };
        static constexpr float kDefaultPan[6] = { -0.7f, -0.4f, -0.15f, 0.15f, 0.4f, 0.7f };
        static constexpr float kDefaultDepth[6] = { 12.0f, 18.0f, 24.0f, 14.0f, 20.0f, 26.0f };
        static constexpr float kDefaultRate[6] = { 0.25f, 0.31f, 0.19f, 0.28f, 0.22f, 0.34f };
        for (int i = 0; i < 6; ++i)
        {
            params.push_back(floatParam(voiceParamId("chorusRvb", i, "Delay"),
                                         "Voice " + juce::String(i + 1) + " Delay", 0.0f, 1.365f,
                                         kDefaultDelay[i], "s", 0.5f));
            params.push_back(floatParam(voiceParamId("chorusRvb", i, "Level"),
                                         "Voice " + juce::String(i + 1) + " Level", -1.0f, 1.0f, 0.5f));
            params.push_back(floatParam(voiceParamId("chorusRvb", i, "Pan"),
                                         "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                         kDefaultPan[i]));
            params.push_back(floatParam(voiceParamId("chorusRvb", i, "Feedback"),
                                         "Voice " + juce::String(i + 1) + " Fbk", -1.0f, 1.0f, 0.15f));
            params.push_back(floatParam(voiceParamId("chorusRvb", i, "Depth"),
                                         "Voice " + juce::String(i + 1) + " Depth", 0.0f, 500.0f,
                                         kDefaultDepth[i], "ms"));
            params.push_back(floatParam(voiceParamId("chorusRvb", i, "Rate"),
                                         "Voice " + juce::String(i + 1) + " Rate", 0.0f, 3.5f,
                                         kDefaultRate[i], "Hz"));
        }

        params.push_back(floatParam(pid("fxMix"), "FX Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("fxWidth"), "FX Width", -360.0f, 360.0f, 45.0f, "deg"));
        params.push_back(floatParam(pid("hiCut"), "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("fxAdjust"), "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 1.0f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("freeze"), 1 }, "Freeze", false));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::chorusRvbSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::ChorusRvbAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("chorusRvb", suffix)); };

        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));
        engine_.setInPan(v("inPanLeft"), v("inPanRight"));
        engine_.setChorusHighCut(v("chorusHighCut"));

        engine_.setDecaySeconds(v("decay"));
        engine_.setLowRatio(v("lowRatio"));
        engine_.setCrossoverFrequency(v("crossover"));
        engine_.setDamping(v("damping"));
        engine_.setDiffusion(v("diffusion"));
        engine_.setSize(v("size"));
        engine_.setLink(v("link") >= 0.5f);
        engine_.setAttack(v("attack"));
        engine_.setRvbOut(v("rvbOut"));
        engine_.setPreDelaySeconds(v("preDelay"));
        engine_.setEarlyReflectionLevel(v("earlyReflectionLevelLeft"), v("earlyReflectionLevelRight"));
        engine_.setEarlyReflectionDelaySeconds(v("earlyReflectionDelayLeft"), v("earlyReflectionDelayRight"));
        engine_.setEkoDelaySeconds(v("ekoDelayLeft"), v("ekoDelayRight"));
        engine_.setEkoFeedback(v("ekoFeedbackLeft"), v("ekoFeedbackRight"));
        engine_.setSpin(v("spin"));

        engine_.setChorusMaster(v("chorusMasterDepth"), v("chorusMasterRate"));

        for (int i = 0; i < 6; ++i)
        {
            auto vv = [&](const char* suffix) {
                return paramValue(apvts, voiceParamId("chorusRvb", i, suffix));
            };
            engine_.setVoiceDelay(i, vv("Delay"));
            engine_.setVoiceLevel(i, vv("Level"));
            engine_.setVoicePan(i, vv("Pan"));
            engine_.setVoiceFeedback(i, vv("Feedback"));
            engine_.setVoiceChorus(i, vv("Depth"), vv("Rate"));
        }

        engine_.setFxMix(v("fxMix"));
        engine_.setFxWidth(v("fxWidth"));
        engine_.setHiCut(v("hiCut"));
        engine_.setFxAdjustDb(v("fxAdjust"));
        engine_.setMix(v("mix"));
        engine_.setFrozen(v("freeze") >= 0.5f);

        engine_.process(left, right);
    }

    // See PlateAdapter.h's importPcm80Preset() for the pattern. Chorus+
    // Rvb's PCM80 field list uses CONTROLS_NO_VOICEDIF (has Controls
    // High Cut, mapped onto this adapter's shared hiCut - there's no
    // distinct PCM80 field for its own separate chorusHighCut, left
    // untouched), no glide, no clear. Chorus MstDepth/MstRate and each
    // voice's Depth/Rate are already in this adapter's own declared
    // units (0-200% and ms/Hz respectively, not 0-1 fractions or
    // seconds), so they're copied directly rather than through the
    // percent/ms helpers used elsewhere. No RvbDesign RvbIn, RvbWidth,
    // Definition, Depth, or Shape/Spread fields exist for this
    // algorithm, matching this adapter having no such parameters.
    const char* pcm80AlgorithmName() const override { return "Chorus+Rvb"; }

    void importPcm80Preset(const pcm80::Preset& preset, juce::AudioProcessorValueTreeState& apvts) const override
    {
        using namespace pcm80;
        auto pid = [](const char* suffix) { return prefixedId("chorusRvb", suffix); };

        auto apply = [&](const juce::String& group, const juce::String& label, const juce::String& paramId,
                          float lo, float hi, const std::function<float(const Field&)>& convert) {
            auto* f = preset.find(group, label);
            if (f == nullptr || !f->numeric.has_value())
            {
                return;
            }
            setParamValue(apvts, paramId, clampf(convert(*f), lo, hi));
        };

        auto percent = [](const Field& f) { return percentToFraction(*f.numeric); };
        auto db = [](const Field& f) { return dbToLinear(*f.numeric); };
        auto dbSigned = [](const Field& f) {
            auto linear = dbToLinear(*f.numeric);
            return f.unit == "db_phase_inverted" ? -linear : linear;
        };
        auto ms = [](const Field& f) { return msToSeconds(*f.numeric); };
        auto direct = [](const Field& f) { return static_cast<float>(*f.numeric); };
        auto boolean = direct;
        auto bipolarPercent = [](const Field& f) { return direct_(f) / 100.0f; };

        apply("Controls", "Mix", pid("mix"), 0.0f, 1.0f, percent);
        apply("Controls", "FX ADJUST", pid("fxAdjust"), -73.0f, 12.0f, direct);
        apply("Controls", "InLvl L", pid("inLevelLeft"), -1.0f, 1.0f, dbSigned);
        apply("Controls", "InLvl R", pid("inLevelRight"), -1.0f, 1.0f, dbSigned);
        apply("Controls", "InPan L", pid("inPanLeft"), -1.0f, 1.0f, direct);
        apply("Controls", "InPan R", pid("inPanRight"), -1.0f, 1.0f, direct);
        apply("Controls", "High Cut", pid("hiCut"), 1000.0f, 20000.0f, direct);
        apply("Controls", "FX Mix", pid("fxMix"), 0.0f, 1.0f, percent);
        apply("Controls", "FX Width", pid("fxWidth"), -360.0f, 360.0f, direct);

        apply("Rvb Time", "Low Rt", pid("lowRatio"), 0.2f, 2.0f, direct);
        apply("Rvb Time", "Mid Rt", pid("decay"), 0.3f, 8.0f, ms);
        apply("Rvb Time", "Crossover", pid("crossover"), 100.0f, 2000.0f, direct);
        apply("Rvb Time", "Rt HC", pid("damping"), 0.0f, 1.0f,
              [](const Field& f) { return 1.0f - (direct_(f) - 1000.0f) / 19000.0f; });
        apply("Rvb Time", "Pre Delay", pid("preDelay"), 0.0f, 0.93f, ms);
        apply("Rvb Time", "RefLvl L", pid("earlyReflectionLevelLeft"), 0.0f, 1.0f, db);
        apply("Rvb Time", "RefDly L", pid("earlyReflectionDelayLeft"), 0.0f, 1.2f, ms);
        apply("Rvb Time", "RefLvl R", pid("earlyReflectionLevelRight"), 0.0f, 1.0f, db);
        apply("Rvb Time", "RefDly R", pid("earlyReflectionDelayRight"), 0.0f, 1.2f, ms);
        apply("Rvb Time", "EkoFbk L", pid("ekoFeedbackLeft"), -1.0f, 1.0f, bipolarPercent);
        apply("Rvb Time", "EkoDly L", pid("ekoDelayLeft"), 0.0f, 1.2f, ms);
        apply("Rvb Time", "EkoFbk R", pid("ekoFeedbackRight"), -1.0f, 1.0f, bipolarPercent);
        apply("Rvb Time", "EkoDly R", pid("ekoDelayRight"), 0.0f, 1.2f, ms);

        apply("RvbDesign", "Size", pid("size"), 0.0f, 1.0f,
              [](const Field& f) { return (direct_(f) - 4.0f) / 72.0f; });
        apply("RvbDesign", "Diffusion", pid("diffusion"), 0.0f, 1.0f, percent);
        apply("RvbDesign", "Attack", pid("attack"), 0.0f, 1.0f, [](const Field& f) { return direct_(f) / 100.0f; });
        apply("RvbDesign", "Spin", pid("spin"), 0.0f, 1.0f, percent);
        apply("RvbDesign", "Link", pid("link"), 0.0f, 1.0f, boolean);
        apply("RvbDesign", "Rvb Out", pid("rvbOut"), 0.0f, 1.0f, db);

        apply("Chorus", "MstDepth", pid("chorusMasterDepth"), 0.0f, 200.0f, direct);
        apply("Chorus", "MstRate", pid("chorusMasterRate"), 0.0f, 200.0f, direct);

        static constexpr const char* kVoiceLabels[6] = { "Voice1", "Voice2", "Voice3",
                                                           "Voice4", "Voice5", "Voice6" };
        for (int i = 0; i < 6; ++i)
        {
            apply("Levels", kVoiceLabels[i], voiceParamId("chorusRvb", i, "Level"), -1.0f, 1.0f, dbSigned);
            apply("DelayTime", kVoiceLabels[i], voiceParamId("chorusRvb", i, "Delay"), 0.0f, 1.365f, ms);
            apply("Feedback", kVoiceLabels[i], voiceParamId("chorusRvb", i, "Feedback"), -1.0f, 1.0f,
                  bipolarPercent);
            apply("Panning", kVoiceLabels[i], voiceParamId("chorusRvb", i, "Pan"), -1.0f, 1.0f, direct);
            apply("Chorus", "V" + juce::String(i + 1) + " Depth", voiceParamId("chorusRvb", i, "Depth"),
                  0.0f, 500.0f, direct);
            apply("Chorus", "V" + juce::String(i + 1) + " Rate", voiceParamId("chorusRvb", i, "Rate"),
                  0.0f, 3.5f, direct);
        }
    }

  private:
    static float direct_(const pcm80::Field& f) { return static_cast<float>(*f.numeric); }

    dsp::graphs::ChorusRvbAlgorithm engine_;
};
}

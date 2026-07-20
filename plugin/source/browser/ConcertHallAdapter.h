#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/ConcertHallAlgorithm.h"
#include "dsp/schema/ReverbCoreSchemas.h"
#include "pcm80/Pcm80UnitConvert.h"

#include <cmath>
#include <functional>
#include <span>

// Adapts dsp::graphs::ConcertHallAlgorithm to EngineAdapter - the same
// parameter layout and processBlock logic as PluginProcessor.cpp (the
// single-algorithm Concert Hall plugin), just namespaced under the
// "concertHall" id and split into createParameters()/prepare()/process()
// so the browser can call them independently.
namespace loom::browser
{
class ConcertHallAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "concertHall"; }
    const char* displayName() const override { return "Lexicon Concert Hall"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("concertHall", suffix); };

        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inPanLeft"), "In Pan L", -1.0f, 1.0f, -1.0f));
        params.push_back(floatParam(pid("inPanRight"), "In Pan R", -1.0f, 1.0f, 1.0f));

        params.push_back(floatParam(pid("decay"), "Decay", 0.3f, 8.0f, 2.5f, "s", 0.5f));
        params.push_back(floatParam(pid("lowRatio"), "Low Ratio", 0.2f, 2.0f, 1.0f));
        params.push_back(floatParam(pid("crossover"), "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("damping"), "Damping", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("diffusion"), "Diffusion", 0.0f, 1.0f, 0.6f));
        params.push_back(floatParam(pid("size"), "Size", 0.0f, 1.0f, 1.0f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("link"), 1 }, "Link", false));
        params.push_back(floatParam(pid("definition"), "Definition", 0.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("depth"), "Depth", 0.0f, 1.0f, 0.5f));
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
        params.push_back(floatParam(pid("chorus"), "Chorus", 0.0f, 1.0f, 0.3f));

        params.push_back(floatParam(pid("voiceDiffusion"), "Voice Diffusion", 0.0f, 1.0f, 0.0f));
        static constexpr float kDefaultDelay[4] = { 0.09f, 0.13f, 0.0f, 0.0f };
        static constexpr float kDefaultFeedback[4] = { 0.15f, 0.10f, 0.0f, 0.0f };
        static constexpr float kDefaultLevel[4] = { 0.25f, 0.18f, 0.0f, 0.0f };
        static constexpr float kDefaultPan[4] = { -0.3f, 0.3f, 0.0f, 0.0f };
        for (int i = 0; i < 4; ++i)
        {
            params.push_back(floatParam(voiceParamId("concertHall", i, "Delay"),
                                         "Voice " + juce::String(i + 1) + " Delay", 0.0f, 1.365f,
                                         kDefaultDelay[i], "s"));
            params.push_back(floatParam(voiceParamId("concertHall", i, "Feedback"),
                                         "Voice " + juce::String(i + 1) + " Feedback", -1.0f, 1.0f,
                                         kDefaultFeedback[i]));
            params.push_back(floatParam(voiceParamId("concertHall", i, "Level"),
                                         "Voice " + juce::String(i + 1) + " Level", -1.0f, 1.0f,
                                         kDefaultLevel[i]));
            params.push_back(floatParam(voiceParamId("concertHall", i, "Pan"),
                                         "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                         kDefaultPan[i]));
        }
        params.push_back(floatParam(pid("voiceGlideResponse"), "Voice Glide Response", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("voiceGlideRange"), "Voice Glide Range", 0.0f, 1.365f, 0.0f, "s"));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("clear"), 1 }, "Clear", false));

        params.push_back(floatParam(pid("postDelayLeft"), "Post Delay L", 0.0f, 1.365f, 0.25f, "s"));
        params.push_back(floatParam(pid("postDelayRight"), "Post Delay R", 0.0f, 1.365f, 0.25f, "s"));
        params.push_back(
          floatParam(pid("postDelayGlideResponse"), "Post Delay Glide Response", 0.0f, 100.0f, 50.0f));
        params.push_back(
          floatParam(pid("postDelayGlideRange"), "Post Delay Glide Range", 0.0f, 1.365f, 0.0f, "s"));
        params.push_back(floatParam(pid("postDelayMix"), "Post Delay Mix", 0.0f, 1.0f, 0.15f));
        params.push_back(floatParam(pid("rvbWidth"), "Rvb Width", -360.0f, 360.0f, 45.0f, "deg"));
        params.push_back(floatParam(pid("fxMix"), "FX Mix", 0.0f, 1.0f, 0.75f));
        params.push_back(floatParam(pid("fxWidth"), "FX Width", -360.0f, 360.0f, 45.0f, "deg"));
        params.push_back(floatParam(pid("hiCut"), "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("fxAdjust"), "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 0.35f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("freeze"), 1 }, "Freeze", false));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::concertHallAlgorithmSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::ConcertHallAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("concertHall", suffix)); };

        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));
        engine_.setInPan(v("inPanLeft"), v("inPanRight"));
        engine_.setDecaySeconds(v("decay"));
        engine_.setLowRatio(v("lowRatio"));
        engine_.setCrossoverFrequency(v("crossover"));
        engine_.setDamping(v("damping"));
        engine_.setDiffusion(v("diffusion"));
        engine_.setSize(v("size"));
        engine_.setLink(v("link") >= 0.5f);
        engine_.setDefinition(v("definition"));
        engine_.setDepth(v("depth"));
        engine_.setRvbIn(v("rvbIn"));
        engine_.setRvbOut(v("rvbOut"));
        engine_.setPreDelaySeconds(v("preDelay"));
        engine_.setEarlyReflectionLevel(v("earlyReflectionLevelLeft"), v("earlyReflectionLevelRight"));
        engine_.setEarlyReflectionDelaySeconds(v("earlyReflectionDelayLeft"), v("earlyReflectionDelayRight"));
        engine_.setSpin(v("spin"));
        engine_.setChorus(v("chorus"));
        engine_.setVoiceDiffusion(v("voiceDiffusion"));

        engine_.setVoiceGlide(v("voiceGlideResponse"), v("voiceGlideRange"));
        for (int i = 0; i < 4; ++i)
        {
            auto vv = [&](const char* suffix) {
                return paramValue(apvts, voiceParamId("concertHall", i, suffix));
            };
            engine_.setVoice(i, vv("Delay"), vv("Feedback"), vv("Level"), vv("Pan"));
        }
        engine_.setClear(v("clear") >= 0.5f);

        engine_.setPostDelayGlide(v("postDelayGlideResponse"), v("postDelayGlideRange"));
        engine_.setPostDelaySeconds(v("postDelayLeft"), v("postDelayRight"));
        engine_.setPostDelayMix(v("postDelayMix"));
        engine_.setRvbWidth(v("rvbWidth"));
        engine_.setFxMix(v("fxMix"));
        engine_.setFxWidth(v("fxWidth"));
        engine_.setHiCut(v("hiCut"));
        engine_.setFxAdjustDb(v("fxAdjust"));
        engine_.setMix(v("mix"));
        engine_.setFrozen(v("freeze") >= 0.5f);

        engine_.process(left, right);
    }

    // See PlateAdapter.h's importPcm80Preset() for the pattern. Concert
    // Hall's PCM80 field list uses the same "Rvb Time"/"RvbDesign"/etc
    // groups as Plate/Chamber but with three fields unique to it (Def,
    // Depth, Chorus, matching this adapter's own definition/depth/
    // chorus) in place of Attack/Shape+Spread - Depth is a 4-bit raw
    // field (max 15) and Chorus a 4-bit raw field (max 10), so both
    // normalize by their own declared max rather than the /100 used for
    // percent-scale fields elsewhere. Mid Rt is additionally halved -
    // see hallMidRt's own doc comment below.
    //
    // Like every other algorithm here, this imports each Levels/
    // DelayTime/Feedback/Panning "VoiceN" field directly and does not
    // apply the corresponding "Master" field (Levels Master/DelayTime
    // Master/Feedback Master/Panning Master) that would otherwise scale
    // all four voices together - the MIDI Implementation Details manual
    // documents the Master fields' own display formatting (Range Decode
    // 33/9/9/12) but never the Master-to-Voice combination formula
    // itself, so it isn't implemented. A real preset with non-neutral
    // Masters (anything other than Levels +0dB / DelayTime, Feedback,
    // Panning 100%) will therefore import its raw per-voice values
    // un-scaled; test presets with Masters left at their neutral default
    // avoid this gap entirely.
    const char* pcm80AlgorithmName() const override { return "Concert Hall"; }

    void importPcm80Preset(const pcm80::Preset& preset, juce::AudioProcessorValueTreeState& apvts) const override
    {
        using namespace pcm80;
        auto pid = [](const char* suffix) { return prefixedId("concertHall", suffix); };

        auto apply = [&](const char* group, const char* label, const juce::String& paramId, float lo,
                          float hi, const std::function<float(const Field&)>& convert) {
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
        // Concert Hall's real decay time is half of what Range Decode
        // 16's own documented lookup-table+Link/Size formula computes -
        // an empirical hardware quirk (confirmed against real PCM81
        // hardware), not something the MIDI Implementation Details
        // manual itself documents, and not shared by Plate/Chamber/
        // Infinite/Inverse's own Mid Rt-equivalent fields.
        auto hallMidRt = [](const Field& f) { return msToSeconds(*f.numeric) * 0.5f; };

        apply("Controls", "Mix", pid("mix"), 0.0f, 1.0f, percent);
        apply("Controls", "FX ADJUST", pid("fxAdjust"), -73.0f, 12.0f, direct);
        apply("Controls", "InLvl L", pid("inLevelLeft"), -1.0f, 1.0f, dbSigned);
        apply("Controls", "InLvl R", pid("inLevelRight"), -1.0f, 1.0f, dbSigned);
        apply("Controls", "InPan L", pid("inPanLeft"), -1.0f, 1.0f, direct);
        apply("Controls", "InPan R", pid("inPanRight"), -1.0f, 1.0f, direct);
        apply("Controls", "High Cut", pid("hiCut"), 1000.0f, 20000.0f, direct);
        apply("Controls", "Voice Dif", pid("voiceDiffusion"), 0.0f, 1.0f, percent);
        apply("Controls", "FX Mix", pid("fxMix"), 0.0f, 1.0f, percent);
        apply("Controls", "FX Width", pid("fxWidth"), -360.0f, 360.0f, direct);

        apply("RvbTime", "LowRt", pid("lowRatio"), 0.2f, 2.0f, direct);
        apply("RvbTime", "MidRt", pid("decay"), 0.3f, 8.0f, hallMidRt);
        apply("RvbTime", "Crossover", pid("crossover"), 100.0f, 2000.0f, direct);
        apply("RvbTime", "RtHC", pid("damping"), 0.0f, 1.0f,
              [](const Field& f) { return 1.0f - (direct_(f) - 1000.0f) / 19000.0f; });
        apply("RvbTime", "PreDelay", pid("preDelay"), 0.0f, 0.93f, ms);
        apply("RvbTime", "RefLvlL", pid("earlyReflectionLevelLeft"), 0.0f, 1.0f, db);
        apply("RvbTime", "RefDlyL", pid("earlyReflectionDelayLeft"), 0.0f, 1.2f, ms);
        apply("RvbTime", "RefLvlR", pid("earlyReflectionLevelRight"), 0.0f, 1.0f, db);
        apply("RvbTime", "RefDlyR", pid("earlyReflectionDelayRight"), 0.0f, 1.2f, ms);
        apply("RvbTime", "PstMix", pid("postDelayMix"), 0.0f, 1.0f, percent);
        apply("RvbTime", "PstDlyL", pid("postDelayLeft"), 0.0f, 1.365f, ms);
        apply("RvbTime", "PstDlyR", pid("postDelayRight"), 0.0f, 1.365f, ms);
        apply("RvbTime", "GldResp", pid("postDelayGlideResponse"), 0.0f, 100.0f, direct);
        apply("RvbTime", "GldRange", pid("postDelayGlideRange"), 0.0f, 1.365f, ms);

        apply("RvbDesign", "Size", pid("size"), 0.0f, 1.0f,
              [](const Field& f) { return (direct_(f) - 4.0f) / 72.0f; });
        apply("RvbDesign", "Diffusion", pid("diffusion"), 0.0f, 1.0f, percent);
        apply("RvbDesign", "Def", pid("definition"), 0.0f, 1.0f, percent);
        apply("RvbDesign", "Depth", pid("depth"), 0.0f, 1.0f, [](const Field& f) { return direct_(f) / 15.0f; });
        apply("RvbDesign", "Spin", pid("spin"), 0.0f, 1.0f, percent);
        apply("RvbDesign", "Chorus", pid("chorus"), 0.0f, 1.0f, [](const Field& f) { return direct_(f) / 10.0f; });
        apply("RvbDesign", "Link", pid("link"), 0.0f, 1.0f, boolean);
        apply("RvbDesign", "RvbWidth", pid("rvbWidth"), -360.0f, 360.0f, direct);
        apply("RvbDesign", "RvbIn", pid("rvbIn"), 0.0f, 1.0f, db);
        apply("RvbDesign", "RvbOut", pid("rvbOut"), 0.0f, 1.0f, db);

        apply("DelayTime", "GldResp", pid("voiceGlideResponse"), 0.0f, 100.0f, direct);
        apply("DelayTime", "GldRange", pid("voiceGlideRange"), 0.0f, 1.365f, ms);
        apply("DelayTime", "Clear", pid("clear"), 0.0f, 1.0f, boolean);

        static constexpr const char* kVoiceLabels[4] = { "Voice1", "Voice2", "Voice3", "Voice4" };
        for (int i = 0; i < 4; ++i)
        {
            apply("Levels", kVoiceLabels[i], voiceParamId("concertHall", i, "Level"), -1.0f, 1.0f, dbSigned);
            apply("DelayTime", kVoiceLabels[i], voiceParamId("concertHall", i, "Delay"), 0.0f, 1.365f, ms);
            apply("Feedback", kVoiceLabels[i], voiceParamId("concertHall", i, "Feedback"), -1.0f, 1.0f,
                  [](const Field& f) { return direct_(f) / 100.0f; });
            apply("Panning", kVoiceLabels[i], voiceParamId("concertHall", i, "Pan"), -1.0f, 1.0f, direct);
        }
    }

  private:
    static float direct_(const pcm80::Field& f) { return static_cast<float>(*f.numeric); }

    dsp::graphs::ConcertHallAlgorithm engine_;
};
}

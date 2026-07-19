#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/Res1PlateAlgorithm.h"
#include "dsp/schema/Res1PlateSchema.h"
#include "pcm80/Pcm80UnitConvert.h"

#include <cmath>
#include <functional>
#include <span>

// Adapts dsp::graphs::Res1PlateAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// Res1PlatePluginProcessor.cpp's own layout/processBlock, namespaced
// under "res1Plate".
namespace loom::browser
{
class Res1PlateAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "res1Plate"; }
    const char* displayName() const override { return "Lexicon Res1>Plate"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("res1Plate", suffix); };

        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inPanLeft"), "In Pan L", -1.0f, 1.0f, -1.0f));
        params.push_back(floatParam(pid("inPanRight"), "In Pan R", -1.0f, 1.0f, 1.0f));

        params.push_back(floatParam(pid("decay"), "Decay", 0.3f, 8.0f, 2.0f, "s", 0.5f));
        params.push_back(floatParam(pid("lowRatio"), "Low Ratio", 0.2f, 2.0f, 1.0f));
        params.push_back(floatParam(pid("crossover"), "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("damping"), "Damping", 0.0f, 1.0f, 0.4f));
        params.push_back(floatParam(pid("diffusion"), "Diffusion", 0.0f, 1.0f, 0.6f));
        params.push_back(floatParam(pid("size"), "Size", 0.0f, 1.0f, 0.5f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("link"), 1 }, "Link", false));
        params.push_back(floatParam(pid("attack"), "Attack", 0.0f, 1.0f, 1.0f));
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

        static constexpr float kDefaultHz[6] = { 130.81f, 164.81f, 196.00f, 261.63f, 329.63f, 392.00f };
        static constexpr float kDefaultPan[6] = { -0.7f, -0.35f, -0.85f, 0.7f, 0.35f, 0.85f };
        for (int i = 0; i < 6; ++i)
        {
            params.push_back(floatParam(voiceParamId("res1Plate", i, "Pitch"),
                                         "Voice " + juce::String(i + 1) + " Pitch", 20.0f, 4000.0f,
                                         kDefaultHz[i], "Hz", 0.3f));
            params.push_back(floatParam(voiceParamId("res1Plate", i, "Level"),
                                         "Voice " + juce::String(i + 1) + " Level", -1.0f, 1.0f, 0.5f));
            params.push_back(floatParam(voiceParamId("res1Plate", i, "Pan"),
                                         "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                         kDefaultPan[i]));
            params.push_back(floatParam(voiceParamId("res1Plate", i, "Duration"),
                                         "Voice " + juce::String(i + 1) + " Duration", 0.05f, 8.0f, 3.0f,
                                         "s", 0.4f));
            params.push_back(floatParam(voiceParamId("res1Plate", i, "HiCut"),
                                         "Voice " + juce::String(i + 1) + " Hi Cut", 200.0f, 20000.0f,
                                         4000.0f, "Hz", 0.4f));
        }

        params.push_back(floatParam(pid("voiceDiffusion"), "Voice Diffusion", 0.0f, 1.0f, 0.2f));
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
        return dsp::schema::res1PlateSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::Res1PlateAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("res1Plate", suffix)); };

        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));
        engine_.setInPan(v("inPanLeft"), v("inPanRight"));

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

        for (int i = 0; i < 6; ++i)
        {
            auto vv = [&](const char* suffix) {
                return paramValue(apvts, voiceParamId("res1Plate", i, suffix));
            };
            engine_.setVoicePitch(i, vv("Pitch"));
            engine_.setVoiceLevel(i, vv("Level"));
            engine_.setVoicePan(i, vv("Pan"));
            engine_.setVoiceDuration(i, vv("Duration"));
            engine_.setVoiceHiCut(i, vv("HiCut"));
        }

        engine_.setVoiceDiffusion(v("voiceDiffusion"));
        engine_.setFxMix(v("fxMix"));
        engine_.setFxWidth(v("fxWidth"));
        engine_.setHiCut(v("hiCut"));
        engine_.setFxAdjustDb(v("fxAdjust"));
        engine_.setMix(v("mix"));
        engine_.setFrozen(v("freeze") >= 0.5f);

        engine_.process(left, right);
    }

    // See PlateAdapter.h's importPcm80Preset() for the pattern. Res1>
    // Plate's PCM80 field list uses CONTROLS_MINIMAL (no Controls High
    // Cut, so this adapter's shared hiCut has no source - PCM80's own
    // per-voice pitch comes from a MIDI-note round-robin this project
    // has no pathway for, per CLAUDE.md, which is why this adapter
    // exposes a direct settable Hz per voice instead). The closest
    // PCM80 correspondence to that Hz value is each voice's own
    // DelayTime (a Karplus-Strong resonator's delay period), so Pitch
    // is derived via the physical fundamental-frequency relationship
    // (Hz = 1000/delay_ms) rather than left unmapped - an approximation,
    // not a verified match. Resonance V{n} Res (a bipolar percent) has
    // no established unit correspondence to this adapter's own Duration
    // (an RT60 target in seconds) and is left unmapped rather than
    // guessed. No RvbDesign RvbIn/RvbWidth or master Levels/Resonance/
    // Pitch-assignment fields have a corresponding parameter either.
    const char* pcm80AlgorithmName() const override { return "Res1>Plate"; }

    void importPcm80Preset(const pcm80::Preset& preset, juce::AudioProcessorValueTreeState& apvts) const override
    {
        using namespace pcm80;
        auto pid = [](const char* suffix) { return prefixedId("res1Plate", suffix); };

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

        static constexpr const char* kPanLabels[6] = { "Voice1", "Voice2", "Voice3", "Voice4", "Voice5", "Voice6" };
        for (int i = 0; i < 6; ++i)
        {
            auto vLabel = "V" + juce::String(i + 1);
            apply("Levels", vLabel + " Lvl", voiceParamId("res1Plate", i, "Level"), -1.0f, 1.0f, dbSigned);
            apply("Panning", kPanLabels[i], voiceParamId("res1Plate", i, "Pan"), -1.0f, 1.0f, direct);
            apply("Resonance", vLabel + " HiCut", voiceParamId("res1Plate", i, "HiCut"), 200.0f, 20000.0f,
                  direct);
            apply("DelayTime", "Voice" + juce::String(i + 1), voiceParamId("res1Plate", i, "Pitch"), 20.0f,
                  4000.0f, [](const Field& f) { return 1000.0f / std::max(direct_(f), 0.25f); });
        }
    }

  private:
    static float direct_(const pcm80::Field& f) { return static_cast<float>(*f.numeric); }

    dsp::graphs::Res1PlateAlgorithm engine_;
};
}

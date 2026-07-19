#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/Res2PlateAlgorithm.h"
#include "dsp/schema/Res2PlateSchema.h"
#include "pcm80/Pcm80UnitConvert.h"

#include <cmath>
#include <functional>
#include <span>

// Adapts dsp::graphs::Res2PlateAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// Res2PlatePluginProcessor.cpp's own layout/processBlock, namespaced
// under "res2Plate".
namespace loom::browser
{
namespace res2plate_detail
{
inline const juce::StringArray kKeyNames = { "C",  "C#", "D",  "D#", "E",  "F",
                                              "F#", "G",  "G#", "A",  "A#", "B" };
inline const juce::StringArray kScaleNames = { "Major", "Natural Minor", "Harmonic Minor", "Dorian",
                                                "Mixolydian", "Lydian" };
// Order matches dsp::HarmonicInterval exactly, so the AudioParameterChoice
// index can be cast straight to the enum - see Res2PlatePluginProcessor.cpp,
// the original source of this table.
inline const juce::StringArray kIntervalNames = { "Octave Down",       "7th Down",         "6th Down",
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

class Res2PlateAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "res2Plate"; }
    const char* displayName() const override { return "Lexicon Res2>Plate"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace res2plate_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("res2Plate", suffix); };

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

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("key"), 1 }, "Key", kKeyNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("scale"), 1 }, "Scale", kScaleNames, 0));
        params.push_back(floatParam(pid("tune"), "Tune", -50.0f, 50.0f, 0.0f, "cents"));
        params.push_back(floatParam(pid("lowNoteHz"), "Low Note", 20.0f, 800.0f, 80.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("highNoteHz"), "High Note", 200.0f, 4000.0f, 800.0f, "Hz", 0.4f));

        static constexpr int kDefaultIntervalIndex[6] = { 8, 10, 13, 7, 11, 12 };
        static constexpr float kDefaultPan[6] = { -0.7f, -0.35f, -0.85f, 0.7f, 0.35f, 0.85f };
        for (int i = 0; i < 6; ++i)
        {
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
              juce::ParameterID{ voiceParamId("res2Plate", i, "Interval"), 1 },
              "Voice " + juce::String(i + 1) + " Interval", kIntervalNames, kDefaultIntervalIndex[i]));
            params.push_back(floatParam(voiceParamId("res2Plate", i, "Level"),
                                         "Voice " + juce::String(i + 1) + " Level", -1.0f, 1.0f, 0.5f));
            params.push_back(floatParam(voiceParamId("res2Plate", i, "Pan"),
                                         "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                         kDefaultPan[i]));
            params.push_back(floatParam(voiceParamId("res2Plate", i, "Duration"),
                                         "Voice " + juce::String(i + 1) + " Duration", 0.05f, 8.0f, 2.5f,
                                         "s", 0.4f));
            params.push_back(floatParam(voiceParamId("res2Plate", i, "HiCut"),
                                         "Voice " + juce::String(i + 1) + " Hi Cut", 200.0f, 20000.0f,
                                         4000.0f, "Hz", 0.4f));
        }

        params.push_back(floatParam(pid("voiceDiffusion"), "Voice Diffusion", 0.0f, 1.0f, 0.2f));
        params.push_back(floatParam(pid("fxMix"), "FX Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("fxWidth"), "FX Width", -360.0f, 360.0f, 0.0f, "deg"));
        params.push_back(floatParam(pid("hiCut"), "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("fxAdjust"), "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 1.0f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("freeze"), 1 }, "Freeze", false));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::res2PlateSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::Res2PlateAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using namespace res2plate_detail;
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("res2Plate", suffix)); };

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

        engine_.setKey(static_cast<int>(v("key")));
        engine_.setScale(scaleFromIndex(static_cast<int>(v("scale"))));
        engine_.setTuneCents(v("tune"));
        engine_.setFrequencyRange(v("lowNoteHz"), v("highNoteHz"));

        for (int i = 0; i < 6; ++i)
        {
            auto vv = [&](const char* suffix) {
                return paramValue(apvts, voiceParamId("res2Plate", i, suffix));
            };
            engine_.setVoiceInterval(i, static_cast<dsp::HarmonicInterval>(static_cast<int>(vv("Interval"))));
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

    // See PlateAdapter.h's importPcm80Preset() for the pattern; Res2>
    // Plate shares Res1>Plate's reverb-core field list (see that
    // adapter's own doc comment for what's skipped there and why) but
    // replaces per-voice direct Hz with PCM80's actual Pitch group
    // (Key/Scale/Root/Rule/per-voice interval), which corresponds much
    // more directly to this adapter's own Key/Scale/Tune/Interval
    // design than Res1>Plate's did. Pitch Scale only has 2 raw values
    // (Major/Harmonic) against this adapter's 6 choices, mapped to the
    // closest two (Major, Harmonic Minor); Pitch Root (a scale-degree
    // offset) and Pitch Rule (round/shift behavior) have no equivalent
    // here and are left unmapped, as is Resonance V{n} Res (see
    // Res1PlateAdapter.h's note on why a bipolar percent doesn't map to
    // a Duration in seconds).
    const char* pcm80AlgorithmName() const override { return "Res2>Plate"; }

    void importPcm80Preset(const pcm80::Preset& preset, juce::AudioProcessorValueTreeState& apvts) const override
    {
        using namespace pcm80;
        auto pid = [](const char* suffix) { return prefixedId("res2Plate", suffix); };

        auto apply = [&](const juce::String& group, const juce::String& label, const juce::String& paramId,
                          float lo, float hi, const std::function<float(const Field&)>& convert) {
            auto* f = preset.find(group, label);
            if (f == nullptr || !f->numeric.has_value())
            {
                return;
            }
            setParamValue(apvts, paramId, clampf(convert(*f), lo, hi));
        };
        // For enum/choice parameters PCM80 encodes as an index with no
        // single-number "meaning" (so NUMERIC_DECODE_FUNCS reports
        // none) - reads the raw index directly instead.
        auto applyRawIndex = [&](const juce::String& group, const juce::String& label,
                                  const juce::String& paramId, float maxIndex) {
            auto* f = preset.find(group, label);
            if (f == nullptr)
            {
                return;
            }
            setParamValue(apvts, paramId, clampf(static_cast<float>(f->raw), 0.0f, maxIndex));
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
        apply("Rvb Time", "EkoFbk L", pid("ekoFeedbackLeft"), -1.0f, 1.0f,
              [](const Field& f) { return direct_(f) / 100.0f; });
        apply("Rvb Time", "EkoDly L", pid("ekoDelayLeft"), 0.0f, 1.2f, ms);
        apply("Rvb Time", "EkoFbk R", pid("ekoFeedbackRight"), -1.0f, 1.0f,
              [](const Field& f) { return direct_(f) / 100.0f; });
        apply("Rvb Time", "EkoDly R", pid("ekoDelayRight"), 0.0f, 1.2f, ms);

        apply("RvbDesign", "Size", pid("size"), 0.0f, 1.0f,
              [](const Field& f) { return (direct_(f) - 4.0f) / 72.0f; });
        apply("RvbDesign", "Diffusion", pid("diffusion"), 0.0f, 1.0f, percent);
        apply("RvbDesign", "Attack", pid("attack"), 0.0f, 1.0f, [](const Field& f) { return direct_(f) / 100.0f; });
        apply("RvbDesign", "Spin", pid("spin"), 0.0f, 1.0f, percent);
        apply("RvbDesign", "Link", pid("link"), 0.0f, 1.0f, boolean);
        apply("RvbDesign", "Rvb Out", pid("rvbOut"), 0.0f, 1.0f, db);

        applyRawIndex("Pitch", "Key", pid("key"), 11.0f);
        if (auto* scaleField = preset.find("Pitch", "Scale"))
        {
            setParamValue(apvts, pid("scale"), scaleField->raw >= 0.5 ? 2.0f : 0.0f);
        }
        apply("Pitch", "Tuning", pid("tune"), -50.0f, 50.0f,
              [](const Field& f) { return 1200.0f * std::log2(direct_(f) / 440.0f); });

        static constexpr const char* kVLabels[6] = { "V1", "V2", "V3", "V4", "V5", "V6" };
        static constexpr const char* kVoiceLabels[6] = { "Voice1", "Voice2", "Voice3",
                                                           "Voice4", "Voice5", "Voice6" };
        for (int i = 0; i < 6; ++i)
        {
            apply("Levels", juce::String(kVLabels[i]) + " Lvl", voiceParamId("res2Plate", i, "Level"),
                  -1.0f, 1.0f, dbSigned);
            apply("Panning", kVoiceLabels[i], voiceParamId("res2Plate", i, "Pan"), -1.0f, 1.0f, direct);
            apply("Resonance", juce::String(kVLabels[i]) + " HiCut", voiceParamId("res2Plate", i, "HiCut"),
                  200.0f, 20000.0f, direct);
            applyRawIndex("Pitch", kVoiceLabels[i], voiceParamId("res2Plate", i, "Interval"), 17.0f);
        }
    }

  private:
    static float direct_(const pcm80::Field& f) { return static_cast<float>(*f.numeric); }

    dsp::graphs::Res2PlateAlgorithm engine_;
};
}

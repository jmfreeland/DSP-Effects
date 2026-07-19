#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/GlideHallAlgorithm.h"
#include "dsp/schema/GlideHallSchema.h"
#include "pcm80/Pcm80UnitConvert.h"

#include <cmath>
#include <functional>
#include <span>

// Adapts dsp::graphs::GlideHallAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// GlideHallPluginProcessor.cpp's own layout/processBlock, namespaced
// under "glideHall".
namespace loom::browser
{
class GlideHallAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "glideHall"; }
    const char* displayName() const override { return "Lexicon Glide>Hall"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("glideHall", suffix); };

        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inPanLeft"), "In Pan L", -1.0f, 1.0f, -1.0f));
        params.push_back(floatParam(pid("inPanRight"), "In Pan R", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("voiceDiffusion"), "Voice Diffusion", 0.0f, 1.0f, 0.3f));

        params.push_back(floatParam(pid("decay"), "Decay", 0.3f, 8.0f, 2.5f, "s", 0.5f));
        params.push_back(floatParam(pid("lowRatio"), "Low Ratio", 0.2f, 2.0f, 1.0f));
        params.push_back(floatParam(pid("crossover"), "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("damping"), "Damping", 0.0f, 1.0f, 0.4f));
        params.push_back(floatParam(pid("diffusion"), "Diffusion", 0.0f, 1.0f, 0.6f));
        params.push_back(floatParam(pid("size"), "Size", 0.0f, 1.0f, 0.6f));
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

        params.push_back(floatParam(pid("glideLevel"), "Gld Lvl", 0.0f, 1.0f, 0.8f));
        params.push_back(floatParam(pid("glideTapALevelLeft"), "A Lvl L", -1.0f, 1.0f, 0.7f));
        params.push_back(floatParam(pid("glideTapADelayLeft"), "A Dly L", 0.0f, 0.042f, 0.006f, "s"));
        params.push_back(floatParam(pid("glideTapALevelRight"), "A Lvl R", -1.0f, 1.0f, 0.7f));
        params.push_back(floatParam(pid("glideTapADelayRight"), "A Dly R", 0.0f, 0.042f, 0.007f, "s"));
        params.push_back(floatParam(pid("glideTapBLevelLeft"), "B Lvl L", -1.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("glideTapBDelayLeft"), "B Dly L", 0.0f, 0.042f, 0.018f, "s"));
        params.push_back(floatParam(pid("glideTapBLevelRight"), "B Lvl R", -1.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("glideTapBDelayRight"), "B Dly R", 0.0f, 0.042f, 0.020f, "s"));
        params.push_back(floatParam(pid("glideFeedbackLeft"), "Fbk L", -1.0f, 1.0f, 0.2f));
        params.push_back(floatParam(pid("glideFeedbackRight"), "Fbk R", -1.0f, 1.0f, 0.2f));
        params.push_back(floatParam(pid("glideCrossFeedbackLeft"), "X-Fbk L", -1.0f, 1.0f, 0.1f));
        params.push_back(floatParam(pid("glideCrossFeedbackRight"), "X-Fbk R", -1.0f, 1.0f, 0.1f));

        static constexpr float kDefaultDelay[6] = { 0.12f, 0.24f, 0.36f, 0.15f, 0.27f, 0.39f };
        static constexpr float kDefaultPan[6] = { -0.6f, -0.3f, -0.8f, 0.6f, 0.3f, 0.8f };
        for (int i = 0; i < 6; ++i)
        {
            params.push_back(floatParam(voiceParamId("glideHall", i, "Delay"),
                                         "Voice " + juce::String(i + 1) + " Delay", 0.0f, 10.581f,
                                         kDefaultDelay[i], "s", 0.4f));
            params.push_back(floatParam(voiceParamId("glideHall", i, "Level"),
                                         "Voice " + juce::String(i + 1) + " Level", -1.0f, 1.0f, 0.5f));
            params.push_back(floatParam(voiceParamId("glideHall", i, "Pan"),
                                         "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                         kDefaultPan[i]));
            params.push_back(floatParam(voiceParamId("glideHall", i, "Feedback"),
                                         "Voice " + juce::String(i + 1) + " Fbk", -1.0f, 1.0f, 0.15f));
            params.push_back(floatParam(voiceParamId("glideHall", i, "CrossFeedback"),
                                         "Voice " + juce::String(i + 1) + " X-Fbk", -1.0f, 1.0f, 0.03f));
        }

        params.push_back(floatParam(pid("fxMix"), "FX Mix", 0.0f, 1.0f, 0.6f));
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
        return dsp::schema::glideHallSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::GlideHallAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("glideHall", suffix)); };

        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));
        engine_.setInPan(v("inPanLeft"), v("inPanRight"));
        engine_.setVoiceDiffusion(v("voiceDiffusion"));

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

        engine_.setGlideLevel(v("glideLevel"));
        engine_.setGlideTapALeft(v("glideTapALevelLeft"), v("glideTapADelayLeft"));
        engine_.setGlideTapARight(v("glideTapALevelRight"), v("glideTapADelayRight"));
        engine_.setGlideTapBLeft(v("glideTapBLevelLeft"), v("glideTapBDelayLeft"));
        engine_.setGlideTapBRight(v("glideTapBLevelRight"), v("glideTapBDelayRight"));
        engine_.setGlideFeedback(v("glideFeedbackLeft"), v("glideFeedbackRight"));
        engine_.setGlideCrossFeedback(v("glideCrossFeedbackLeft"), v("glideCrossFeedbackRight"));

        for (int i = 0; i < 6; ++i)
        {
            auto vv = [&](const char* suffix) {
                return paramValue(apvts, voiceParamId("glideHall", i, suffix));
            };
            engine_.setVoiceDelay(i, vv("Delay"));
            engine_.setVoiceLevel(i, vv("Level"));
            engine_.setVoicePan(i, vv("Pan"));
            engine_.setVoiceFeedback(i, vv("Feedback"));
            engine_.setVoiceCrossFeedback(i, vv("CrossFeedback"));
        }

        engine_.setFxMix(v("fxMix"));
        engine_.setFxWidth(v("fxWidth"));
        engine_.setHiCut(v("hiCut"));
        engine_.setFxAdjustDb(v("fxAdjust"));
        engine_.setMix(v("mix"));
        engine_.setFrozen(v("freeze") >= 0.5f);

        engine_.process(left, right);
    }

    // See PlateAdapter.h's importPcm80Preset() for the pattern. Glide>
    // Hall's PCM80 field list uses CONTROLS_NO_HIGHCUT (has Voice Dif,
    // no Controls High Cut - and no other field maps onto this
    // adapter's own hiCut either, so it's left untouched), no Rvb Time
    // Eko fields, and its own "Glide FX" group for the two glide taps
    // (A/B Lvl/Dly per side) and their Feedback/X-Feedback - Glide FX
    // A/B Dly use 0.1ms increments (rd43), finer-grained than the 1ms
    // fields used elsewhere. The per-voice Feedback group is also
    // uniquely named ("V1 Fbk"/"V1 X-Fbk" rather than "Voice1"). No
    // DelayTime Clear field exists (matching GlideHallAlgorithm having
    // no clear() of its own).
    const char* pcm80AlgorithmName() const override { return "Glide>Hall"; }

    void importPcm80Preset(const pcm80::Preset& preset, juce::AudioProcessorValueTreeState& apvts) const override
    {
        using namespace pcm80;
        auto pid = [](const char* suffix) { return prefixedId("glideHall", suffix); };

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
        apply("Controls", "Voice Dif", pid("voiceDiffusion"), 0.0f, 1.0f, percent);
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

        apply("RvbDesign", "Size", pid("size"), 0.0f, 1.0f,
              [](const Field& f) { return (direct_(f) - 4.0f) / 72.0f; });
        apply("RvbDesign", "Diffusion", pid("diffusion"), 0.0f, 1.0f, percent);
        apply("RvbDesign", "Def", pid("definition"), 0.0f, 1.0f, percent);
        apply("RvbDesign", "Depth", pid("depth"), 0.0f, 1.0f, [](const Field& f) { return direct_(f) / 15.0f; });
        apply("RvbDesign", "Spin", pid("spin"), 0.0f, 1.0f, percent);
        apply("RvbDesign", "Chorus", pid("chorus"), 0.0f, 1.0f, [](const Field& f) { return direct_(f) / 10.0f; });
        apply("RvbDesign", "Link", pid("link"), 0.0f, 1.0f, boolean);
        apply("RvbDesign", "Rvb In", pid("rvbIn"), 0.0f, 1.0f, db);
        apply("RvbDesign", "Rvb Out", pid("rvbOut"), 0.0f, 1.0f, db);

        apply("Glide FX", "Gld Lvl", pid("glideLevel"), 0.0f, 1.0f, db);
        apply("Glide FX", "A Lvl L", pid("glideTapALevelLeft"), -1.0f, 1.0f, dbSigned);
        apply("Glide FX", "A Dly L", pid("glideTapADelayLeft"), 0.0f, 0.042f, ms);
        apply("Glide FX", "A Lvl R", pid("glideTapALevelRight"), -1.0f, 1.0f, dbSigned);
        apply("Glide FX", "A Dly R", pid("glideTapADelayRight"), 0.0f, 0.042f, ms);
        apply("Glide FX", "B Lvl L", pid("glideTapBLevelLeft"), -1.0f, 1.0f, dbSigned);
        apply("Glide FX", "B Dly L", pid("glideTapBDelayLeft"), 0.0f, 0.042f, ms);
        apply("Glide FX", "B Lvl R", pid("glideTapBLevelRight"), -1.0f, 1.0f, dbSigned);
        apply("Glide FX", "B Dly R", pid("glideTapBDelayRight"), 0.0f, 0.042f, ms);
        apply("Glide FX", "Fbk L", pid("glideFeedbackLeft"), -1.0f, 1.0f, bipolarPercent);
        apply("Glide FX", "Fbk R", pid("glideFeedbackRight"), -1.0f, 1.0f, bipolarPercent);
        apply("Glide FX", "X-Fbk L", pid("glideCrossFeedbackLeft"), -1.0f, 1.0f, bipolarPercent);
        apply("Glide FX", "X-Fbk R", pid("glideCrossFeedbackRight"), -1.0f, 1.0f, bipolarPercent);

        static constexpr const char* kVoiceLabels[6] = { "Voice1", "Voice2", "Voice3",
                                                           "Voice4", "Voice5", "Voice6" };
        for (int i = 0; i < 6; ++i)
        {
            apply("Levels", kVoiceLabels[i], voiceParamId("glideHall", i, "Level"), -1.0f, 1.0f, dbSigned);
            apply("DelayTime", kVoiceLabels[i], voiceParamId("glideHall", i, "Delay"), 0.0f, 10.581f, ms);
            apply("Panning", kVoiceLabels[i], voiceParamId("glideHall", i, "Pan"), -1.0f, 1.0f, direct);
            apply("Feedback", "V" + juce::String(i + 1) + " Fbk", voiceParamId("glideHall", i, "Feedback"),
                  -1.0f, 1.0f, bipolarPercent);
            apply("Feedback", "V" + juce::String(i + 1) + " X-Fbk",
                  voiceParamId("glideHall", i, "CrossFeedback"), -1.0f, 1.0f, bipolarPercent);
        }
    }

  private:
    static float direct_(const pcm80::Field& f) { return static_cast<float>(*f.numeric); }

    dsp::graphs::GlideHallAlgorithm engine_;
};
}

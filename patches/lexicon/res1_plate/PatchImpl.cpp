#include "Patch.h"

#include "dsp/graphs/Res1PlateAlgorithm.h"

// Lexicon PCM81-inspired Res1>Plate algorithm for the Polyend Endless: six
// chromatically, directly-tuned resonator voices excited by the live
// input, feeding a fixed Plate reverb in series. See
// dsp/include/dsp/graphs/Res1PlateAlgorithm.h.
//
// A fixed default 6-voice chord (a spread major-ish triad across two
// octaves, gentle stereo spread) is set up in init() so the effect is
// immediately usable from 3 knobs; the JUCE plugin exposes every voice's
// Pitch/Level/Pan/Duration/HiCut individually.
//
// Knob mapping:
//   Left  - Duration: all-voice sustain time, from a short pluck-like
//           decay to a long, chord-organ-like ring.
//   Mid   - FX Mix: balance of the raw resonator signal vs. the
//           reverberated signal.
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle freeze (sustain whatever is currently ringing in the
//           reverb tank forever).
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::Res1PlateAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "Res1PlateAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::Res1PlateAlgorithm::requiredWorkingBufferSize()));
    }

    void processAudio(std::span<float> audioBufferLeft, std::span<float> audioBufferRight) override
    {
        if (bypassed_)
        {
            return;
        }
        engine_.process(audioBufferLeft, audioBufferRight);
    }

    ParameterMetadata getParameterMetadata(int paramIdx) override
    {
        switch (static_cast<endless::ParamId>(paramIdx))
        {
            case endless::ParamId::kParamLeft:
                return ParameterMetadata{ 0.0f, 1.0f, 0.4f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 1.0f, 0.5f };
            case endless::ParamId::kParamRight:
                return ParameterMetadata{ 0.0f, 1.0f, 0.4f };
        }
        return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
    }

    void setParamValue(int paramIdx, float value) override
    {
        switch (static_cast<endless::ParamId>(paramIdx))
        {
            case endless::ParamId::kParamLeft:
            {
                // 0.15s .. 6s sustain across all six voices.
                auto durationSeconds = 0.15f + value * 5.85f;
                for (int i = 0; i < 6; ++i)
                {
                    engine_.setVoiceDuration(i, durationSeconds);
                }
                break;
            }
            case endless::ParamId::kParamMid:
                engine_.setFxMix(value);
                break;
            case endless::ParamId::kParamRight:
                engine_.setMix(value);
                break;
        }
    }

    void handleAction(int actionIdx) override
    {
        switch (static_cast<endless::ActionId>(actionIdx))
        {
            case endless::ActionId::kLeftFootSwitchPress:
                bypassed_ = !bypassed_;
                break;
            case endless::ActionId::kLeftFootSwitchHold:
                frozen_ = !frozen_;
                engine_.setFrozen(frozen_);
                break;
        }
    }

    Color getStateLedColor() override
    {
        if (bypassed_)
        {
            return Color::kDimWhite;
        }
        return frozen_ ? Color::kLightYellow : Color::kDarkCobalt;
    }

    void init() override
    {
        bypassed_ = false;
        frozen_ = false;

        // A spread chord across two octaves: root-third-fifth-octave-tenth-
        // twelfth (C3-E3-G3-C4-E4-G4 at A440 12-TET), each panned to its
        // own place in the stereo field per the manual's Voices 1-3
        // left / 4-6 right convention.
        static constexpr float kVoiceHz[6] = { 130.81f, 164.81f, 196.00f, 261.63f, 329.63f, 392.00f };
        static constexpr float kVoicePans[6] = { -0.7f, -0.35f, -0.85f, 0.7f, 0.35f, 0.85f };
        for (int i = 0; i < 6; ++i)
        {
            engine_.setVoicePitch(i, kVoiceHz[i]);
            engine_.setVoiceLevel(i, 0.5f);
            engine_.setVoicePan(i, kVoicePans[i]);
            engine_.setVoiceDuration(i, 3.0f);
            engine_.setVoiceHiCut(i, 4000.0f);
        }

        engine_.setDecaySeconds(2.0f);
        engine_.setSize(0.5f);
        engine_.setDiffusion(0.6f);
        engine_.setVoiceDiffusion(0.2f);
    }

  private:
    dsp::graphs::Res1PlateAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

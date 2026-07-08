#include "Patch.h"

#include "dsp/graphs/MBandRvbAlgorithm.h"

// Lexicon PCM81-inspired M-Band+Rvb algorithm for the Polyend Endless: a
// 6-voice multiband EQ'd delay (feedback re-entering through the
// diffuser each pass) running in parallel with a Chamber reverb. See
// dsp/include/dsp/graphs/MBandRvbAlgorithm.h.
//
// A fixed default 6-voice pattern (musically-spaced delay times, gentle
// per-voice band-pass spread, stereo split) is set up in init() so the
// effect is immediately usable from 3 knobs; the JUCE plugin exposes
// every voice's filters individually.
//
// Knob mapping:
//   Left  - Voice feedback (applied to all six voices together): more
//           feedback means more diffuse, filtered repeats.
//   Mid   - FX Mix: balance of the six-voice multiband signal vs. the
//           Chamber reverb.
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
        static_assert(dsp::graphs::MBandRvbAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "MBandRvbAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::MBandRvbAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 0.8f, 0.25f };
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
                for (int i = 0; i < 6; ++i)
                {
                    engine_.setVoiceFeedback(i, value);
                }
                break;
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

        static constexpr float kVoiceDelays[6] = { 0.09f, 0.17f, 0.26f, 0.11f, 0.19f, 0.28f };
        static constexpr float kVoicePans[6] = { -0.7f, -0.4f, -0.15f, 0.15f, 0.4f, 0.7f };
        static constexpr float kVoiceHiCut[6] = { 7000.0f, 5500.0f, 4000.0f, 7200.0f, 5700.0f, 4200.0f };
        static constexpr float kVoiceLoCut[6] = { 120.0f, 250.0f, 500.0f, 130.0f, 260.0f, 520.0f };
        for (int i = 0; i < 6; ++i)
        {
            engine_.setVoiceDelay(i, kVoiceDelays[i]);
            engine_.setVoiceLevel(i, 0.55f);
            engine_.setVoicePan(i, kVoicePans[i]);
            engine_.setVoiceFeedback(i, 0.25f);
            engine_.setVoiceHiCut(i, kVoiceHiCut[i]);
            engine_.setVoiceLoCut(i, kVoiceLoCut[i]);
        }

        engine_.setDiffusion(0.5f);
        engine_.setDecaySeconds(2.5f);
        engine_.setSize(0.6f);
    }

  private:
    dsp::graphs::MBandRvbAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

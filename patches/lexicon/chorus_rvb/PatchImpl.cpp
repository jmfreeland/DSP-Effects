#include "Patch.h"

#include "dsp/graphs/ChorusRvbAlgorithm.h"

// Lexicon PCM81-inspired Chorus+Rvb algorithm for the Polyend Endless: a
// 6-voice stereo chorus running in parallel with a Plate reverb. See
// dsp/include/dsp/graphs/ChorusRvbAlgorithm.h.
//
// A fixed default 6-voice chorus pattern (musically-spaced delay times,
// gentle per-voice depth/rate spread, stereo split) is set up in init()
// so the effect is immediately usable from 3 knobs; the JUCE plugin
// exposes every voice individually.
//
// Knob mapping:
//   Left  - Chorus Master Depth (0-200%): scales every voice's own
//           modulation depth together.
//   Mid   - FX Mix: balance of the six-voice chorus vs. the Plate reverb.
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
        static_assert(dsp::graphs::ChorusRvbAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "ChorusRvbAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::ChorusRvbAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 200.0f, 100.0f };
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
                engine_.setChorusMaster(value, 100.0f);
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

        static constexpr float kVoiceDelays[6] = { 0.02f, 0.035f, 0.05f, 0.025f, 0.04f, 0.055f };
        static constexpr float kVoicePans[6] = { -0.7f, -0.4f, -0.15f, 0.15f, 0.4f, 0.7f };
        static constexpr float kVoiceDepths[6] = { 12.0f, 18.0f, 24.0f, 14.0f, 20.0f, 26.0f };
        static constexpr float kVoiceRates[6] = { 0.25f, 0.31f, 0.19f, 0.28f, 0.22f, 0.34f };
        for (int i = 0; i < 6; ++i)
        {
            engine_.setVoiceDelay(i, kVoiceDelays[i]);
            engine_.setVoiceLevel(i, 0.5f);
            engine_.setVoicePan(i, kVoicePans[i]);
            engine_.setVoiceFeedback(i, 0.1f);
            engine_.setVoiceChorus(i, kVoiceDepths[i], kVoiceRates[i]);
        }

        engine_.setDiffusion(0.5f);
        engine_.setChorusHighCut(10000.0f);
        engine_.setDecaySeconds(2.2f);
        engine_.setSize(0.6f);
    }

  private:
    dsp::graphs::ChorusRvbAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

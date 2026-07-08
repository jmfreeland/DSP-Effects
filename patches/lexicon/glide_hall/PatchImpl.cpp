#include "Patch.h"

#include "dsp/graphs/GlideHallAlgorithm.h"

// Lexicon PCM81-inspired Glide>Hall algorithm for the Polyend Endless: a
// stereo pair of gliding delay taps feeding six delay voices in series
// with a Concert Hall reverb. See
// dsp/include/dsp/graphs/GlideHallAlgorithm.h.
//
// A fixed default 6-voice pattern (musically-spaced delay times, gentle
// stereo spread, modest feedback) is set up in init() so the effect is
// immediately usable from 3 knobs; the JUCE plugin exposes every voice
// and the glide taps individually.
//
// Knob mapping:
//   Left  - Glide amount: sweeps both A/B glide tap delay times together,
//           from a fast flange/pitch-modulation character (short taps)
//           to a slower chorus-like smear (longer taps).
//   Mid   - FX Mix: balance of the raw six-voice signal vs. the
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
        static_assert(dsp::graphs::GlideHallAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "GlideHallAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::GlideHallAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 1.0f, 0.3f };
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
                // 0.3ms .. 40ms, matching the manual's own 0-42ms A/B range.
                auto delaySeconds = 0.0003f + value * 0.0397f;
                engine_.setGlideTapALeft(0.7f, delaySeconds);
                engine_.setGlideTapARight(0.7f, delaySeconds * 1.05f);
                engine_.setGlideTapBLeft(0.5f, delaySeconds * 1.8f);
                engine_.setGlideTapBRight(0.5f, delaySeconds * 1.9f);
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

        engine_.setGlideLevel(0.8f);
        engine_.setGlideFeedback(0.15f, 0.15f);
        engine_.setGlideCrossFeedback(0.1f, 0.1f);

        static constexpr float kVoiceDelays[6] = { 0.12f, 0.24f, 0.36f, 0.15f, 0.27f, 0.39f };
        static constexpr float kVoicePans[6] = { -0.6f, -0.3f, -0.8f, 0.6f, 0.3f, 0.8f };
        for (int i = 0; i < 6; ++i)
        {
            engine_.setVoiceDelay(i, kVoiceDelays[i]);
            engine_.setVoiceLevel(i, 0.5f);
            engine_.setVoicePan(i, kVoicePans[i]);
            engine_.setVoiceFeedback(i, 0.15f);
            engine_.setVoiceCrossFeedback(i, 0.03f);
        }

        engine_.setDecaySeconds(2.5f);
        engine_.setSize(0.5f);
        engine_.setDiffusion(0.6f);
        engine_.setVoiceDiffusion(0.3f);
    }

  private:
    dsp::graphs::GlideHallAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

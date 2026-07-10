#include "Patch.h"

#include "dsp/graphs/QuadHallAlgorithm.h"

// Lexicon PCM81-inspired Quad>Hall algorithm for the Polyend Endless: a
// 4-voice pitch shifter (Voices 1-2 from the Left input, 3-4 from the
// Right) in series with a Concert Hall reverb. See
// dsp/include/dsp/graphs/QuadHallAlgorithm.h.
//
// A fixed default voice spread (a detuned-doubling quartet, per the
// manual's own "doubling (or quintupling) effects" use case) is set up
// in init(); Left scales that spread symmetrically wider/narrower. The
// JUCE plugin exposes every voice's Delay/Pitch/Level/Pan/Feedback
// individually.
//
// Knob mapping:
//   Left  - Spread: scales the 4 voices' pitch-shift amounts symmetrically
//           from unison (0) out to +-1 octave.
//   Mid   - FX Mix: balance of the raw shifted signal vs. the
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
        static_assert(dsp::graphs::QuadHallAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "QuadHallAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::QuadHallAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 1.0f, 0.6f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 1.0f, 0.35f };
            case endless::ParamId::kParamRight:
                return ParameterMetadata{ 0.0f, 1.0f, 0.6f };
        }
        return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
    }

    void setParamValue(int paramIdx, float value) override
    {
        switch (static_cast<endless::ParamId>(paramIdx))
        {
            case endless::ParamId::kParamLeft:
            {
                // 0 (unison, all voices silent-detune) .. 1 (+-1 octave
                // spread), scaling the default quartet's own cents ratios.
                auto cents = value * 1200.0f;
                engine_.setVoice(0, 0.02f, cents * 0.58f, 0.5f, -0.6f);
                engine_.setVoice(1, 0.03f, -cents * 0.58f, 0.5f, -0.3f);
                engine_.setVoice(2, 0.02f, cents * 0.58f, 0.5f, 0.3f);
                engine_.setVoice(3, 0.03f, -cents * 0.58f, 0.5f, 0.6f);
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

        engine_.setVoiceFeedback(0, 0.0f, 0.0f);
        engine_.setVoiceFeedback(1, 0.0f, 0.0f);
        engine_.setVoiceFeedback(2, 0.0f, 0.0f);
        engine_.setVoiceFeedback(3, 0.0f, 0.0f);
        engine_.setSpliceSeconds(0.004f);
        engine_.setDecaySeconds(2.5f);
        engine_.setSize(0.5f);
        engine_.setDiffusion(0.6f);
    }

  private:
    dsp::graphs::QuadHallAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

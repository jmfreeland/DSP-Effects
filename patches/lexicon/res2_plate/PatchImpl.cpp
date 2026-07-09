#include "Patch.h"

#include "dsp/graphs/Res2PlateAlgorithm.h"

// Lexicon PCM81-inspired Res2>Plate algorithm for the Polyend Endless: six
// diatonically pitch-tracked resonator voices excited by the live input,
// feeding a fixed Plate reverb in series. See
// dsp/include/dsp/graphs/Res2PlateAlgorithm.h.
//
// A fixed default chord voicing (Third/Fifth/Octave/Second/Sixth/Seventh
// up from the tracked note, gentle stereo spread) is set up in init() so
// the effect is immediately usable from 3 knobs; the JUCE plugin exposes
// every voice's HarmonicInterval/Level/Pan/Duration/HiCut individually.
//
// Knob mapping:
//   Left  - Key: the tonic the tracked note is diatonically interpreted
//           against (C through B, chromatic steps).
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
        static_assert(dsp::graphs::Res2PlateAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "Res2PlateAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::Res2PlateAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
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
                // 0..11: C through B, chromatic steps.
                auto key = static_cast<int>(value * 11.999f);
                engine_.setKey(key);
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

        engine_.setScale(dsp::Scale::kMajor);
        engine_.setKey(0);

        static constexpr dsp::HarmonicInterval kVoiceIntervals[6] = {
            dsp::HarmonicInterval::kThirdUp,  dsp::HarmonicInterval::kFifthUp,
            dsp::HarmonicInterval::kOctaveUp, dsp::HarmonicInterval::kSecondUp,
            dsp::HarmonicInterval::kSixthUp,  dsp::HarmonicInterval::kSeventhUp,
        };
        static constexpr float kVoicePans[6] = { -0.7f, -0.35f, -0.85f, 0.7f, 0.35f, 0.85f };
        for (int i = 0; i < 6; ++i)
        {
            engine_.setVoiceInterval(i, kVoiceIntervals[i]);
            engine_.setVoiceLevel(i, 0.5f);
            engine_.setVoicePan(i, kVoicePans[i]);
            engine_.setVoiceDuration(i, 2.5f);
            engine_.setVoiceHiCut(i, 4000.0f);
        }

        engine_.setDecaySeconds(2.0f);
        engine_.setSize(0.5f);
        engine_.setDiffusion(0.6f);
        engine_.setVoiceDiffusion(0.2f);
    }

  private:
    dsp::graphs::Res2PlateAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

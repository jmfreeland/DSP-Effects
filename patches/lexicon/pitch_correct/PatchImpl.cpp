#include "Patch.h"

#include "dsp/graphs/PitchCorrectAlgorithm.h"

// Lexicon PCM81-inspired Pitch Correct algorithm for the Polyend Endless:
// a pitch detector driving a corrective PitchShifter (nearest chromatic
// semitone relative to Tuning), in series with a fixed Chamber reverb
// (FX Mix default near 0% per the manual - "most applications require
// only pitch processing"). See
// dsp/include/dsp/graphs/PitchCorrectAlgorithm.h.
//
// Knob mapping:
//   Left  - Correction amount: 0 (no correction) .. 100% (fully corrected
//           to the nearest chromatic semitone).
//   Mid   - FX Mix: dry corrected signal .. reverbed signal.
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle Tracking between Fastest and Hold ("effectively
//           turning any melody into a pedal tone," per the manual).
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::PitchCorrectAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "PitchCorrectAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::PitchCorrectAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 1.0f, 1.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
            case endless::ParamId::kParamRight:
                return ParameterMetadata{ 0.0f, 1.0f, 1.0f };
        }
        return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
    }

    void setParamValue(int paramIdx, float value) override
    {
        switch (static_cast<endless::ParamId>(paramIdx))
        {
            case endless::ParamId::kParamLeft:
                engine_.setCorrection(value);
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
                held_ = !held_;
                engine_.setTracking(held_ ? dsp::graphs::PitchCorrectAlgorithm::Tracking::kHold
                                           : dsp::graphs::PitchCorrectAlgorithm::Tracking::kFastest);
                break;
        }
    }

    Color getStateLedColor() override
    {
        if (bypassed_)
        {
            return Color::kDimWhite;
        }
        return held_ ? Color::kLightYellow : Color::kDarkCobalt;
    }

    void init() override
    {
        bypassed_ = false;
        held_ = false;

        engine_.setPitchRange(80.0f, 800.0f);
        engine_.setTuning(440.0f);
        engine_.setTracking(dsp::graphs::PitchCorrectAlgorithm::Tracking::kFastest);
        engine_.setShiftCents(0.0f);
        engine_.setShiftSemitones(0);
        engine_.setDecaySeconds(2.0f);
        engine_.setSize(0.5f);
        engine_.setDiffusion(0.6f);
    }

  private:
    dsp::graphs::PitchCorrectAlgorithm engine_;
    bool bypassed_ = false;
    bool held_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

#include "Patch.h"

#include "dsp/graphs/StereoChmbAlgorithm.h"

// Lexicon PCM81-inspired Stereo-Chmb algorithm for the Polyend Endless: a
// Submixer routing a Chamber reverb against a "Stereo Shifter" FX block -
// one shared pitch-shift amount applied sample-synchronously to both
// channels (reusing dsp::algorithms::StereoShift, built for the
// Eventide H3000's own Algorithm 103), preserving the input's stereo
// image rather than treating L/R as independent voices. See
// dsp/include/dsp/graphs/StereoChmbAlgorithm.h.
//
// Knob mapping:
//   Left  - Shift: pitch shift amount, +-1 octave.
//   Mid   - FX Mix: balance of dry vs. shifted signal within the Stereo
//           Shifter block itself.
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle Routing between Parallel (reverb and shifter run
//           independently) and Rvb->FX series (the shifter processes
//           the reverb's own tail).
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::StereoChmbAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "StereoChmbAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::StereoChmbAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 1.0f, 0.7f };
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
                // 0 (-1 octave) .. 0.5 (unison) .. 1 (+1 octave).
                engine_.setShiftCents((value - 0.5f) * 2400.0f);
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
                seriesRouting_ = !seriesRouting_;
                engine_.setRouting(seriesRouting_ ? dsp::graphs::StereoChmbAlgorithm::Routing::kRvbIntoFx
                                                   : dsp::graphs::StereoChmbAlgorithm::Routing::kParallel);
                break;
        }
    }

    Color getStateLedColor() override
    {
        if (bypassed_)
        {
            return Color::kDimWhite;
        }
        return seriesRouting_ ? Color::kLightYellow : Color::kDarkCobalt;
    }

    void init() override
    {
        bypassed_ = false;
        seriesRouting_ = false;

        engine_.setSends(dsp::Submixer::Sends::kStereo);
        engine_.setReturns(dsp::Submixer::Returns::kStereo);
        engine_.setRouting(dsp::graphs::StereoChmbAlgorithm::Routing::kParallel);
        engine_.setRvbMix(0.8f);
        engine_.setShiftDelaySeconds(0.02f);
        engine_.setShiftFeedback(0.0f);
        engine_.setSpliceSeconds(0.004f);
        engine_.setDecaySeconds(2.0f);
        engine_.setSize(0.5f);
        engine_.setDiffusion(0.6f);
    }

  private:
    dsp::graphs::StereoChmbAlgorithm engine_;
    bool bypassed_ = false;
    bool seriesRouting_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

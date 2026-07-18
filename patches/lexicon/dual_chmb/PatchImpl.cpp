#include "Patch.h"

#include "dsp/graphs/DualChmbAlgorithm.h"

#include <cmath>

// Lexicon PCM81-inspired Dual-Chmb algorithm for the Polyend Endless: a
// Submixer routing a Chamber reverb against a 2-voice "Dual Shifter" FX
// block, in parallel by default. See
// dsp/include/dsp/graphs/DualChmbAlgorithm.h.
//
// The pedal's 3 knobs cover the most common adjustments; Sends/Returns
// (which stereo-imaging configuration feeds each block) stay at the
// manual's own default "Stereo" position, and the JUCE plugin exposes
// the full Submixer parameter set including every Sends/Returns option.
//
// Knob mapping:
//   Left  - Spread: scales the 2 voices' pitch-shift amounts symmetrically
//           from unison (0) out to +-1 octave.
//   Mid   - FX Mix: balance of dry vs. shifted signal within the Dual
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
        static_assert(dsp::graphs::DualChmbAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "DualChmbAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::DualChmbAlgorithm::requiredWorkingBufferSize()));
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
            {
                // 0 (unison) .. 1 (+-1 octave spread), curved (^4) so the
                // knob's middle range stays in doubling territory instead
                // of racing past it to dissonant multi-semitone detune.
                auto cents = 1200.0f * std::pow(value, 4.0f);
                engine_.setVoice(0, 0.02f, cents, 0.7f, -0.7f);
                engine_.setVoice(1, 0.03f, -cents, 0.7f, 0.7f);
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
                seriesRouting_ = !seriesRouting_;
                engine_.setRouting(seriesRouting_ ? dsp::graphs::DualChmbAlgorithm::Routing::kRvbIntoFx
                                                   : dsp::graphs::DualChmbAlgorithm::Routing::kParallel);
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
        engine_.setRouting(dsp::graphs::DualChmbAlgorithm::Routing::kParallel);
        engine_.setRvbMix(0.8f);
        engine_.setVoiceFeedback(0, 0.0f, 0.0f);
        engine_.setVoiceFeedback(1, 0.0f, 0.0f);
        engine_.setSpliceSeconds(0.004f);
        engine_.setDecaySeconds(2.0f);
        engine_.setSize(0.5f);
        engine_.setDiffusion(0.5f);
    }

  private:
    dsp::graphs::DualChmbAlgorithm engine_;
    bool bypassed_ = false;
    bool seriesRouting_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

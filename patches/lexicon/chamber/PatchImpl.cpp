#include "Patch.h"

#include "dsp/graphs/ChamberAlgorithm.h"

// Lexicon PCM81-inspired Chamber algorithm for the Polyend Endless: the
// Chamber reverb (Shape+Spread onset envelope + EkoDly/EkoFbk pre-echo on
// top of the shared reverb core) plus its 4-Voice "Reverb Shell" (parallel
// delay voices, post-delay, FX Mix/Width/Hi-Cut/Adjust). See
// dsp/include/dsp/graphs/ChamberAlgorithm.h.
//
// Knob mapping:
//   Left  - Decay time (RT60), ~0.3s to 8s.
//   Mid   - Shape (onset envelope contour; Spread stays at the Graph's
//           default since the hardware only has 3 knobs).
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle freeze (sustain whatever is currently ringing forever).
//
// The Voice/post-delay/FX-chain layer runs at the graph's own defaults
// (a modest slapback layered under the reverb) since the hardware only
// exposes 3 knobs; the JUCE plugin exposes the full parameter set.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::ChamberAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "ChamberAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(kSampleRate,
                         std::span<float>(buffer.data(),
                                           dsp::graphs::ChamberAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.3f, 8.0f, 2.8f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 1.0f, 0.3f };
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
                engine_.setDecaySeconds(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setShape(value);
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
    }

  private:
    dsp::graphs::ChamberAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

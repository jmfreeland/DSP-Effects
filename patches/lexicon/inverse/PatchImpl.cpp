#include "Patch.h"

#include "dsp/graphs/InverseAlgorithm.h"

// Lexicon PCM81-inspired Inverse algorithm for the Polyend Endless: the
// Inverse reverb (Duration + independent Low Slope/Mid Slope replacing
// RT60 decay - decay, gate, or rise) plus its 4-Voice "Reverb Shell"
// (parallel delay voices, post-delay, FX Mix/Width/Hi-Cut/Adjust). See
// dsp/include/dsp/graphs/InverseAlgorithm.h.
//
// Knob mapping:
//   Left  - Duration (time from onset to the hard cutoff), ~0.05s to 8s.
//   Mid   - Slope, applied to both Low and Mid Slope together since the
//           hardware only has 3 knobs (-1 natural decay tail .. 0 gate
//           .. +1 inverse/rise - the manual's own sign convention).
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle freeze (sustain whatever is currently ringing forever;
//           inherited generically from ReverbCore, same as every other
//           core here, though the manual doesn't call it out for
//           Inverse specifically the way it does for Infinite).
//
// The Voice/post-delay/FX-chain layer runs at the graph's own defaults
// (a modest slapback layered under the reverb) since the hardware only
// exposes 3 knobs; the JUCE plugin exposes the full parameter set
// (independent Low Slope/Mid Slope).
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::InverseAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "InverseAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(),
                            dsp::graphs::InverseAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.05f, 8.0f, 1.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ -1.0f, 1.0f, -0.3f };
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
                engine_.setDuration(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setLowSlope(value);
                engine_.setMidSlope(value);
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
    dsp::graphs::InverseAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

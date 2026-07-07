#include "Patch.h"

#include "dsp/graphs/SweptCombsAlgorithm.h"

// Eventide H3000-inspired Swept Combs algorithm for the Polyend Endless
// (Algorithm 105): six independently-swept feedback delay lines panned
// into a stereo mixer. See dsp/graphs/SweptCombsAlgorithm.h and
// docs/eventide-swept-combs.md.
//
// Knob mapping (the hardware's own "Quickset" master controls, minus
// Rate/Depth/Width, which stay at their built-in defaults - the JUCE
// plugin exposes all five masters plus the full per-line "Tedium" set):
//   Left  - Master Delay: scales all six lines' delay times together.
//   Mid   - Master Feedback: scales all six lines' feedback together.
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle Repeat: mutes new input into the six lines so
//           whatever's currently recirculating keeps repeating
//           indefinitely, matching the manual's own Repeat control.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::SweptCombsAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "SweptCombsAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::SweptCombsAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ -1.0f, 1.0f, 1.0f };
            case endless::ParamId::kParamRight:
                return ParameterMetadata{ 0.0f, 1.0f, 0.5f };
        }
        return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
    }

    void setParamValue(int paramIdx, float value) override
    {
        switch (static_cast<endless::ParamId>(paramIdx))
        {
            case endless::ParamId::kParamLeft:
                engine_.setMasterDelay(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setMasterFeedback(value);
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
                repeating_ = !repeating_;
                engine_.setRepeat(repeating_);
                break;
        }
    }

    Color getStateLedColor() override
    {
        if (bypassed_)
        {
            return Color::kDimWhite;
        }
        return repeating_ ? Color::kLightYellow : Color::kDarkCobalt;
    }

    void init() override
    {
        bypassed_ = false;
        repeating_ = false;
    }

  private:
    dsp::graphs::SweptCombsAlgorithm engine_;
    bool bypassed_ = false;
    bool repeating_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

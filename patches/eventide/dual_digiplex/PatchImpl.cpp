#include "Patch.h"

#include "dsp/graphs/DualDigiplexAlgorithm.h"

// Eventide H3000-inspired Dual Digiplex algorithm for the Polyend
// Endless (Algorithm 110): two independent delay lines (up to 0.7s
// each), one per channel. See dsp/graphs/DualDigiplexAlgorithm.h and
// docs/eventide-dual-digiplex.md.
//
// Knob mapping:
//   Left  - Left channel delay, 0-0.7s.
//   Mid   - Right channel delay, 0-0.7s (independent of Left).
//   Right - Dry/wet mix (applied to both channels equally). Feedback
//           stays at its built-in default since the hardware only has
//           3 knobs - the JUCE plugin exposes independent Left/Right
//           Feedback.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle Repeat: captures the current delay content on both
//           channels and replays it continuously, matching the manual's
//           own Repeat control.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::DualDigiplexAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "DualDigiplexAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::DualDigiplexAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 0.7f, 0.2f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 0.7f, 0.35f };
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
                engine_.setLeftDelaySeconds(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setRightDelaySeconds(value);
                break;
            case endless::ParamId::kParamRight:
                engine_.setLeftMix(value);
                engine_.setRightMix(value);
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
        engine_.setLeftFeedback(0.3f);
        engine_.setRightFeedback(0.3f);
    }

  private:
    dsp::graphs::DualDigiplexAlgorithm engine_;
    bool bypassed_ = false;
    bool repeating_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

#include "Patch.h"

#include "dsp/graphs/SweptReverbAlgorithm.h"

// Eventide H3000-inspired Swept Reverb algorithm for the Polyend Endless
// (Algorithm 106): six independently-swept delay lines feeding a
// Householder-mixed feedback network. See
// dsp/graphs/SweptReverbAlgorithm.h and docs/eventide-swept-reverb.md.
//
// Knob mapping:
//   Left  - Feedback: the reverb's decay length.
//   Mid   - Master Delay: scales all six lines' delay times together.
//   Right - Dry/wet mix. Rate/Depth stay at their built-in defaults
//           since the hardware only has 3 knobs - the JUCE plugin
//           exposes both Masters plus the full per-line Tedium set.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle Repeat: mutes new input into the network so whatever
//           is currently recirculating keeps repeating indefinitely,
//           matching the manual's own Repeat control.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::SweptReverbAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "SweptReverbAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::SweptReverbAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ -1.0f, 1.0f, 0.7f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 1.0f, 1.0f };
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
                engine_.setFeedback(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setMasterDelay(value);
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
    dsp::graphs::SweptReverbAlgorithm engine_;
    bool bypassed_ = false;
    bool repeating_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

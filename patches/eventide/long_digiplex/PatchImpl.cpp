#include "Patch.h"

#include "dsp/graphs/LongDigiplexAlgorithm.h"

// Eventide H3000-inspired Long Digiplex algorithm for the Polyend
// Endless (Algorithm 109): a single long delay line (up to 1.4s) with
// feedback. See dsp/graphs/LongDigiplexAlgorithm.h and
// docs/eventide-long-digiplex.md.
//
// Knob mapping:
//   Left  - Delay time, 0-1.4s.
//   Mid   - Feedback.
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle Repeat: captures the current delay content and
//           replays it continuously, muting new input, matching the
//           manual's own Repeat control.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::LongDigiplexAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "LongDigiplexAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::LongDigiplexAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 1.4f, 0.3f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ -1.0f, 0.99f, 0.3f };
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
                engine_.setDelaySeconds(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setFeedback(value);
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
    dsp::graphs::LongDigiplexAlgorithm engine_;
    bool bypassed_ = false;
    bool repeating_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

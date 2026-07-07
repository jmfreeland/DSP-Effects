#include "Patch.h"

#include "dsp/graphs/ReverseShiftAlgorithm.h"

// Eventide H3000-inspired Reverse Shift algorithm for the Polyend
// Endless (Algorithm 104): a "tape reverse" splice generator - records a
// settable-length segment and plays back the previous one time-reversed,
// with an additional pitch shift layered on top. See
// dsp/graphs/ReverseShiftAlgorithm.h and docs/eventide-reverse-shift.md.
//
// Knob mapping:
//   Left  - Splice Length, shared by both channels, 20ms..1s.
//   Mid   - Feedback: shared level for both voices' cascading repeats.
//   Right - Dry/wet mix (applied to both channels equally). Pitch shift
//           is fixed at 0 cents (pure reversal, no extra pitch change)
//           since the hardware only has 3 knobs - the JUCE plugin
//           exposes independent Left/Right Length and cents.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle "freeze": latches Feedback near 1 so the reversed
//           cascade rings indefinitely, the same footswitch-hold freeze
//           idea used elsewhere in this archive.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::ReverseShiftAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "ReverseShiftAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::ReverseShiftAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.02f, 1.0f, 0.15f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
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
                engine_.setLeftLengthSeconds(value);
                engine_.setRightLengthSeconds(value);
                break;
            case endless::ParamId::kParamMid:
                normalFeedback_ = value;
                if (!frozen_)
                {
                    engine_.setLeftFeedback(normalFeedback_);
                    engine_.setRightFeedback(normalFeedback_);
                }
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
                frozen_ = !frozen_;
                if (frozen_)
                {
                    engine_.setLeftFeedback(0.97f);
                    engine_.setRightFeedback(0.97f);
                }
                else
                {
                    engine_.setLeftFeedback(normalFeedback_);
                    engine_.setRightFeedback(normalFeedback_);
                }
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
    dsp::graphs::ReverseShiftAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
    float normalFeedback_ = 0.0f;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

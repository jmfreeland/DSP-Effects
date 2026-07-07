#include "Patch.h"

#include "dsp/graphs/DualShiftAlgorithm.h"

// Eventide H3000-inspired Dual Shift algorithm for the Polyend Endless
// (Algorithm 102): two completely independent, fixed-interval pitch
// shifters, one per channel - unlike Layered Shift there's no shared
// input or feedback point at all. See dsp/graphs/DualShiftAlgorithm.h
// and docs/eventide-dual-shift.md.
//
// Knob mapping:
//   Left  - Left channel shift, -1200..+1200 cents.
//   Mid   - Right channel shift, -1200..+1200 cents (independent of
//           Left, since the two channels never interact on real
//           hardware either).
//   Right - Dry/wet mix (applied to both channels equally). Feedback is
//           fixed off by default (only reachable via the footswitch-hold
//           freeze below, or the JUCE plugin's independent controls),
//           since the hardware only has 3 knobs.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle "freeze": latches both channels' Feedback near 1 so
//           each rings indefinitely, the same footswitch-hold freeze
//           idea used elsewhere in this archive.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::DualShiftAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "DualShiftAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::DualShiftAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ -1200.0f, 1200.0f, -1200.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ -1200.0f, 1200.0f, 1200.0f };
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
                engine_.setLeftCents(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setRightCents(value);
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
                    engine_.setLeftFeedback(0.0f);
                    engine_.setRightFeedback(0.0f);
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
    dsp::graphs::DualShiftAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

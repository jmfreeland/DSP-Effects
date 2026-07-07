#include "Patch.h"

#include "dsp/graphs/LayeredShiftAlgorithm.h"

// Eventide H3000-inspired Layered Shift algorithm for the Polyend Endless
// (Algorithm 101): two independent, fixed-interval pitch shifters driven
// from the left input alone - no pitch tracking, unlike Diatonic Shift.
// See dsp/graphs/LayeredShiftAlgorithm.h and docs/eventide-layered-shift.md.
//
// Knob mapping:
//   Left  - Left Voice shift, -1200..+1200 cents (an octave down to an
//           octave up). Right Voice trails a fixed minor 3rd (+300 cents)
//           above Left Voice for a simple built-in two-part harmony,
//           since the hardware only has 3 knobs (the JUCE plugin exposes
//           independent Left/Right Voice cents).
//   Mid   - Feedback: shared level for both voices' cascading repeats.
//   Right - Dry/wet mix (applied to both channels equally).
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle "freeze": latches Feedback near 1 so the harmony
//           cascade rings indefinitely, the same footswitch-hold freeze
//           idea used elsewhere in this archive.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::LayeredShiftAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "LayeredShiftAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::LayeredShiftAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ -1200.0f, 1200.0f, 400.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 1.0f, 0.3f };
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
                engine_.setRightCents(value + 300.0f > 1200.0f ? 1200.0f : value + 300.0f);
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
    dsp::graphs::LayeredShiftAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
    float normalFeedback_ = 0.3f;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

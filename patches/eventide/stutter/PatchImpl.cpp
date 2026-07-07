#include "Patch.h"

#include "dsp/graphs/StutterAlgorithm.h"

// Eventide H3000-inspired Stutter algorithm for the Polyend Endless
// (Algorithm 112): a real-time "st..st..stutter" performance effect. See
// dsp/graphs/StutterAlgorithm.h and docs/eventide-stutter.md. The
// hardware's 3 knobs/2 footswitch actions can't drive the manual's full
// trigger-patch system, so this Patch exposes the stutter shape
// directly and a manual trigger; the JUCE plugin exposes the full
// parameter set including both sweep generators and Auto mode.
//
// Knob mapping:
//   Left  - Stutter Length (0-0.5s): how long a captured window is.
//   Mid   - Stutter Count (0-16): how many times the window repeats.
//   Right - Dry/wet mix, applied to both channels equally.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - manually trigger the stutter (the algorithm's core gesture).
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::StutterAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "StutterAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(kSampleRate,
                         std::span<float>(buffer.data(), dsp::graphs::StutterAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 0.5f, 0.1f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 16.0f, 4.0f };
            case endless::ParamId::kParamRight:
                return ParameterMetadata{ 0.0f, 1.0f, 1.0f };
        }
        return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
    }

    void setParamValue(int paramIdx, float value) override
    {
        switch (static_cast<endless::ParamId>(paramIdx))
        {
            case endless::ParamId::kParamLeft:
                engine_.setLength1(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setCount1(static_cast<int>(value + 0.5f));
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
                engine_.triggerStutter1();
                break;
        }
    }

    Color getStateLedColor() override { return bypassed_ ? Color::kDimWhite : Color::kDarkCobalt; }

    void init() override { bypassed_ = false; }

  private:
    dsp::graphs::StutterAlgorithm engine_;
    bool bypassed_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

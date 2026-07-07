#include "Patch.h"

#include "dsp/graphs/TimesqueezeAlgorithm.h"

// Eventide H3000-inspired Timesqueeze algorithm for the Polyend Endless
// (Algorithm 113): a tape-speed pitch-correction utility - Time% is
// converted to the speed ratio a connected tape machine would be told
// to run at, and both channels are shifted by its inverse, times an
// independent Pitch trim. See dsp/graphs/TimesqueezeAlgorithm.h and
// docs/eventide-timesqueeze.md. Only 2 real parameters exist on this
// algorithm's own manual page (no Mix control), so the Right knob is
// unused here.
//
// Knob mapping:
//   Left  - Time (-87.5% to 100%): the tape-speed change to compensate
//           for. 0% = no shift.
//   Mid   - Pitch trim (0.001 to 2.000): an independent secondary shift
//           ratio, 1.0 = no additional shift.
//   Right - Unused (no third parameter exists on this algorithm).
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - reset Pitch trim to 1.0 (undo any detune).
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::TimesqueezeAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "TimesqueezeAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::TimesqueezeAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ -87.5f, 100.0f, 0.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.001f, 2.0f, 1.0f };
            case endless::ParamId::kParamRight:
                return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
        }
        return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
    }

    void setParamValue(int paramIdx, float value) override
    {
        switch (static_cast<endless::ParamId>(paramIdx))
        {
            case endless::ParamId::kParamLeft:
                engine_.setTimePercent(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setPitchRatio(value);
                break;
            case endless::ParamId::kParamRight:
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
                engine_.setPitchRatio(1.0f);
                break;
        }
    }

    Color getStateLedColor() override { return bypassed_ ? Color::kDimWhite : Color::kDarkCobalt; }

    void init() override { bypassed_ = false; }

  private:
    dsp::graphs::TimesqueezeAlgorithm engine_;
    bool bypassed_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

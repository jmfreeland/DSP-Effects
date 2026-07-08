#include "Patch.h"

#include "dsp/graphs/MultiShiftAlgorithm.h"

// Eventide H3000-inspired Multi-Shift algorithm for the Polyend Endless
// (Algorithm 116): two independent pitch-shift channels (each with its
// own dry delay tap and patchable feedback), micro-pitch-shift-optimized.
// See dsp/graphs/MultiShiftAlgorithm.h and docs/eventide-multi-shift.md.
//
// Knob mapping:
//   Left  - Left channel Coarse/Fine, -3600 to +3600 cents.
//   Mid   - Right channel Coarse/Fine, -3600 to +3600 cents.
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle both channels' Direction between Forward and Reverse
//           together.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::MultiShiftAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "MultiShiftAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::MultiShiftAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ -3600.0f, 3600.0f, 0.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ -3600.0f, 3600.0f, 0.0f };
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
                engine_.setLeftCents(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setRightCents(value);
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
                reversed_ = !reversed_;
                engine_.setLeftDirection(reversed_);
                engine_.setRightDirection(reversed_);
                break;
        }
    }

    Color getStateLedColor() override
    {
        if (bypassed_)
        {
            return Color::kDimWhite;
        }
        return reversed_ ? Color::kLightYellow : Color::kDarkCobalt;
    }

    void init() override
    {
        bypassed_ = false;
        reversed_ = false;
    }

  private:
    dsp::graphs::MultiShiftAlgorithm engine_;
    bool bypassed_ = false;
    bool reversed_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

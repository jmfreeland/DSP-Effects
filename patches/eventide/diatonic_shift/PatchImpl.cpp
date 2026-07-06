#include "Patch.h"

#include "dsp/graphs/DiatonicShiftAlgorithm.h"

// Eventide H3000-inspired Diatonic Shift algorithm for the Polyend
// Endless: a diatonically-quantized pitch shifter with a regenerating
// feedback loop that cascades into an ascending/descending arpeggio. See
// dsp/graphs/DiatonicShiftAlgorithm.h.
//
// Knob mapping:
//   Left  - Scale Degree: how many diatonic steps to shift per repeat,
//           -7 (a 7th down) .. +7 (a 7th up). Scale is fixed to Major
//           since the hardware only has 3 knobs.
//   Mid   - Regen: feedback level for the cascading repeats.
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle "freeze": latches Regen near 1 so the cascade rings
//           indefinitely instead of decaying, the same footswitch-hold
//           freeze idea used by every Lexicon core here.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::DiatonicShiftAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "DiatonicShiftAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(),
                            dsp::graphs::DiatonicShiftAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ -7.0f, 7.0f, 2.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 1.0f, 0.5f };
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
                engine_.setScaleDegree(static_cast<int>(value >= 0.0f ? value + 0.5f : value - 0.5f));
                break;
            case endless::ParamId::kParamMid:
                normalRegen_ = value;
                if (!frozen_)
                {
                    engine_.setRegen(normalRegen_);
                }
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
                frozen_ = !frozen_;
                engine_.setRegen(frozen_ ? 0.97f : normalRegen_);
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
    dsp::graphs::DiatonicShiftAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
    float normalRegen_ = 0.5f;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

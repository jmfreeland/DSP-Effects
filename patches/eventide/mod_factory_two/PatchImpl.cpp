#include "Patch.h"

#include "dsp/graphs/ModFactoryTwoAlgorithm.h"

// Eventide H3000-inspired "mod factory|two" algorithm for the Polyend
// Endless (Algorithm 123): a smaller cousin to mod factory|one - 2
// filtered delays, 2 detuning pitch shifters, one LFO, one envelope/
// ducker detector, 2 amplitude modulators, 4 mixers, wired by a settable
// 28x22 patch matrix. See dsp/algorithms/ModFactoryTwo.h and
// docs/eventide-mod-factory-two.md. The hardware's 3 knobs obviously
// can't reach the full patch matrix, so this Patch ships with the
// manual's own suggested chorus recipe wired by default (Left Input
// split between both detuners, shifted +/-10 cents, mixed back
// together). The JUCE plugin exposes the entire patch matrix as 28
// source dropdowns.
//
// Knob mapping:
//   Left  - Detune amount (symmetric, +/- this many cents).
//   Mid   - Splice Length (chorus smoothness vs. glitchiness).
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle between a subtle (+/-10 cent) and a wide (+/-40 cent)
//            detune preset.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::ModFactoryTwoAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "ModFactoryTwoAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::ModFactoryTwoAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 50.0f, 10.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 10.0f, 400.0f, 150.0f };
            case endless::ParamId::kParamRight:
                return ParameterMetadata{ 0.0f, 100.0f, 100.0f };
        }
        return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
    }

    void setParamValue(int paramIdx, float value) override
    {
        switch (static_cast<endless::ParamId>(paramIdx))
        {
            case endless::ParamId::kParamLeft:
                engine_.setDetuneCents(0, -value);
                engine_.setDetuneCents(1, value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setDetuneSpliceLengthMs(0, value);
                engine_.setDetuneSpliceLengthMs(1, value);
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
                wide_ = !wide_;
                engine_.setDetuneCents(0, wide_ ? -40.0f : -10.0f);
                engine_.setDetuneCents(1, wide_ ? 40.0f : 10.0f);
                break;
        }
    }

    Color getStateLedColor() override { return bypassed_ ? Color::kDimWhite : Color::kDarkCobalt; }

    void init() override
    {
        bypassed_ = false;
        wide_ = false;
    }

  private:
    dsp::graphs::ModFactoryTwoAlgorithm engine_;
    bool bypassed_ = false;
    bool wide_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

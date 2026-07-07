#include "Patch.h"

#include "dsp/graphs/ReverbFactoryAlgorithm.h"

// Eventide H3000-inspired Reverb Factory algorithm for the Polyend
// Endless (Algorithm 107): a six-line Householder reverb network with a
// dynamics Gate that crossfades decay time and tone between On
// (loud/above threshold) and Off (soft/below) settings. See
// dsp/graphs/ReverbFactoryAlgorithm.h and
// docs/eventide-reverb-factory.md.
//
// Knob mapping:
//   Left  - On Decay: the reverb tail length while the Gate is open.
//   Mid   - Gate Threshold: how loud the input must get to trigger the
//           longer On decay (higher = harder to trigger).
//   Right - Dry/wet mix. Off Decay/EQ/Gate Time/Speed stay at their
//           built-in defaults since the hardware only has 3 knobs - the
//           JUCE plugin exposes the full parameter set.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle Gate Enable: when disabled, the reverb always uses
//           the On decay/EQ settings, matching the manual.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::ReverbFactoryAlgorithm::requiredWorkingBufferSize() <=
                        kWorkingBufferSize,
                      "ReverbFactoryAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(kSampleRate,
                         std::span<float>(buffer.data(),
                                           dsp::graphs::ReverbFactoryAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.1f, 10.0f, 2.5f };
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
                engine_.setOnDecaySeconds(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setGateThreshold(value);
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
                gateEnabled_ = !gateEnabled_;
                engine_.setGateEnabled(gateEnabled_);
                break;
        }
    }

    Color getStateLedColor() override
    {
        if (bypassed_)
        {
            return Color::kDimWhite;
        }
        return gateEnabled_ ? Color::kDarkCobalt : Color::kLightYellow;
    }

    void init() override
    {
        bypassed_ = false;
        gateEnabled_ = true;
    }

  private:
    dsp::graphs::ReverbFactoryAlgorithm engine_;
    bool bypassed_ = false;
    bool gateEnabled_ = true;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

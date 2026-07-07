#include "Patch.h"

#include "dsp/graphs/UltraTapAlgorithm.h"

// Eventide H3000-inspired Ultra-Tap algorithm for the Polyend Endless
// (Algorithm 108): a 4-stage Allpass diffusor feeding a 12-tap
// cumulative delay line. See dsp/graphs/UltraTapAlgorithm.h and
// docs/eventide-ultra-tap.md.
//
// Knob mapping:
//   Left  - Length: scales all 12 tap delay times together.
//   Mid   - Diffusion: the 4-stage Allpass diffusor amount.
//   Right - Dry/wet mix. Width/Feedback stay at their built-in defaults
//           since the hardware only has 3 knobs - the JUCE plugin
//           exposes the full Tedium set (per-tap Delay/Level/Pan) plus
//           the Quickset Spacing/Weights/Pans shape generators.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle "freeze": latches Feedback near 1 so the tap field
//           rings indefinitely, the same footswitch-hold freeze idea
//           used elsewhere in this archive.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::UltraTapAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "UltraTapAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::UltraTapAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 1.0f, 1.0f };
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
                engine_.setLength(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setDiffusion(value);
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
                engine_.setFeedback(frozen_ ? -0.97f : 0.0f);
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
    dsp::graphs::UltraTapAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

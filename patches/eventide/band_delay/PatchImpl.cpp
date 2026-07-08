#include "Patch.h"

#include "dsp/graphs/BandDelayAlgorithm.h"

// Eventide H3000-inspired Band Delay algorithm for the Polyend Endless
// (Algorithm 117): a multi-tap delay line whose eight taps each feed
// their own bandpass filter, output level, and pan. See
// dsp/graphs/BandDelayAlgorithm.h and docs/eventide-band-delay.md.
//
// Knob mapping:
//   Left  - Global Delay, 0-100%: scales all eight tap delay times.
//   Mid   - Feedback, -100% to 100%.
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle Global Frequency between 0 and +12 semitones (an
//           "octave up" preset for all eight filters at once).
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::BandDelayAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "BandDelayAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::BandDelayAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 100.0f, 100.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ -100.0f, 100.0f, 0.0f };
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
                engine_.setGlobalDelay(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setFeedback(value);
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
                octaveUp_ = !octaveUp_;
                engine_.setGlobalFrequency(octaveUp_ ? 12.0f : 0.0f);
                break;
        }
    }

    Color getStateLedColor() override
    {
        if (bypassed_)
        {
            return Color::kDimWhite;
        }
        return octaveUp_ ? Color::kLightYellow : Color::kDarkCobalt;
    }

    void init() override
    {
        bypassed_ = false;
        octaveUp_ = false;
    }

  private:
    dsp::graphs::BandDelayAlgorithm engine_;
    bool bypassed_ = false;
    bool octaveUp_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

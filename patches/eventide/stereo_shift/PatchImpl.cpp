#include "Patch.h"

#include "dsp/graphs/StereoShiftAlgorithm.h"

// Eventide H3000-inspired Stereo Shift algorithm for the Polyend Endless
// (Algorithm 103): a true stereo pitch shifter where one shared set of
// controls drives both channels identically. See
// dsp/graphs/StereoShiftAlgorithm.h and docs/eventide-stereo-shift.md.
//
// Knob mapping:
//   Left  - Shared shift, -1200..+1200 cents.
//   Mid   - Shared Feedback.
//   Right - Shared dry/wet mix. Delay is fixed at a reasonable default
//           (the JUCE plugin exposes it directly), since the hardware
//           only has 3 knobs.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle "freeze": latches Feedback near 1 so the shift rings
//           indefinitely, the same footswitch-hold freeze idea used
//           elsewhere in this archive.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::StereoShiftAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "StereoShiftAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::StereoShiftAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ -1200.0f, 1200.0f, 700.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
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
                engine_.setCents(value);
                break;
            case endless::ParamId::kParamMid:
                normalFeedback_ = value;
                if (!frozen_)
                {
                    engine_.setFeedback(normalFeedback_);
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
                engine_.setFeedback(frozen_ ? 0.97f : normalFeedback_);
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
    dsp::graphs::StereoShiftAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
    float normalFeedback_ = 0.0f;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

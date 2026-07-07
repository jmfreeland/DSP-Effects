#include "Patch.h"

#include "dsp/graphs/DiatonicShiftAlgorithm.h"

// Eventide H3000-inspired Diatonic Shift algorithm for the Polyend
// Endless: a real-time pitch-tracking harmonizer that plays the
// diatonically-correct interval above or below whatever note is playing.
// See dsp/graphs/DiatonicShiftAlgorithm.h.
//
// Knob mapping:
//   Left  - Left Voice interval, -7 (a 7th down) .. +7 (a 7th up) scale
//           steps from the tracked note. Key/Scale fixed to C Major and
//           Right Voice fixed a 5th above Left Voice, since the hardware
//           only has 3 knobs (the JUCE plugin exposes independent
//           Left/Right Voice, Key, and Scale).
//   Mid   - Feedback: shared level for both voices' cascading repeats.
//   Right - Dry/wet mix (applied to both channels equally).
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle "freeze": latches Feedback near 1 so the harmony
//           cascade rings indefinitely, the same footswitch-hold freeze
//           idea used by every Lexicon core here.
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
            {
                auto degree = static_cast<int>(value >= 0.0f ? value + 0.5f : value - 0.5f);
                auto leftInterval = dsp::harmonicIntervalFromDegreeOffset(degree);
                engine_.setLeftVoice(leftInterval);
                // Right Voice trails a 5th above Left Voice for a simple
                // built-in two-part harmony, clamped to stay in range.
                auto rightDegree = degree + 4 > 7 ? 7 : degree + 4;
                engine_.setRightVoice(dsp::harmonicIntervalFromDegreeOffset(rightDegree));
                break;
            }
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
    dsp::graphs::DiatonicShiftAlgorithm engine_;
    bool bypassed_ = false;
    bool frozen_ = false;
    float normalFeedback_ = 0.3f;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

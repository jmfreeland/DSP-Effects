#include "Patch.h"

#include "dsp/graphs/DenseRoomAlgorithm.h"

// Eventide H3000-inspired Dense Room algorithm for the Polyend Endless
// (Algorithm 114): a denser evolution of Reverb Factory's 6-line tank
// with an added Diffusion stage and explicit per-line Pan/Level. See
// dsp/graphs/DenseRoomAlgorithm.h and docs/eventide-dense-room.md.
//
// Knob mapping:
//   Left  - Rev Time, 0.1 to 10s (the manual's own range goes to
//           "infinity"; capped here to a practical sweep).
//   Mid   - Size, 0-100%.
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle Position between front (0) and rear (1) - a discrete
//           "swap listener seat" gesture.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::DenseRoomAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "DenseRoomAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::DenseRoomAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.1f, 10.0f, 2.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 1.0f, 0.7f };
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
                engine_.setRevTimeSeconds(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setSize(value);
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
                rearPosition_ = !rearPosition_;
                engine_.setPosition(rearPosition_ ? 1.0f : 0.0f);
                break;
        }
    }

    Color getStateLedColor() override
    {
        if (bypassed_)
        {
            return Color::kDimWhite;
        }
        return rearPosition_ ? Color::kLightYellow : Color::kDarkCobalt;
    }

    void init() override
    {
        bypassed_ = false;
        rearPosition_ = false;
    }

  private:
    dsp::graphs::DenseRoomAlgorithm engine_;
    bool bypassed_ = false;
    bool rearPosition_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

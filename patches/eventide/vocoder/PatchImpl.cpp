#include "Patch.h"

#include "dsp/graphs/VocoderAlgorithm.h"

// Eventide H3000-inspired Vocoder algorithm for the Polyend Endless
// (Algorithm 115): impresses the articulatory characteristics of one
// input onto the timbre/pitch of another. See
// dsp/graphs/VocoderAlgorithm.h and docs/eventide-vocoder.md. Per the
// manual's own channel convention: Left In = synthesis (instrument),
// Right In = analysis (voice) - get this right or the effect won't
// track anything.
//
// Knob mapping:
//   Left  - Formant Shift, 0-100%: "munchkin-izes" the vocoded sound at
//           high settings.
//   Mid   - Envelope Speed, 0-100%: how quickly the effect tracks the
//           analysis input's articulation.
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle pseudo-stereo Depth between 0 (mono) and 1 (full).
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::VocoderAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "VocoderAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::VocoderAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 100.0f, 0.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 100.0f, 50.0f };
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
                engine_.setFormantShift(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setEnvelopeSpeed(value);
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
                stereo_ = !stereo_;
                engine_.setDepth(stereo_ ? 1.0f : 0.0f);
                break;
        }
    }

    Color getStateLedColor() override
    {
        if (bypassed_)
        {
            return Color::kDimWhite;
        }
        return stereo_ ? Color::kLightYellow : Color::kDarkCobalt;
    }

    void init() override
    {
        bypassed_ = false;
        stereo_ = false;
        engine_.setDepth(0.0f);
    }

  private:
    dsp::graphs::VocoderAlgorithm engine_;
    bool bypassed_ = false;
    bool stereo_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

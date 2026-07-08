#include "Patch.h"

#include "dsp/graphs/PhaserAlgorithm.h"

// Eventide H3000-inspired Phaser algorithm for the Polyend Endless
// (Algorithm 119): a mono-in, stereo-out phase shifter - twelve allpass
// filters in series, swept by an LFO, an envelope follower, or an ADSR,
// mixed with the dry signal to produce moving notches. See
// dsp/algorithms/Phaser.h and docs/eventide-phaser.md. The hardware's 3
// knobs can't reach the ADSR's own rates/thresholds or the Envelope
// Channel sidechain option; the JUCE plugin exposes the full set.
//
// Knob mapping:
//   Left  - Mix: how much phase-shifted signal blends with dry.
//   Mid   - Sweep Rate: how fast the LFO sweeps the notches.
//   Right - Feedback: how resonant the allpass loop is.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle the sweep source between LFO and Envelope Follower
//            (an "auto-wah into phaser" character switch reachable
//            without the ADSR's extra parameters).
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::PhaserAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "PhaserAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(kSampleRate,
                         std::span<float>(buffer.data(), dsp::graphs::PhaserAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 100.0f, 50.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 100.0f, 50.0f };
            case endless::ParamId::kParamRight:
                return ParameterMetadata{ -100.0f, 100.0f, 0.0f };
        }
        return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
    }

    void setParamValue(int paramIdx, float value) override
    {
        switch (static_cast<endless::ParamId>(paramIdx))
        {
            case endless::ParamId::kParamLeft:
                engine_.setMix(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setSweepRate(value);
                break;
            case endless::ParamId::kParamRight:
                engine_.setFeedback(value);
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
                envelopeMode_ = !envelopeMode_;
                engine_.setSweepMode(envelopeMode_ ? dsp::algorithms::Phaser::SweepMode::kEnvelope
                                                    : dsp::algorithms::Phaser::SweepMode::kLfo);
                break;
        }
    }

    Color getStateLedColor() override { return bypassed_ ? Color::kDimWhite : Color::kDarkCobalt; }

    void init() override
    {
        bypassed_ = false;
        envelopeMode_ = false;
    }

  private:
    dsp::graphs::PhaserAlgorithm engine_;
    bool bypassed_ = false;
    bool envelopeMode_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

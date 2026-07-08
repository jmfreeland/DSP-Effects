#include "Patch.h"

#include "dsp/graphs/ModFactoryOneAlgorithm.h"

// Eventide H3000-inspired "mod factory|one" algorithm for the Polyend
// Endless (Algorithm 122): a genuine modular patch-bay (2 delays, 2
// filters, 2 LFOs, 2 envelope/ducker detectors, 2 amplitude modulators,
// 4 mixers, wired by a settable 28x26 patch matrix). See
// dsp/algorithms/ModFactoryOne.h and docs/eventide-mod-factory-one.md.
// The hardware's 3 knobs obviously can't reach the full patch matrix, so
// this Patch ships with the module doc's own suggested "manual flanger"
// patch wired up by default (LFO 1 sweeps Delay 1, mixed with dry
// through Mixer 1). The JUCE plugin exposes the entire patch matrix as
// 28 source dropdowns, matching the Patch Factory precedent.
//
// Knob mapping:
//   Left  - LFO 1 Rate (the flange sweep speed).
//   Mid   - Delay 1 Mod (the flange depth, in ms).
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle Delay 1 Feedback between 0% and 40% (a more resonant,
//            "jet flanger" character switch).
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::ModFactoryOneAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "ModFactoryOneAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::ModFactoryOneAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.02f, 5.0f, 0.3f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 20.0f, 6.0f };
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
                engine_.setLfoFrequency(0, value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setDelayModMs(0, value);
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
                resonant_ = !resonant_;
                engine_.setDelayFeedback(0, resonant_ ? 40.0f : 0.0f);
                break;
        }
    }

    Color getStateLedColor() override { return bypassed_ ? Color::kDimWhite : Color::kDarkCobalt; }

    void init() override
    {
        bypassed_ = false;
        resonant_ = false;

        using MF1 = dsp::algorithms::ModFactoryOne;
        engine_.setPatch(MF1::Destination::kDly1In, MF1::Source::kLeftInput);
        engine_.setPatch(MF1::Destination::kDly1Mod, MF1::Source::kLfo1);
        engine_.setDelayMs(0, 8.0f);
        engine_.setLfoWaveform(0, dsp::MultiWaveLFO::Waveform::kTriangle);
        engine_.setPatch(MF1::Destination::kMix1aIn, MF1::Source::kLeftInput);
        engine_.setPatch(MF1::Destination::kMix1bIn, MF1::Source::kDelay1);
        engine_.setMixAAmount(0, 50.0f);
        engine_.setMixBAmount(0, 50.0f);
        engine_.setPatch(MF1::Destination::kLeftOut, MF1::Source::kMixer1);
        engine_.setPatch(MF1::Destination::kRightOut, MF1::Source::kMixer1);
    }

  private:
    dsp::graphs::ModFactoryOneAlgorithm engine_;
    bool bypassed_ = false;
    bool resonant_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

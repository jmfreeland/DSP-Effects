#include "Patch.h"

#include "dsp/graphs/StringModellerAlgorithm.h"

// Eventide H3000-inspired String Modeller algorithm for the Polyend
// Endless (Algorithm 118): six Karplus-Strong string resonators, excited
// continuously by filtered noise and/or the live input, plus a manual
// "pluck" gesture standing in for the manual's MIDI note-on triggering.
// See dsp/algorithms/StringModeller.h and docs/eventide-string-modeller.md.
// The hardware's 3 knobs can't reach the per-string Note tuning (left at
// standard guitar open-string tuning) or the stimulation filter's
// Freq/Qfac/High-Band-Low Amt balance; the JUCE plugin exposes the full
// set.
//
// Knob mapping:
//   Left  - Pitch: global tuning offset for all six strings.
//   Mid   - Decay: how long the plucked/excited strings keep ringing.
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - manually "pluck" all six strings (the algorithm's core gesture,
//            since no MIDI keyboard exists to trigger them).
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::StringModellerAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "StringModellerAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::StringModellerAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ -100.0f, 100.0f, 0.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 100.0f, 60.0f };
            case endless::ParamId::kParamRight:
                return ParameterMetadata{ 0.0f, 100.0f, 60.0f };
        }
        return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
    }

    void setParamValue(int paramIdx, float value) override
    {
        switch (static_cast<endless::ParamId>(paramIdx))
        {
            case endless::ParamId::kParamLeft:
                engine_.setPitch(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setDecay(value);
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
                engine_.trigger();
                break;
        }
    }

    Color getStateLedColor() override { return bypassed_ ? Color::kDimWhite : Color::kDarkCobalt; }

    void init() override
    {
        bypassed_ = false;
        // A guitar-pedal-friendly default: the strings sing on their own
        // (BandAmt from the Block's own default) and also resonate with
        // whatever's plugged in, per the manual's own "Interesting Ideas"
        // sympathetic-resonator use.
        engine_.setInAmt(30.0f);
    }

  private:
    dsp::graphs::StringModellerAlgorithm engine_;
    bool bypassed_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

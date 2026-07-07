#include "Patch.h"

#include "dsp/graphs/PatchFactoryAlgorithm.h"

// Eventide H3000-inspired Patch Factory algorithm for the Polyend
// Endless (Algorithm 111): a modular patch-bay (noise gen, two
// switchable-tap filters, two delay lines, one pitch shifter, two
// scalers, two summing junctions) wired by a settable patch matrix. See
// dsp/graphs/PatchFactoryAlgorithm.h and docs/eventide-patch-factory.md.
// The hardware's 3 knobs can't drive a full patch matrix, so this Patch
// leaves the factory-default patch in place and exposes the parameters
// most likely to be reached for on a physical unit; the JUCE plugin
// exposes the full Cutoff/Q/Delay/Scale/Shift parameter set.
//
// Knob mapping:
//   Left  - Filter Cutoff, drives both Filter 1 and Filter 2 together.
//   Mid   - Pitch Shift amount (the "Shift" element feeding Filter 2 /
//           Right Output in the default patch), -48 to +12 semitones.
//   Right - Dry/wet mix, applied to both channels equally.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - toggle "self-oscillate" mode: pushes both filters' Q toward
//           1 (the manual's own documented self-oscillation point) for a
//           dramatic ringing/drone character, off by default.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::PatchFactoryAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "PatchFactoryAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::PatchFactoryAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ 0.0f, 7000.0f, 1000.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ -4800.0f, 1200.0f, 0.0f };
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
                engine_.setCutoff1(value);
                engine_.setCutoff2(value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setShiftCents(value);
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
                selfOscillate_ = !selfOscillate_;
                engine_.setQ1(selfOscillate_ ? 0.98f : 0.0f);
                engine_.setQ2(selfOscillate_ ? 0.98f : 0.0f);
                break;
        }
    }

    Color getStateLedColor() override
    {
        if (bypassed_)
        {
            return Color::kDimWhite;
        }
        return selfOscillate_ ? Color::kRed : Color::kDarkCobalt;
    }

    void init() override
    {
        bypassed_ = false;
        selfOscillate_ = false;
    }

  private:
    dsp::graphs::PatchFactoryAlgorithm engine_;
    bool bypassed_ = false;
    bool selfOscillate_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

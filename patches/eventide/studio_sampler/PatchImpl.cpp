#include "Patch.h"

#include "dsp/graphs/StudioSamplerAlgorithm.h"

// Eventide H3000-inspired Studio Sampler algorithm for the Polyend
// Endless (Algorithm 120/121): two fully independent per-channel
// samplers, record on command, play back with independent Pitch and
// Time control. See dsp/algorithms/StudioSampler.h and
// docs/eventide-studio-sampler.md. The hardware's 3 knobs can't reach
// per-channel Attack/Release, Start/End range, loop, or audio-trigger
// threshold; the JUCE plugin exposes the full set.
//
// Knob mapping (applies to both channels together):
//   Left  - Pitch (-1200 to 1200 cents).
//   Mid   - Time (0-800%).
//   Right - Dry/wet mix.
//
// Footswitch:
//   Press - toggle bypass.
//   Hold  - a single-button looper-pedal gesture cycling both channels
//            together: idle -> record; recording -> stop then
//            immediately play; playing -> stop.
class PatchImpl : public Patch
{
  public:
    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        static_assert(dsp::graphs::StudioSamplerAlgorithm::requiredWorkingBufferSize() <= kWorkingBufferSize,
                      "StudioSamplerAlgorithm needs more working buffer than the Patch provides");
        engine_.prepare(
          kSampleRate,
          std::span<float>(buffer.data(), dsp::graphs::StudioSamplerAlgorithm::requiredWorkingBufferSize()));
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
                return ParameterMetadata{ -1200.0f, 1200.0f, 0.0f };
            case endless::ParamId::kParamMid:
                return ParameterMetadata{ 0.0f, 800.0f, 100.0f };
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
                engine_.setPitchCents(0, value);
                engine_.setPitchCents(1, value);
                break;
            case endless::ParamId::kParamMid:
                engine_.setTimePercent(0, value);
                engine_.setTimePercent(1, value);
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
                if (engine_.isPlaying(0) || engine_.isPlaying(1))
                {
                    engine_.stop(0);
                    engine_.stop(1);
                }
                else if (engine_.isRecording(0) || engine_.isRecording(1))
                {
                    engine_.stop(0);
                    engine_.stop(1);
                    engine_.play(0);
                    engine_.play(1);
                }
                else
                {
                    engine_.record(0);
                    engine_.record(1);
                }
                break;
        }
    }

    Color getStateLedColor() override { return bypassed_ ? Color::kDimWhite : Color::kDarkCobalt; }

    void init() override { bypassed_ = false; }

  private:
    dsp::graphs::StudioSamplerAlgorithm engine_;
    bool bypassed_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}

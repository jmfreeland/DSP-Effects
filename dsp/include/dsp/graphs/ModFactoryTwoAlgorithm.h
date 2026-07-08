#pragma once

#include "dsp/algorithms/ModFactoryTwo.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "mod factory|two" algorithm: the
 * ModFactoryTwo Block (see dsp/algorithms/ModFactoryTwo.h) plus
 * independent Left/Right input trim.
 */
class ModFactoryTwoAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::ModFactoryTwo::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        engine_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- ModFactoryTwo Block pass-throughs --
    void setPatch(dsp::algorithms::ModFactoryTwo::Destination destination,
                   dsp::algorithms::ModFactoryTwo::Source source)
    {
        engine_.setPatch(destination, source);
    }
    void setBpm(float bpm) { engine_.setBpm(bpm); }
    void setModKnob(float percent0to100) { engine_.setModKnob(percent0to100); }
    void setMix(float percent0to100) { engine_.setMix(percent0to100); }
    void setDelayMs(int delay, float ms) { engine_.setDelayMs(delay, ms); }
    void setDelayBpmBeats(int delay, float beats24) { engine_.setDelayBpmBeats(delay, beats24); }
    void setDelayFeedback(int delay, float percent) { engine_.setDelayFeedback(delay, percent); }
    void setDelayLoop(int delay, bool loop) { engine_.setDelayLoop(delay, loop); }
    void setDelayModMs(int delay, float ms) { engine_.setDelayModMs(delay, ms); }
    void setDelayHighcutHz(int delay, float hz) { engine_.setDelayHighcutHz(delay, hz); }
    void setDelayHighcutModHz(int delay, float hz) { engine_.setDelayHighcutModHz(delay, hz); }
    void setDetuneCents(int detuner, float cents) { engine_.setDetuneCents(detuner, cents); }
    void setDetuneDelayMs(int detuner, float ms) { engine_.setDetuneDelayMs(detuner, ms); }
    void setDetuneBpmBeats(int detuner, float beats24) { engine_.setDetuneBpmBeats(detuner, beats24); }
    void setDetuneModAmountCents(int detuner, float cents) { engine_.setDetuneModAmountCents(detuner, cents); }
    void setDetuneSpliceLengthMs(int detuner, float ms) { engine_.setDetuneSpliceLengthMs(detuner, ms); }
    void setLfoFrequency(float hz) { engine_.setLfoFrequency(hz); }
    void setLfoBpmBeats(float beats24) { engine_.setLfoBpmBeats(beats24); }
    void setLfoWaveform(dsp::MultiWaveLFO::Waveform waveform) { engine_.setLfoWaveform(waveform); }
    void setLfoThresholdDb(float db) { engine_.setLfoThresholdDb(db); }
    void setLfoModAmount(float hz) { engine_.setLfoModAmount(hz); }
    void setEnvAttackMs(float ms) { engine_.setEnvAttackMs(ms); }
    void setEnvDecayMs(float ms) { engine_.setEnvDecayMs(ms); }
    void setEnvThresholdDb(float db) { engine_.setEnvThresholdDb(db); }
    void setEnvRatio(float ratioToOne) { engine_.setEnvRatio(ratioToOne); }
    void setAmpModAmount(int am, float percent) { engine_.setAmpModAmount(am, percent); }
    void setAmpModOffset(int am, float percent) { engine_.setAmpModOffset(am, percent); }
    void setMixAAmount(int mixer, float percent) { engine_.setMixAAmount(mixer, percent); }
    void setMixBAmount(int mixer, float percent) { engine_.setMixBAmount(mixer, percent); }
    void setModScaleAmount(int scaler, float percent) { engine_.setModScaleAmount(scaler, percent); }

    // -- Input conditioning --
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    void reset() { engine_.reset(); }

    void process(std::span<float> left, std::span<float> right)
    {
        for (std::size_t n = 0; n < left.size(); ++n)
        {
            processSample(left[n], right[n]);
        }
    }

    void processSample(float& left, float& right)
    {
        left *= inLevelLeft_;
        right *= inLevelRight_;
        engine_.processSample(left, right);
    }

  private:
    dsp::algorithms::ModFactoryTwo engine_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}

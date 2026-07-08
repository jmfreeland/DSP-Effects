#pragma once

#include "dsp/algorithms/ModFactoryOne.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "mod factory|one" algorithm: the
 * ModFactoryOne Block (see dsp/algorithms/ModFactoryOne.h) plus
 * independent Left/Right input trim.
 */
class ModFactoryOneAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::ModFactoryOne::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        engine_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- ModFactoryOne Block pass-throughs --
    void setPatch(dsp::algorithms::ModFactoryOne::Destination destination,
                   dsp::algorithms::ModFactoryOne::Source source)
    {
        engine_.setPatch(destination, source);
    }
    void setBpm(float bpm) { engine_.setBpm(bpm); }
    void setKnob1(float percent0to100) { engine_.setKnob1(percent0to100); }
    void setKnob2(float percent0to100) { engine_.setKnob2(percent0to100); }
    void setMix(float percent0to100) { engine_.setMix(percent0to100); }
    void setFilterCutoff(int filter, float hz) { engine_.setFilterCutoff(filter, hz); }
    void setFilterQ(int filter, float q1to1000) { engine_.setFilterQ(filter, q1to1000); }
    void setFilterType(int filter, dsp::algorithms::ModFactoryOne::FilterType type)
    {
        engine_.setFilterType(filter, type);
    }
    void setFilterModAmount(int filter, float hz) { engine_.setFilterModAmount(filter, hz); }
    void setDelayMs(int delay, float ms) { engine_.setDelayMs(delay, ms); }
    void setDelayBpmBeats(int delay, float beats24) { engine_.setDelayBpmBeats(delay, beats24); }
    void setDelayFeedback(int delay, float percent) { engine_.setDelayFeedback(delay, percent); }
    void setDelayLoop(int delay, bool loop) { engine_.setDelayLoop(delay, loop); }
    void setDelayModMs(int delay, float ms) { engine_.setDelayModMs(delay, ms); }
    void setLfoFrequency(int lfo, float hz) { engine_.setLfoFrequency(lfo, hz); }
    void setLfoBpmBeats(int lfo, float beats24) { engine_.setLfoBpmBeats(lfo, beats24); }
    void setLfoWaveform(int lfo, dsp::MultiWaveLFO::Waveform waveform) { engine_.setLfoWaveform(lfo, waveform); }
    void setLfoThresholdDb(int lfo, float db) { engine_.setLfoThresholdDb(lfo, db); }
    void setLfoModAmount(int lfo, float hz) { engine_.setLfoModAmount(lfo, hz); }
    void setEnvAttackMs(int env, float ms) { engine_.setEnvAttackMs(env, ms); }
    void setEnvDecayMs(int env, float ms) { engine_.setEnvDecayMs(env, ms); }
    void setEnvThresholdDb(int env, float db) { engine_.setEnvThresholdDb(env, db); }
    void setEnvRatio(int env, float ratioToOne) { engine_.setEnvRatio(env, ratioToOne); }
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
    dsp::algorithms::ModFactoryOne engine_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}

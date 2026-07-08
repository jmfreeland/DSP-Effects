#pragma once

#include "dsp/algorithms/StudioSampler.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Studio Sampler" algorithm (Algorithm
 * 120/121): the StudioSampler Block (see dsp/algorithms/StudioSampler.h)
 * plus independent Left/Right input trim, matching its two fully
 * independent per-channel samplers.
 */
class StudioSamplerAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::StudioSampler::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        engine_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- StudioSampler Block pass-throughs --
    void setPitchCents(int channel, float cents) { engine_.setPitchCents(channel, cents); }
    void setTimePercent(int channel, float percent) { engine_.setTimePercent(channel, percent); }
    void setAttackSeconds(int channel, float seconds) { engine_.setAttackSeconds(channel, seconds); }
    void setReleaseSeconds(int channel, float seconds) { engine_.setReleaseSeconds(channel, seconds); }
    void setStartFraction(int channel, float frac0to1) { engine_.setStartFraction(channel, frac0to1); }
    void setEndFraction(int channel, float frac0to1) { engine_.setEndFraction(channel, frac0to1); }
    void setLoop(int channel, bool loop) { engine_.setLoop(channel, loop); }
    void setShiftMode(int channel, dsp::SamplerVoice::ShiftMode mode) { engine_.setShiftMode(channel, mode); }
    void setTriggerMode(int channel, dsp::SamplerVoice::TriggerMode mode)
    {
        engine_.setTriggerMode(channel, mode);
    }
    void setThreshold(int channel, float level0to1) { engine_.setThreshold(channel, level0to1); }
    void record(int channel) { engine_.record(channel); }
    void stop(int channel) { engine_.stop(channel); }
    void play(int channel) { engine_.play(channel); }
    bool isRecording(int channel) const { return engine_.isRecording(channel); }
    bool isPlaying(int channel) const { return engine_.isPlaying(channel); }
    void setMix(float percent0to100) { engine_.setMix(percent0to100); }

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
    dsp::algorithms::StudioSampler engine_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}

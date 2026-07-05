#pragma once

#include "dsp/Comb.h"
#include "dsp/GlideParameter.h"

namespace dsp
{
/**
 * A single delay voice: a settable-length recirculating (feedback) delay
 * with independent level and stereo pan - the repeated unit behind the
 * PCM81's 4-Voice/6-Voice "toolbox" sections layered on top of a reverb
 * core (each voice has its own Level/Delay/Feedback/Pan). Delay-time
 * changes glide per the shared GldResp/GldRange setting (setGlide())
 * rather than jumping, unless the change exceeds the glide range.
 */
class Voice
{
  public:
    void setBuffer(std::span<float> buffer) { comb_.setBuffer(buffer); }
    void setSampleRate(float sampleRate) { delayGlide_.setSampleRate(sampleRate); }

    void setDelaySamples(float samples) { delayGlide_.setTarget(samples); }
    void setGlide(float response0to100, float rangeSeconds)
    {
        delayGlide_.setResponse(response0to100);
        delayGlide_.setRangeSeconds(rangeSeconds);
    }

    void setFeedback(float feedback) { comb_.setFeedback(feedback); }
    void setLevel(float level) { level_ = level; }

    // pan in [-1 (full left), 0 (center), +1 (full right)].
    void setPan(float pan)
    {
        panLeft_ = (1.0f - pan) * 0.5f;
        panRight_ = (1.0f + pan) * 0.5f;
    }

    struct Output
    {
        float left;
        float right;
    };

    Output process(float input)
    {
        comb_.setDelaySamples(delayGlide_.next());
        auto voiceOut = comb_.process(input) * level_;
        return { voiceOut * panLeft_, voiceOut * panRight_ };
    }

    void reset()
    {
        comb_.reset();
        delayGlide_.reset();
    }

  private:
    Comb comb_;
    GlideParameter delayGlide_;
    float level_ = 0.0f;
    float panLeft_ = 0.5f;
    float panRight_ = 0.5f;
};
}

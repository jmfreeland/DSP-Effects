#pragma once

#include "dsp/Comb.h"

namespace dsp
{
/**
 * A single delay voice: a settable-length recirculating (feedback) delay
 * with independent level and stereo pan - the repeated unit behind the
 * PCM81's 4-Voice/6-Voice "toolbox" sections layered on top of a reverb
 * core (each voice has its own Level/Delay/Feedback/Pan).
 */
class Voice
{
  public:
    void setBuffer(std::span<float> buffer) { comb_.setBuffer(buffer); }
    void setDelaySamples(float samples) { comb_.setDelaySamples(samples); }
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
        auto voiceOut = comb_.process(input) * level_;
        return { voiceOut * panLeft_, voiceOut * panRight_ };
    }

    void reset() { comb_.reset(); }

  private:
    Comb comb_;
    float level_ = 0.0f;
    float panLeft_ = 0.5f;
    float panRight_ = 0.5f;
};
}

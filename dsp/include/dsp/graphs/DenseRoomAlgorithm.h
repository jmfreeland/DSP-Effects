#pragma once

#include "dsp/algorithms/DenseRoom.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Dense Room" algorithm: the DenseRoom
 * Block (see dsp/algorithms/DenseRoom.h) plus input trim. Left-In only,
 * matching the Block's own mono-in design (the manual's Block Diagram
 * draws only a Left Input) - same convention already used for Long
 * Digiplex/Layered Shift/Reverse Shift/etc.
 */
class DenseRoomAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::DenseRoom::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        engine_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f);
        reset();
    }

    // -- DenseRoom Block pass-throughs --
    void setPredelaySeconds(float seconds) { engine_.setPredelaySeconds(seconds); }
    void setRevTimeSeconds(float seconds) { engine_.setRevTimeSeconds(seconds); }
    void setHighCut(float amount0to1) { engine_.setHighCut(amount0to1); }
    void setSize(float amount0to1) { engine_.setSize(amount0to1); }
    void setPosition(float amount0to1) { engine_.setPosition(amount0to1); }
    void setPan(float panMinus1to1) { engine_.setPan(panMinus1to1); }
    void setEarlyMix(float amount0to1) { engine_.setEarlyMix(amount0to1); }
    void setDiffusion(float amount0to1) { engine_.setDiffusion(amount0to1); }
    void setMix(float wet) { engine_.setMix(wet); }
    void setAllpassDelaySamples(int stage, float samples) { engine_.setAllpassDelaySamples(stage, samples); }
    void setLineDelayMs(int line, float ms) { engine_.setLineDelayMs(line, ms); }
    void setLinePan(int line, float panMinus1to1) { engine_.setLinePan(line, panMinus1to1); }
    void setLineLevel(int line, float percent) { engine_.setLineLevel(line, percent); }

    // -- Input conditioning --
    void setInLevel(float level) { inLevel_ = level; }

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
        left *= inLevel_;
        engine_.processSample(left, right);
    }

  private:
    dsp::algorithms::DenseRoom engine_;
    float inLevel_ = 1.0f;
};
}

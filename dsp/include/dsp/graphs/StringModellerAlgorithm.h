#pragma once

#include "dsp/algorithms/StringModeller.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "String Modeller" algorithm: the
 * StringModeller Block (see dsp/algorithms/StringModeller.h) plus input
 * trim. Left-In only, matching the Block's own Block Diagram (only
 * "Left Input" is drawn feeding the stimulation filter) - the same
 * Left-In-only convention already used for Patch Factory and the
 * Left/Layered/Reverse Shift family.
 */
class StringModellerAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::StringModeller::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        engine_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f);
        reset();
    }

    // -- StringModeller Block pass-throughs --
    void setPitch(float semitones) { engine_.setPitch(semitones); }
    void setDecay(float percent0to100) { engine_.setDecay(percent0to100); }
    void setGateAmount(float percent1to100) { engine_.setGateAmount(percent1to100); }
    void setFreq(float percent0to100) { engine_.setFreq(percent0to100); }
    void setQfac(float percent0to100) { engine_.setQfac(percent0to100); }
    void setBright(float percent0to100) { engine_.setBright(percent0to100); }
    void setHighAmt(float percent) { engine_.setHighAmt(percent); }
    void setBandAmt(float percent) { engine_.setBandAmt(percent); }
    void setLowAmt(float percent) { engine_.setLowAmt(percent); }
    void setInAmt(float percent) { engine_.setInAmt(percent); }
    void setChorus(float percent0to100) { engine_.setChorus(percent0to100); }
    void setChorusSpeed(float percent0to100) { engine_.setChorusSpeed(percent0to100); }
    void setChorusDepth(float percent0to100) { engine_.setChorusDepth(percent0to100); }
    void setMix(float percent0to100) { engine_.setMix(percent0to100); }
    void setNoteHz(int string, float hz) { engine_.setNoteHz(string, hz); }
    void trigger() { engine_.trigger(); }

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
    dsp::algorithms::StringModeller engine_;
    float inLevel_ = 1.0f;
};
}

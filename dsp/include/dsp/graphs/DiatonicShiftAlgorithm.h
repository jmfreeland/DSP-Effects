#pragma once

#include "dsp/algorithms/DiatonicShift.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Diatonic Shift" algorithm: the
 * DiatonicShift Block (see dsp/algorithms/DiatonicShift.h) plus a
 * deliberately modest front end.
 *
 * Unlike the PCM81 Graphs, which wrap their Block in the manual-verified
 * "Reverb Shell" front end (see ConcertHallAlgorithm.h), we don't have a
 * primary source for the H3000's own UI/parameter-matrix structure the
 * way we do for the PCM81 - the H3000 Instruction Manual's own
 * "Algorithm 100" page (docs/eventide-diatonic-shift.md) documents the
 * Block's own parameters in detail, but not a separate class-wide front
 * end the way the PCM81's shared "Reverb Shell" is documented. So this
 * Graph is just the Block plus input trim - no generic stereo-width
 * control, since the real algorithm's stereo image comes from two
 * independently-harmonizing Voice generators (Left/Right), not from
 * rotating a stereo pair after the fact.
 */
class DiatonicShiftAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::DiatonicShift::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        shift_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- DiatonicShift Block pass-throughs --
    void setGrainSeconds(float seconds) { shift_.setGrainSeconds(seconds); }
    void setDelaySeconds(float seconds) { shift_.setDelaySeconds(seconds); }
    void setKey(int key) { shift_.setKey(key); }
    void setScale(dsp::Scale scale) { shift_.setScale(scale); }
    void setLeftVoice(dsp::HarmonicInterval interval) { shift_.setLeftVoice(interval); }
    void setRightVoice(dsp::HarmonicInterval interval) { shift_.setRightVoice(interval); }
    void setLeftFeedback(float amount) { shift_.setLeftFeedback(amount); }
    void setRightFeedback(float amount) { shift_.setRightFeedback(amount); }
    void setLeftMix(float wet) { shift_.setLeftMix(wet); }
    void setRightMix(float wet) { shift_.setRightMix(wet); }
    void setTuneCents(float cents) { shift_.setTuneCents(cents); }
    void setFrequencyRange(float minHz, float maxHz) { shift_.setFrequencyRange(minHz, maxHz); }
    float trackedFrequencyHz() const { return shift_.trackedFrequencyHz(); }

    // -- Input conditioning --
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    void reset() { shift_.reset(); }

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

        shift_.processSample(left, right);
    }

  private:
    dsp::algorithms::DiatonicShift shift_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}

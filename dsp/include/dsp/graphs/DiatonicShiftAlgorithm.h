#pragma once

#include "dsp/StereoRotate.h"
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
 * way we do for the PCM81 - docs/eventide-h3000-notes.md is built from
 * the *service* manual (hardware architecture), not a user/parameter
 * guide. So this front end is a plain, honestly-generic wrapper (input
 * trim, output stereo width) rather than a claimed reconstruction of the
 * H3000's actual control surface - revisit if an H3000 user guide turns
 * up the way the PCM81 one did.
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
        setWidth(0.0f);
        reset();
    }

    // -- DiatonicShift Block pass-throughs --
    void setGrainSeconds(float seconds) { shift_.setGrainSeconds(seconds); }
    void setDelaySeconds(float seconds) { shift_.setDelaySeconds(seconds); }
    void setKey(int key) { shift_.setKey(key); }
    void setScale(dsp::Scale scale) { shift_.setScale(scale); }
    void setScaleDegree(int degrees) { shift_.setScaleDegree(degrees); }
    void setRegen(float amount) { shift_.setRegen(amount); }
    void setMix(float wet) { shift_.setMix(wet); }

    // -- Input conditioning --
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    // -360..360 degrees on the final output. See dsp/StereoRotate.h.
    void setWidth(float degrees) { widthDegrees_ = degrees; }

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

        rotateStereoWidth(left, right, widthDegrees_);
    }

  private:
    dsp::algorithms::DiatonicShift shift_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
    float widthDegrees_ = 0.0f;
};
}

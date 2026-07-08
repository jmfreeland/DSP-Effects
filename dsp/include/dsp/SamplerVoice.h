#pragma once

#include "dsp/Math.h"
#include "dsp/PitchShifter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp
{
/**
 * One channel of a sample record/playback engine - the repeated unit
 * behind the Eventide H3000's Studio Sampler (Algorithm 120/121, see
 * docs/eventide-studio-sampler.md): record live input into a fixed
 * buffer on command, then play it back (once or looped, within a
 * settable Start/End range) with independent Pitch and Time control and
 * its own Attack/Release envelope.
 *
 * Two Shift Modes, matching the manual's own #18 exactly: "generic
 * sampler" is a single varispeed read - pitch and speed change together,
 * like a tape (`rate = timeRatio * pitchRatio`); "constant length" reads
 * at a Time-only rate, then applies Pitch independently via
 * `PitchShifter` (reusing the same live pitch-ratio technique Timesqueeze
 * applies to a continuous input, just fed this Component's own
 * varispeed-scrubbed buffer read instead of a live signal) - "splicing is
 * used to shift the pitch of the sample without changing playback
 * length," per the manual.
 */
class SamplerVoice
{
  public:
    enum class ShiftMode
    {
        kGenericSampler,
        kConstantLength,
    };
    enum class TriggerMode
    {
        kOff,
        kAudio,
    };

    void setRecordBuffer(std::span<float> buffer) { recordBuffer_ = buffer; }
    void setPitchShifterBuffer(std::span<float> buffer) { pitchShifter_.setBuffer(buffer); }

    void prepare(float sampleRate)
    {
        sampleRate_ = sampleRate;
        pitchShifter_.prepare(sampleRate_);
        pitchShifter_.setGrainSeconds(0.08f);
        reset();
    }

    // -3600..3600 cents (six octaves), per the manual's own Pitch range.
    void setPitchCents(float cents) { pitchRatio_ = std::pow(2.0f, cents / 1200.0f); }
    // 0-800%: 100 = normal speed; higher compresses (speeds up) playback.
    void setTimePercent(float percent) { timeRatio_ = std::max(percent, 1.0f) / 100.0f; }
    void setAttackSeconds(float seconds) { attackSeconds_ = std::clamp(seconds, 0.001f, 1.0f); }
    void setReleaseSeconds(float seconds) { releaseSeconds_ = std::clamp(seconds, 0.001f, 1.0f); }
    // 0..1: playback bounds within the recorded buffer - a fixed-parameter
    // stand-in for the manual's interactive "rock 'n' reel" point editing
    // (no display exists in any of this project's consumers to drive that).
    void setStartFraction(float frac0to1) { startFraction_ = clamp01(frac0to1); }
    void setEndFraction(float frac0to1) { endFraction_ = clamp01(frac0to1); }
    void setLoop(bool loop) { loop_ = loop; }
    void setShiftMode(ShiftMode mode) { shiftMode_ = mode; }
    // Audio-level record triggering (#9-11) - no MIDI needed for this one.
    void setTriggerMode(TriggerMode mode) { triggerMode_ = mode; }
    void setThreshold(float level0to1) { threshold_ = clamp01(level0to1); }

    bool isRecording() const { return state_ == State::kRecording || state_ == State::kArmed; }
    bool isPlaying() const { return state_ == State::kPlaying; }
    std::size_t recordedLength() const { return recordedLength_; }

    // Manual "record" gesture: starts recording immediately, or arms for
    // audio-level triggering if Trigger Mode is Audio.
    void record()
    {
        writeIndex_ = 0;
        state_ = triggerMode_ == TriggerMode::kAudio ? State::kArmed : State::kRecording;
    }

    // Manual "stop" gesture: ends recording early, or halts playback.
    void stop()
    {
        if (state_ == State::kRecording || state_ == State::kArmed)
        {
            recordedLength_ = writeIndex_;
            state_ = State::kIdle;
        }
        else if (state_ == State::kPlaying)
        {
            state_ = State::kIdle;
        }
    }

    // Manual "play" gesture: starts playback from Start, if a sample exists.
    void play()
    {
        if (recordedLength_ == 0)
        {
            return;
        }
        readPos_ = startFraction_ * static_cast<float>(recordedLength_);
        elapsedSamples_ = 0.0f;
        state_ = State::kPlaying;
    }

    void reset()
    {
        state_ = State::kIdle;
        writeIndex_ = 0;
        recordedLength_ = 0;
        readPos_ = 0.0f;
        envelope_ = 0.0f;
        elapsedSamples_ = 0.0f;
        pitchShifter_.reset();
    }

    float process(float input)
    {
        if (state_ == State::kArmed)
        {
            if (std::fabs(input) < threshold_)
            {
                return 0.0f;
            }
            state_ = State::kRecording;
        }
        if (state_ == State::kRecording)
        {
            if (writeIndex_ < recordBuffer_.size())
            {
                recordBuffer_[writeIndex_] = input;
                ++writeIndex_;
            }
            else
            {
                recordedLength_ = writeIndex_;
                state_ = State::kIdle;
            }
            return 0.0f;
        }
        if (state_ != State::kPlaying)
        {
            return 0.0f;
        }
        return advancePlayback();
    }

  private:
    enum class State
    {
        kIdle,
        kArmed,
        kRecording,
        kPlaying,
    };

    float advancePlayback()
    {
        auto startSample = startFraction_ * static_cast<float>(recordedLength_);
        auto endSample = endFraction_ * static_cast<float>(recordedLength_);
        if (endSample <= startSample)
        {
            state_ = State::kIdle;
            return 0.0f;
        }

        auto rate = shiftMode_ == ShiftMode::kGenericSampler ? timeRatio_ * pitchRatio_ : timeRatio_;
        auto scrubbed = readSample(readPos_);
        readPos_ += rate;

        float output;
        if (shiftMode_ == ShiftMode::kConstantLength)
        {
            pitchShifter_.setPitchRatio(pitchRatio_);
            output = pitchShifter_.process(scrubbed);
        }
        else
        {
            output = scrubbed;
        }

        auto attackSamples = attackSeconds_ * sampleRate_;
        auto releaseSamples = releaseSeconds_ * sampleRate_;
        auto totalSamples = (endSample - startSample) / rate;
        auto releaseStart = std::max(totalSamples - releaseSamples, attackSamples);

        if (elapsedSamples_ < attackSamples)
        {
            envelope_ = attackSamples > 0.0f ? elapsedSamples_ / attackSamples : 1.0f;
        }
        else if (elapsedSamples_ >= releaseStart)
        {
            auto t = releaseSamples > 0.0f ? (elapsedSamples_ - releaseStart) / releaseSamples : 1.0f;
            envelope_ = std::max(0.0f, 1.0f - t);
        }
        else
        {
            envelope_ = 1.0f;
        }
        elapsedSamples_ += 1.0f;

        if (readPos_ >= endSample)
        {
            if (loop_)
            {
                readPos_ = startSample;
                elapsedSamples_ = 0.0f;
            }
            else
            {
                state_ = State::kIdle;
            }
        }

        return output * envelope_;
    }

    float readSample(float pos) const
    {
        if (recordedLength_ == 0)
        {
            return 0.0f;
        }
        auto maxIndex = static_cast<float>(recordedLength_ - 1);
        auto clamped = std::clamp(pos, 0.0f, maxIndex);
        auto i0 = static_cast<std::size_t>(clamped);
        auto i1 = std::min(i0 + 1, static_cast<std::size_t>(maxIndex));
        auto frac = clamped - static_cast<float>(i0);
        return lerp(recordBuffer_[i0], recordBuffer_[i1], frac);
    }

    std::span<float> recordBuffer_;
    PitchShifter pitchShifter_;
    float sampleRate_ = 48000.0f;

    State state_ = State::kIdle;
    std::size_t writeIndex_ = 0;
    std::size_t recordedLength_ = 0;
    float readPos_ = 0.0f;
    float envelope_ = 0.0f;
    float elapsedSamples_ = 0.0f;

    float pitchRatio_ = 1.0f;
    float timeRatio_ = 1.0f;
    float attackSeconds_ = 0.005f;
    float releaseSeconds_ = 0.005f;
    float startFraction_ = 0.0f;
    float endFraction_ = 1.0f;
    bool loop_ = false;
    ShiftMode shiftMode_ = ShiftMode::kGenericSampler;
    TriggerMode triggerMode_ = TriggerMode::kOff;
    float threshold_ = 0.1f;
};
}

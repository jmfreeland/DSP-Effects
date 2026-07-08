#pragma once

#include "dsp/Math.h"
#include "dsp/SamplerVoice.h"

#include <array>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Eventide H3000-inspired "Studio Sampler" (Algorithm 120/121), per that
 * algorithm's own manual page: "This algorithm will digitally record...
 * stereo or mono audio. Two separate samples can be recorded into memory
 * and played back... the pitch of the samples can be shifted over a six
 * octave range, without altering the playback length. Conversely, the
 * length of the sample can be altered without changing pitch."
 *
 * Implements the manual's own "Mono Mode" Block Diagram literally: two
 * fully independent `SamplerVoice`s (see dsp/SamplerVoice.h), one per
 * channel, each with its own record/stop/play triggers, Pitch, Time,
 * Attack/Release, loop point range, and audio-level record trigger.
 *
 * Algorithm 121 in the manual isn't a distinct algorithm at all - its own
 * page states plainly that it's "algorithm 120... [with] the default of
 * this algorithm... set to stereo," provided purely so a user recording a
 * stereo sample doesn't have to change the Record Mode default by hand.
 * Record Mode (mono/stereo capture linking) itself isn't modeled here:
 * this Block's two channels are always independent, matching Algorithm
 * 120's own default and the "Mono Mode" diagram, which is the more
 * general of the two cases - see docs/eventide-studio-sampler.md.
 *
 * Everything requiring MIDI or a front-panel display - MIDI Mode/Base
 * Note/Split Point/Drum Trigger (#12-17), and the "rock 'n' reel"
 * interactive begin/end point editing (no display exists to drive it) -
 * is out of scope, matching this project's established precedent (no
 * consumer implements MIDI input; see e.g. docs/eventide-band-delay.md).
 * Start/End (#N/A here, a fixed-parameter stand-in for "rock 'n' reel")
 * and Trigger Mode/Threshold (#9-11, audio-level record triggering - a
 * genuinely audio-domain feature needing no MIDI at all) are kept.
 */
class StudioSampler
{
  public:
    static constexpr int kNumChannels = 2;
    static constexpr float kMaxRecordSeconds = 8.0f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kRecordCapacitySamples =
      static_cast<std::size_t>(kMaxRecordSeconds * kMaxSampleRate);
    static constexpr std::size_t kShifterCapacitySamples = static_cast<std::size_t>(0.35f * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return static_cast<std::size_t>(kNumChannels) * (kRecordCapacitySamples + kShifterCapacitySamples);
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        std::size_t offset = 0;
        for (int i = 0; i < kNumChannels; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            voices_[idx].setRecordBuffer(workingBuffer.subspan(offset, kRecordCapacitySamples));
            offset += kRecordCapacitySamples;
            voices_[idx].setPitchShifterBuffer(workingBuffer.subspan(offset, kShifterCapacitySamples));
            offset += kShifterCapacitySamples;
            voices_[idx].prepare(sampleRate);
        }
        setMix(100.0f);
        reset();
    }

    // -- Per-channel controls (channel: 0=left, 1=right) --
    void setPitchCents(int channel, float cents) { voices_[index(channel)].setPitchCents(cents); }
    void setTimePercent(int channel, float percent) { voices_[index(channel)].setTimePercent(percent); }
    void setAttackSeconds(int channel, float seconds) { voices_[index(channel)].setAttackSeconds(seconds); }
    void setReleaseSeconds(int channel, float seconds) { voices_[index(channel)].setReleaseSeconds(seconds); }
    void setStartFraction(int channel, float frac0to1) { voices_[index(channel)].setStartFraction(frac0to1); }
    void setEndFraction(int channel, float frac0to1) { voices_[index(channel)].setEndFraction(frac0to1); }
    void setLoop(int channel, bool loop) { voices_[index(channel)].setLoop(loop); }
    void setShiftMode(int channel, SamplerVoice::ShiftMode mode) { voices_[index(channel)].setShiftMode(mode); }
    void setTriggerMode(int channel, SamplerVoice::TriggerMode mode)
    {
        voices_[index(channel)].setTriggerMode(mode);
    }
    void setThreshold(int channel, float level0to1) { voices_[index(channel)].setThreshold(level0to1); }

    void record(int channel) { voices_[index(channel)].record(); }
    void stop(int channel) { voices_[index(channel)].stop(); }
    void play(int channel) { voices_[index(channel)].play(); }

    bool isRecording(int channel) const { return voices_[index(channel)].isRecording(); }
    bool isPlaying(int channel) const { return voices_[index(channel)].isPlaying(); }

    // #8: 0-100%, dry input vs sampler output.
    void setMix(float percent0to100) { mix_ = clamp01(percent0to100 / 100.0f); }

    void reset()
    {
        for (auto& v : voices_)
        {
            v.reset();
        }
    }

    void process(std::span<float> left, std::span<float> right)
    {
        for (std::size_t n = 0; n < left.size(); ++n)
        {
            processSample(left[n], right[n]);
        }
    }

    void processSample(float& left, float& right)
    {
        auto dryLeft = left;
        auto dryRight = right;
        auto recordingLeft = voices_[0].isRecording();
        auto recordingRight = voices_[1].isRecording();
        auto wetLeft = voices_[0].process(dryLeft);
        auto wetRight = voices_[1].process(dryRight);
        // While recording, pass the live input straight through (for
        // monitoring) regardless of Mix - matching the manual: "the H3000
        // will be passing its audio input to both output channels."
        left = recordingLeft ? dryLeft : lerp(dryLeft, wetLeft, mix_);
        right = recordingRight ? dryRight : lerp(dryRight, wetRight, mix_);
    }

  private:
    static std::size_t index(int channel) { return static_cast<std::size_t>(channel == 1 ? 1 : 0); }

    std::array<SamplerVoice, kNumChannels> voices_;
    float mix_ = 1.0f;
};
}

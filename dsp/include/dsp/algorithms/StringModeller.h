#pragma once

#include "dsp/DelayLine.h"
#include "dsp/Decay.h"
#include "dsp/Envelope.h"
#include "dsp/LFO.h"
#include "dsp/Math.h"
#include "dsp/NoiseGenerator.h"
#include "dsp/StateVariableFilter.h"
#include "dsp/StringVoice.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Eventide H3000-inspired "String Modeller" (Algorithm 118), per that
 * algorithm's own manual page: "This algorithm digitally simulates a set
 * of six strings. When processing audio input, these strings act as
 * passive resonators... To generate some amazingly realistic sounds, the
 * 'strings' can be 'plucked' by playing notes on a MIDI keyboard." Six
 * `StringVoice` Karplus-Strong resonators (see dsp/StringVoice.h),
 * excited by a shared stimulation signal, feed a Chorus mono-to-stereo
 * widener.
 *
 * The stimulation signal is the manual's own three-source mix: a shared
 * `StateVariableFilter` (reused from Patch Factory/Vocoder/Band Delay)
 * fed by a `NoiseGenerator`, whose simultaneous low/band/high outputs are
 * blended by Low/Band/High Amt (#11-13); plus the live input directly,
 * weighted by In Amt (#14) - "usually used when the strings are acting
 * as passive resonators," per the manual's own "Interesting Ideas": set
 * High/Band/Low Amt to 0, In Amt to ~20%, and the strings resonate with
 * the input signal alone.
 *
 * No consumer in this project (the Polyend Endless Patch, the native
 * host harness, or the JUCE plugin) implements MIDI input, so this Block
 * can't offer the manual's note-on "pluck" triggering or MIDI-driven
 * tuning - the same fact that already shaped Band Delay's Note Mode skip
 * (see docs/eventide-band-delay.md). Two changes follow directly:
 *
 * - Gate Mode is always effectively the manual's "Open" setting -
 *   "stimulate the strings constantly, regardless of whether any keys
 *   are pressed" - the only one of the manual's three modes that doesn't
 *   require MIDI note events to produce any sound at all.
 * - A `trigger()` method stands in for a MIDI note-on: it fires a single
 *   shaped noise burst (envelope duration set by Gate, #4) into all six
 *   strings at once, a hands-on "pluck" wired to the Endless's
 *   footswitch - additive on top of the continuous stimulation above,
 *   not a replacement for it.
 *
 * The manual's Decay (#1) and Release (#2) jointly describe a string's
 * behavior across a MIDI note being held and then released; without
 * note-on/off events there's no "held" state distinct from "released,"
 * so this Block collapses them into a single Decay control governing
 * each string's feedback/resonance gain at all times (Release, Sustain
 * #3, and Gate Mode #5 are dropped entirely). Hold (#6) and Offset (#7)
 * exist only to manage MIDI-driven tuning and are dropped for the same
 * reason; Note 1-6 (#29-34) survive as a direct settable Hz per string
 * (`setNoteHz()`) - the manual's own non-MIDI tuning path ("can be tuned
 * either manually (by setting the 'note' parameters) or with a MIDI
 * keyboard"). The velocity- and key-range-scaling expert parameters
 * (#20-28, #35-40) scale MIDI velocity/key-range response and are
 * dropped outright.
 *
 * See docs/eventide-string-modeller.md.
 */
class StringModeller
{
  public:
    static constexpr int kNumStrings = 6;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr float kMinStringHz = 20.0f;
    static constexpr std::size_t kVoiceCapacitySamples =
      static_cast<std::size_t>(kMaxSampleRate / kMinStringHz) + 4;
    static constexpr float kMaxChorusDelaySeconds = 0.4f;
    static constexpr std::size_t kChorusCapacitySamples =
      static_cast<std::size_t>(kMaxChorusDelaySeconds * kMaxSampleRate) + 4;

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return static_cast<std::size_t>(kNumStrings) * kVoiceCapacitySamples + kChorusCapacitySamples;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        for (int i = 0; i < kNumStrings; ++i)
        {
            voices_[static_cast<std::size_t>(i)].setBuffer(workingBuffer.subspan(offset, kVoiceCapacitySamples));
            offset += kVoiceCapacitySamples;
        }
        chorusLine_.setBuffer(workingBuffer.subspan(offset, kChorusCapacitySamples));
        chorusBaseDelaySamples_ = 0.01f * sampleRate_;

        stimulationFilter_.prepare(sampleRate_);

        // Standard guitar open-string tuning - a natural default for a
        // literal six-string model.
        static constexpr std::array<float, kNumStrings> kDefaultHz = { 82.41f, 110.00f, 146.83f,
                                                                         196.00f, 246.94f, 329.63f };
        noteHz_ = kDefaultHz;

        setPitch(0.0f);
        setDecay(60.0f);
        setGateAmount(30.0f);
        setFreq(50.0f);
        setQfac(30.0f);
        setBright(60.0f);
        setHighAmt(0.0f);
        setBandAmt(60.0f);
        setLowAmt(0.0f);
        setInAmt(0.0f);
        setChorus(40.0f);
        setChorusSpeed(30.0f);
        setChorusDepth(50.0f);
        setMix(60.0f);

        reset();
    }

    // #0: -100..100 notes (semitones), applied to all six strings' tuning.
    void setPitch(float semitones) { pitchSemitones_ = std::clamp(semitones, -100.0f, 100.0f); }

    // #1: 0-100, mapped to each string's feedback/resonance amount via
    // rt60ToGain - the sole decay control in this build (see class doc
    // comment for why Release/Sustain aren't exposed).
    void setDecay(float percent0to100) { decayRt60Seconds_ = mapLinear(percent0to100 / 100.0f, 0.05f, 12.0f); }

    // #4: 0-100, the manual pluck trigger's burst duration - see trigger().
    void setGateAmount(float percent1to100)
    {
        gateDurationSeconds_ = mapLinear(percent1to100 / 100.0f, 0.002f, 0.3f);
    }

    // #8: 0-100, stimulation noise filter center frequency.
    void setFreq(float percent0to100) { stimulationHz_ = mapLinear(percent0to100 / 100.0f, 80.0f, 6000.0f); }
    // #9: 0-100, stimulation filter resonance.
    void setQfac(float percent0to100) { stimulationQ_ = clamp01(percent0to100 / 100.0f); }
    // #10: 0-100, per-string feedback-loop damping brightness.
    void setBright(float percent0to100)
    {
        auto hz = mapLinear(percent0to100 / 100.0f, 400.0f, 18000.0f);
        dampingCoefficient_ = onePoleLowpassCoefficient(hz, sampleRate_);
    }

    // #11-13: -100..100%, relative mix of the shared filter's simultaneous
    // high/band/lowpass outputs into the noise stimulation signal.
    void setHighAmt(float percent) { highAmt_ = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }
    void setBandAmt(float percent) { bandAmt_ = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }
    void setLowAmt(float percent) { lowAmt_ = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }

    // #14: -100..100%, external input signal feeding the strings directly
    // (bypassing the noise filter) - see class doc comment.
    void setInAmt(float percent) { inAmt_ = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }

    // #15-17: chorus wet amount, sweep rate, sweep depth.
    void setChorus(float percent0to100) { chorusMix_ = clamp01(percent0to100 / 100.0f); }
    void setChorusSpeed(float percent0to100)
    {
        auto hz = mapLinear(percent0to100 / 100.0f, 0.03f, 4.0f);
        chorusLfo_.setFrequency(hz, sampleRate_);
    }
    void setChorusDepth(float percent0to100) { chorusDepthSeconds_ = mapLinear(percent0to100 / 100.0f, 0.0f, 0.3f); }

    // #19: 0-100%, dry/wet.
    void setMix(float percent0to100) { mix_ = clamp01(percent0to100 / 100.0f); }

    // #29-34 (expert): a direct Hz value per string standing in for the
    // manual's Note parameter - see class doc comment.
    void setNoteHz(int string, float hz) { noteHz_[static_cast<std::size_t>(string)] = std::max(hz, 1.0f); }

    // Manually "plucks" all six strings with a shaped noise burst - see
    // class doc comment. Wired to the Endless's footswitch.
    void trigger() { pluckEnvelope_.trigger(1.0f, 0.0f, std::max(gateDurationSeconds_ * sampleRate_, 1.0f)); }

    void reset()
    {
        for (auto& v : voices_)
        {
            v.reset();
        }
        chorusLine_.reset();
        stimulationFilter_.reset();
        pluckEnvelope_.reset();
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
        auto dry = left; // mono-in per the manual's own Block Diagram.

        stimulationFilter_.setCutoff(stimulationHz_);
        stimulationFilter_.setQ(stimulationQ_);
        auto filtered = stimulationFilter_.process(noise_.next());
        auto filteredNoise = filtered.highpass * highAmt_ + filtered.bandpass * bandAmt_ + filtered.lowpass * lowAmt_;

        auto pluckGate = pluckEnvelope_.next();
        auto pluckBurst = pluckGate * noise_.next();

        auto stimulation = filteredNoise + dry * inAmt_ + pluckBurst;

        auto tuningRatio = std::pow(2.0f, pitchSemitones_ / 12.0f);
        float resonatorSum = 0.0f;
        for (int i = 0; i < kNumStrings; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto hz = std::clamp(noteHz_[idx] * tuningRatio, kMinStringHz, 0.4f * sampleRate_);
            auto delaySamples = sampleRate_ / hz;
            voices_[idx].setDelaySamples(delaySamples);
            voices_[idx].setFeedback(rt60ToGain(delaySamples, sampleRate_, decayRt60Seconds_));
            voices_[idx].setDampingCoefficient(dampingCoefficient_);
            resonatorSum += voices_[idx].process(stimulation);
        }
        resonatorSum *= kStringNormalization;

        auto modDelaySamples =
          chorusBaseDelaySamples_ + chorusDepthSeconds_ * sampleRate_ * (0.5f + 0.5f * chorusLfo_.nextSine());
        chorusLine_.write(resonatorSum);
        auto delayed = chorusLine_.readLinear(std::clamp(modDelaySamples, 0.0f, kMaxChorusReadSamples));
        auto wetLeft = resonatorSum + chorusMix_ * delayed;
        auto wetRight = resonatorSum - chorusMix_ * delayed;

        left = lerp(dry, wetLeft, mix_);
        right = lerp(dry, wetRight, mix_);
    }

  private:
    static constexpr float kStringNormalization = 0.5f;
    static constexpr float kMaxChorusReadSamples = static_cast<float>(kChorusCapacitySamples) - 2.0f;

    float sampleRate_ = 48000.0f;

    std::array<StringVoice, kNumStrings> voices_;
    DelayLine chorusLine_;
    StateVariableFilter stimulationFilter_;
    NoiseGenerator noise_;
    LFO chorusLfo_;
    LinearRamp pluckEnvelope_;

    std::array<float, kNumStrings> noteHz_{};

    float pitchSemitones_ = 0.0f;
    float decayRt60Seconds_ = 4.0f;
    float gateDurationSeconds_ = 0.05f;
    float stimulationHz_ = 1000.0f;
    float stimulationQ_ = 0.3f;
    float dampingCoefficient_ = 0.0f;
    float highAmt_ = 0.0f;
    float bandAmt_ = 0.6f;
    float lowAmt_ = 0.0f;
    float inAmt_ = 0.0f;
    float chorusMix_ = 0.4f;
    float chorusDepthSeconds_ = 0.05f;
    float chorusBaseDelaySamples_ = 0.0f;
    float mix_ = 0.6f;
};
}

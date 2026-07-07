#pragma once

#include "dsp/DelayLine.h"
#include "dsp/Math.h"
#include "dsp/NoiseGenerator.h"
#include "dsp/PitchShifter.h"
#include "dsp/StateVariableFilter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Eventide H3000-inspired "Patch Factory" (Algorithm 111): a small
 * modular patch-bay of basic effect elements - a noise generator, two
 * tuneable filters (each switchable between lowpass/highpass/bandpass),
 * two delay lines, one pitch shifter, two scalers (attenuators), and two
 * summing junctions - wired together by a settable patch matrix, per
 * that algorithm's own manual page: "a flexible patching scheme and some
 * 'glue' ... clever users can create sound effects limited only by their
 * imaginations."
 *
 * Every one of the manual's 13 patch destinations (#19-31: both filter
 * inputs, both delay inputs, both scaler inputs, both summing junctions'
 * two inputs each, the pitch shifter input, and the two channel outputs)
 * can be routed from any of its 16 listed sources via setPatch(). To
 * keep the resulting graph well-defined for *any* user-chosen patch -
 * including ones a real patch cord could tie into a zero-delay loop -
 * every cross-connection reads the *previous* sample's value of its
 * source rather than the current one (Left Input and Noise Gen, the two
 * true external inputs, are the only sources read at zero latency). That
 * one-sample latency per hop is inaudible (~21us at 48kHz) and makes the
 * whole matrix inherently acyclic without needing to detect or reject
 * pathological patches.
 */
class PatchFactory
{
  public:
    enum class Source
    {
        kLeftInput,
        kSum1,
        kSum2,
        kDelay1,
        kDelay2,
        kScaler1,
        kScaler2,
        kLowpass1,
        kBandpass1,
        kHighpass1,
        kLowpass2,
        kBandpass2,
        kHighpass2,
        kPitchShift,
        kNoiseGen,
        kNullInput,
        kCount
    };

    enum class Destination
    {
        kFilter1In,
        kFilter2In,
        kDelay1In,
        kDelay2In,
        kScale1In,
        kScale2In,
        kSum1aIn,
        kSum1bIn,
        kSum2aIn,
        kSum2bIn,
        kShiftIn,
        kLOutput,
        kROutput,
        kCount
    };

    static constexpr float kMaxDelaySeconds = 0.5f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kDelayCapacitySamples =
      static_cast<std::size_t>(kMaxDelaySeconds * kMaxSampleRate);
    static constexpr std::size_t kPitchDelayCapacitySamples = kDelayCapacitySamples;

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return 2 * kDelayCapacitySamples + kPitchDelayCapacitySamples;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        delay1_.setBuffer(workingBuffer.subspan(0, kDelayCapacitySamples));
        delay2_.setBuffer(workingBuffer.subspan(kDelayCapacitySamples, kDelayCapacitySamples));
        pitchShifter_.setBuffer(
          workingBuffer.subspan(2 * kDelayCapacitySamples, kPitchDelayCapacitySamples));
        pitchShifter_.prepare(sampleRate);
        filter1_.prepare(sampleRate);
        filter2_.prepare(sampleRate);

        setCutoff1(1000.0f);
        setCutoff2(1000.0f);
        setQ1(0.0f);
        setQ2(0.0f);
        setDelay1Seconds(0.15f);
        setDelay2Seconds(0.25f);
        setScale1(0.3f);
        setScale2(0.7f);
        setShiftCents(0.0f);
        setPitchDelaySeconds(0.02f);
        setLeftMix(0.5f);
        setRightMix(0.5f);

        // Default patch: roughly the block diagram's own suggestive
        // wiring - Noise Gen through Scale1 into Sum1, Left Input through
        // Scale2 into Sum2, Sum2 feeding Delay1 into Filter1 (Left
        // Output), and Left Input feeding the pitch shifter into Filter2
        // (Right Output). Sum1/Delay2 are left available but unused by
        // the default output chain, exactly as on real hardware before
        // any user re-patching.
        setPatch(Destination::kScale1In, Source::kNoiseGen);
        setPatch(Destination::kScale2In, Source::kLeftInput);
        setPatch(Destination::kSum1aIn, Source::kScaler1);
        setPatch(Destination::kSum1bIn, Source::kNullInput);
        setPatch(Destination::kSum2aIn, Source::kScaler2);
        setPatch(Destination::kSum2bIn, Source::kNullInput);
        setPatch(Destination::kDelay1In, Source::kSum2);
        setPatch(Destination::kDelay2In, Source::kLeftInput);
        setPatch(Destination::kFilter1In, Source::kDelay1);
        setPatch(Destination::kShiftIn, Source::kLeftInput);
        setPatch(Destination::kFilter2In, Source::kPitchShift);
        setPatch(Destination::kLOutput, Source::kLowpass1);
        setPatch(Destination::kROutput, Source::kLowpass2);

        reset();
    }

    void setPatch(Destination destination, Source source)
    {
        patch_[static_cast<std::size_t>(destination)] = source;
    }

    void setCutoff1(float hz) { filter1_.setCutoff(hz); }
    void setCutoff2(float hz) { filter2_.setCutoff(hz); }
    void setQ1(float q0to1) { filter1_.setQ(q0to1); }
    void setQ2(float q0to1) { filter2_.setQ(q0to1); }

    void setDelay1Seconds(float seconds)
    {
        delay1Samples_ = std::clamp(seconds, 0.0f, kMaxDelaySeconds) * sampleRate_;
    }
    void setDelay2Seconds(float seconds)
    {
        delay2Samples_ = std::clamp(seconds, 0.0f, kMaxDelaySeconds) * sampleRate_;
    }

    // -100..100 per cent (negative inverts phase).
    void setScale1(float percent) { scale1_ = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }
    void setScale2(float percent) { scale2_ = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }

    void setShiftCents(float cents) { pitchShifter_.setSemitones(std::clamp(cents, -4800.0f, 1200.0f) / 100.0f); }

    void setPitchDelaySeconds(float seconds)
    {
        pitchShifter_.setGrainSeconds(std::clamp(seconds, 0.01f, 0.3f));
    }

    void setLeftMix(float wet) { leftMix_ = clamp01(wet); }
    void setRightMix(float wet) { rightMix_ = clamp01(wet); }

    void reset()
    {
        delay1_.reset();
        delay2_.reset();
        pitchShifter_.reset();
        filter1_.reset();
        filter2_.reset();
        noise_.reset();
        previous_.fill(0.0f);
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

        std::array<float, static_cast<std::size_t>(Source::kCount)> current{};
        current[static_cast<std::size_t>(Source::kLeftInput)] = dryLeft;
        current[static_cast<std::size_t>(Source::kNoiseGen)] = noise_.next();
        current[static_cast<std::size_t>(Source::kNullInput)] = 0.0f;

        current[static_cast<std::size_t>(Source::kScaler1)] = read(Destination::kScale1In) * scale1_;
        current[static_cast<std::size_t>(Source::kScaler2)] = read(Destination::kScale2In) * scale2_;
        current[static_cast<std::size_t>(Source::kSum1)] =
          read(Destination::kSum1aIn) + read(Destination::kSum1bIn);
        current[static_cast<std::size_t>(Source::kSum2)] =
          read(Destination::kSum2aIn) + read(Destination::kSum2bIn);

        current[static_cast<std::size_t>(Source::kDelay1)] = delay1_.readLinear(delay1Samples_);
        delay1_.write(read(Destination::kDelay1In));
        current[static_cast<std::size_t>(Source::kDelay2)] = delay2_.readLinear(delay2Samples_);
        delay2_.write(read(Destination::kDelay2In));

        auto filter1Out = filter1_.process(read(Destination::kFilter1In));
        current[static_cast<std::size_t>(Source::kLowpass1)] = filter1Out.lowpass;
        current[static_cast<std::size_t>(Source::kHighpass1)] = filter1Out.highpass;
        current[static_cast<std::size_t>(Source::kBandpass1)] = filter1Out.bandpass;

        auto filter2Out = filter2_.process(read(Destination::kFilter2In));
        current[static_cast<std::size_t>(Source::kLowpass2)] = filter2Out.lowpass;
        current[static_cast<std::size_t>(Source::kHighpass2)] = filter2Out.highpass;
        current[static_cast<std::size_t>(Source::kBandpass2)] = filter2Out.bandpass;

        current[static_cast<std::size_t>(Source::kPitchShift)] =
          pitchShifter_.process(read(Destination::kShiftIn));

        auto wetLeft = current[static_cast<std::size_t>(patch_[static_cast<std::size_t>(Destination::kLOutput)])];
        auto wetRight =
          current[static_cast<std::size_t>(patch_[static_cast<std::size_t>(Destination::kROutput)])];

        left = lerp(dryLeft, wetLeft, leftMix_);
        right = lerp(dryLeft, wetRight, rightMix_);

        previous_ = current;
    }

  private:
    float read(Destination destination) const
    {
        auto source = patch_[static_cast<std::size_t>(destination)];
        return previous_[static_cast<std::size_t>(source)];
    }

    float sampleRate_ = 48000.0f;

    DelayLine delay1_, delay2_;
    float delay1Samples_ = 0.0f, delay2Samples_ = 0.0f;

    PitchShifter pitchShifter_;

    StateVariableFilter filter1_, filter2_;

    NoiseGenerator noise_;
    float scale1_ = 0.0f, scale2_ = 0.0f;
    float leftMix_ = 0.5f, rightMix_ = 0.5f;

    std::array<Source, static_cast<std::size_t>(Destination::kCount)> patch_{};
    std::array<float, static_cast<std::size_t>(Source::kCount)> previous_{};
};
}

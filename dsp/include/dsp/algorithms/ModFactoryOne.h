#pragma once

#include "dsp/DelayLine.h"
#include "dsp/EnvelopeDucker.h"
#include "dsp/Math.h"
#include "dsp/MultiWaveLFO.h"
#include "dsp/NoiseGenerator.h"
#include "dsp/StateVariableFilter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Eventide H3000-inspired "mod factory|one" (Algorithm 122), per that
 * algorithm's own manual page: "This is a 'modular' effects processing
 * algorithm. Software 'patch cords' can be used to connect the
 * processing modules shown below in any desired configuration. The main
 * building blocks are a pair of sweepable delays, a pair of
 * state-variable filters, two low-frequency oscillators, two envelope
 * detectors, and two amplitude modulators."
 *
 * Like Patch Factory (Algorithm 111, see PatchFactory.h) this is a
 * genuine patch-bay rather than a fixed topology, using the exact same
 * one-sample-latency technique: every `setPatch(Destination, Source)`
 * cross-connection reads the *previous* sample's value of its source,
 * making the matrix inherently acyclic for any user-chosen patch. This
 * one is substantially larger - 28 destinations x 26 sources, versus
 * Patch Factory's 13 x 16 - reflecting the manual's own framing of mod
 * factory as a dedicated "create your own algorithms" toolkit rather
 * than one more fixed effect. Two new Primitives were needed for modules
 * with no existing equivalent: `MultiWaveLFO` (13 waveforms including
 * audio-triggered and toggle sweeps) and `EnvelopeDucker` (an envelope
 * follower with a second, compressor-style ducking output). Delays reuse
 * `DelayLine`, filters reuse `StateVariableFilter` (a sixth reuse, after
 * Patch Factory/Vocoder/Band Delay/String Modeller/Band Delay again),
 * and the noise source reuses `NoiseGenerator`.
 *
 * The Outputs table on the manual's own page has a numbering slip
 * (mixer 1 is printed as "#2", duplicating right input's own "#2,"
 * with every following entry keeping that off-by-one rather than being
 * renumbered) - this archive has documented similar manual
 * inconsistencies before (e.g. Multi-Shift's Feedback parameter units,
 * mod factory's own LFO waveform-family count in MultiWaveLFO.h). The
 * `Source` enum here implements the complete, correctly-ordered list of
 * 26 distinct sources rather than propagating the printed numbering
 * error.
 *
 * The optional HS322/HS395 expansion boards that raise the maximum delay
 * time to 11 or 32 seconds aren't modeled - this project has no
 * equivalent hardware-option concept, so delays use the standard H3000's
 * own 700ms maximum.
 */
class ModFactoryOne
{
  public:
    enum class Destination
    {
        kLeftOut,
        kRightOut,
        kMix1aIn,
        kMix1bIn,
        kMix2aIn,
        kMix2bIn,
        kMix3aIn,
        kMix3bIn,
        kMix4aIn,
        kMix4bIn,
        kAm1In,
        kAm1Mod,
        kAm2In,
        kAm2Mod,
        kDly1In,
        kDly1Mod,
        kDly2In,
        kDly2Mod,
        kFilt1In,
        kFilt1Mod,
        kFilt2In,
        kFilt2Mod,
        kEnv1In,
        kEnv2In,
        kLfo1In,
        kLfo2In,
        kMdScl1In,
        kMdScl2In,
        kCount
    };

    enum class Source
    {
        kZero,
        kLeftInput,
        kRightInput,
        kMixer1,
        kMixer2,
        kMixer3,
        kMixer4,
        kAmpMod1,
        kAmpMod2,
        kDelay1,
        kDelay2,
        kFilter1,
        kFilter2,
        kDucker1,
        kDucker2,
        kEnvelope1,
        kEnvelope2,
        kLfo1,
        kLfo2,
        kKnob1,
        kKnob2,
        kNoiseGen,
        kFullscale,
        kMinusFullscale,
        kModScale1,
        kModScale2,
        kCount
    };

    enum class FilterType
    {
        kLowpass,
        kBandpass,
        kHighpass,
    };

    static constexpr float kMaxDelaySeconds = 0.7f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kDelayCapacitySamples =
      static_cast<std::size_t>(kMaxDelaySeconds * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize() { return 2 * kDelayCapacitySamples; }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        delay1_.setBuffer(workingBuffer.subspan(0, kDelayCapacitySamples));
        delay2_.setBuffer(workingBuffer.subspan(kDelayCapacitySamples, kDelayCapacitySamples));
        filter1_.prepare(sampleRate_);
        filter2_.prepare(sampleRate_);
        lfo1_.prepare(sampleRate_);
        lfo2_.prepare(sampleRate_);
        env1_.prepare(sampleRate_);
        env2_.prepare(sampleRate_);

        setBpm(120.0f);
        setKnob1(0.0f);
        setKnob2(0.0f);
        setMix(100.0f);
        setFilterCutoff(0, 1000.0f);
        setFilterCutoff(1, 1000.0f);
        setFilterQ(0, 1.0f);
        setFilterQ(1, 1.0f);
        setFilterType(0, FilterType::kLowpass);
        setFilterType(1, FilterType::kLowpass);
        setFilterModAmount(0, 0.0f);
        setFilterModAmount(1, 0.0f);
        setDelayMs(0, 300.0f);
        setDelayMs(1, 200.0f);
        setDelayBpmBeats(0, 0.0f);
        setDelayBpmBeats(1, 0.0f);
        setDelayFeedback(0, 0.0f);
        setDelayFeedback(1, 0.0f);
        setDelayLoop(0, false);
        setDelayLoop(1, false);
        setDelayModMs(0, 0.0f);
        setDelayModMs(1, 0.0f);
        setLfoFrequency(0, 1.0f);
        setLfoFrequency(1, 1.0f);
        setLfoBpmBeats(0, 0.0f);
        setLfoBpmBeats(1, 0.0f);
        setLfoWaveform(0, MultiWaveLFO::Waveform::kSine);
        setLfoWaveform(1, MultiWaveLFO::Waveform::kSine);
        setLfoThresholdDb(0, -20.0f);
        setLfoThresholdDb(1, -20.0f);
        setLfoModAmount(0, 0.0f);
        setLfoModAmount(1, 0.0f);
        setEnvAttackMs(0, 5.0f);
        setEnvAttackMs(1, 5.0f);
        setEnvDecayMs(0, 100.0f);
        setEnvDecayMs(1, 100.0f);
        setEnvThresholdDb(0, -20.0f);
        setEnvThresholdDb(1, -20.0f);
        setEnvRatio(0, 4.0f);
        setEnvRatio(1, 4.0f);
        setAmpModAmount(0, 100.0f);
        setAmpModAmount(1, 100.0f);
        setAmpModOffset(0, 0.0f);
        setAmpModOffset(1, 0.0f);
        setMixAAmount(0, 100.0f);
        setMixBAmount(0, 0.0f);
        setMixAAmount(1, 100.0f);
        setMixBAmount(1, 0.0f);
        setMixAAmount(2, 100.0f);
        setMixBAmount(2, 0.0f);
        setMixAAmount(3, 100.0f);
        setMixBAmount(3, 0.0f);
        setModScaleAmount(0, 100.0f);
        setModScaleAmount(1, 100.0f);

        // A simple demonstrative default patch: Left Input through Delay 1
        // (LFO-swept, a manual flanger per the module doc) into Mixer 1,
        // Mixer 1 to both outputs.
        setPatch(Destination::kDly1In, Source::kLeftInput);
        setPatch(Destination::kDly1Mod, Source::kLfo1);
        setPatch(Destination::kMix1aIn, Source::kLeftInput);
        setPatch(Destination::kMix1bIn, Source::kDelay1);
        setPatch(Destination::kLeftOut, Source::kMixer1);
        setPatch(Destination::kRightOut, Source::kMixer1);
        setPatch(Destination::kLfo1In, Source::kZero);

        reset();
    }

    void setPatch(Destination destination, Source source)
    {
        patch_[static_cast<std::size_t>(destination)] = source;
    }

    // #33: 30-200 beats/min, shared by every tempo-syncable module.
    void setBpm(float bpm) { bpm_ = std::clamp(bpm, 30.0f, 200.0f); }
    // #30-31: 0-100%, Mod Knobs' own settable modulation output.
    void setKnob1(float percent0to100) { knob1_ = clamp01(percent0to100 / 100.0f); }
    void setKnob2(float percent0to100) { knob2_ = clamp01(percent0to100 / 100.0f); }
    // #32: 0-100%, dry/wet.
    void setMix(float percent0to100) { mix_ = clamp01(percent0to100 / 100.0f); }

    // #34-41: filters (0=filter1, 1=filter2).
    void setFilterCutoff(int filter, float hz) { filterCutoffHz_[index2(filter)] = std::clamp(hz, 0.0f, 7000.0f); }
    // 1-1000 per the manual; mapped linearly onto StateVariableFilter's own 0..1 Q range.
    void setFilterQ(int filter, float q1to1000)
    {
        filterQ_[index2(filter)] = clamp01((std::clamp(q1to1000, 1.0f, 1000.0f) - 1.0f) / 999.0f);
    }
    void setFilterType(int filter, FilterType type) { filterType_[index2(filter)] = type; }
    void setFilterModAmount(int filter, float hz) { filterModHz_[index2(filter)] = std::clamp(hz, 0.0f, 7000.0f); }

    // #42-51: delays (0=delay1, 1=delay2).
    void setDelayMs(int delay, float ms) { delayMs_[index2(delay)] = std::clamp(ms, 0.0f, 700.0f); }
    void setDelayBpmBeats(int delay, float beats24) { delayBpmBeats_[index2(delay)] = std::clamp(beats24, 0.0f, 96.0f); }
    void setDelayFeedback(int delay, float percent) { delayFeedback_[index2(delay)] = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }
    void setDelayLoop(int delay, bool loop) { delayLoop_[index2(delay)] = loop; }
    void setDelayModMs(int delay, float ms) { delayModMs_[index2(delay)] = std::clamp(ms, -500.0f, 500.0f); }

    // #52-61: LFOs (0=lfo1, 1=lfo2).
    void setLfoFrequency(int lfo, float hz) { lfoFreqHz_[index2(lfo)] = std::clamp(hz, 0.0f, 300.0f); }
    void setLfoBpmBeats(int lfo, float beats24) { lfoBpmBeats_[index2(lfo)] = std::clamp(beats24, 0.0f, 96.0f); }
    void setLfoWaveform(int lfo, MultiWaveLFO::Waveform waveform) { lfoWaveform_[index2(lfo)] = waveform; }
    void setLfoThresholdDb(int lfo, float db) { lfoThresholdDb_[index2(lfo)] = std::clamp(db, -40.0f, 0.0f); }
    void setLfoModAmount(int lfo, float hz) { lfoModHz_[index2(lfo)] = std::clamp(hz, 0.0f, 300.0f); }

    // #62-69: envelope detectors (0=env1, 1=env2).
    void setEnvAttackMs(int env, float ms) { envAttackMs_[index2(env)] = std::clamp(ms, 0.0f, 1000.0f); }
    void setEnvDecayMs(int env, float ms) { envDecayMs_[index2(env)] = std::clamp(ms, 0.0f, 1000.0f); }
    void setEnvThresholdDb(int env, float db) { envThresholdDb_[index2(env)] = std::clamp(db, -40.0f, 0.0f); }
    void setEnvRatio(int env, float ratioToOne) { envRatio_[index2(env)] = std::clamp(ratioToOne, 1.0f, 100.0f); }

    // #70-73: amplitude modulators (0=am1, 1=am2).
    void setAmpModAmount(int am, float percent) { ampModAmount_[index2(am)] = std::clamp(percent, -200.0f, 200.0f) / 100.0f; }
    void setAmpModOffset(int am, float percent) { ampModOffset_[index2(am)] = std::clamp(percent, -200.0f, 200.0f) / 100.0f; }

    // #74-81: mixers (0-3).
    void setMixAAmount(int mixer, float percent) { mixAAmount_[index4(mixer)] = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }
    void setMixBAmount(int mixer, float percent) { mixBAmount_[index4(mixer)] = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }

    // #82-83: modulation scalers (0-1).
    void setModScaleAmount(int scaler, float percent) { modScaleAmount_[index2(scaler)] = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }

    void reset()
    {
        delay1_.reset();
        delay2_.reset();
        filter1_.reset();
        filter2_.reset();
        lfo1_.reset();
        lfo2_.reset();
        env1_.reset();
        env2_.reset();
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
        current[idx(Source::kZero)] = 0.0f;
        current[idx(Source::kLeftInput)] = left;
        current[idx(Source::kRightInput)] = right;
        current[idx(Source::kKnob1)] = knob1_;
        current[idx(Source::kKnob2)] = knob2_;
        current[idx(Source::kNoiseGen)] = noise_.next();
        current[idx(Source::kFullscale)] = 1.0f;
        current[idx(Source::kMinusFullscale)] = -1.0f;

        current[idx(Source::kModScale1)] = read(Destination::kMdScl1In) * modScaleAmount_[0];
        current[idx(Source::kModScale2)] = read(Destination::kMdScl2In) * modScaleAmount_[1];

        current[idx(Source::kMixer1)] = read(Destination::kMix1aIn) * mixAAmount_[0] + read(Destination::kMix1bIn) * mixBAmount_[0];
        current[idx(Source::kMixer2)] = read(Destination::kMix2aIn) * mixAAmount_[1] + read(Destination::kMix2bIn) * mixBAmount_[1];
        current[idx(Source::kMixer3)] = read(Destination::kMix3aIn) * mixAAmount_[2] + read(Destination::kMix3bIn) * mixBAmount_[2];
        current[idx(Source::kMixer4)] = read(Destination::kMix4aIn) * mixAAmount_[3] + read(Destination::kMix4bIn) * mixBAmount_[3];

        current[idx(Source::kAmpMod1)] =
          read(Destination::kAm1In) * (ampModOffset_[0] + ampModAmount_[0] * read(Destination::kAm1Mod));
        current[idx(Source::kAmpMod2)] =
          read(Destination::kAm2In) * (ampModOffset_[1] + ampModAmount_[1] * read(Destination::kAm2Mod));

        current[idx(Source::kDelay1)] = processDelay(0, delay1_, Destination::kDly1In, Destination::kDly1Mod);
        current[idx(Source::kDelay2)] = processDelay(1, delay2_, Destination::kDly2In, Destination::kDly2Mod);

        current[idx(Source::kFilter1)] = processFilter(0, filter1_, Destination::kFilt1In, Destination::kFilt1Mod);
        current[idx(Source::kFilter2)] = processFilter(1, filter2_, Destination::kFilt2In, Destination::kFilt2Mod);

        auto lfo1Hz = effectiveLfoHz(0);
        lfo1_.setFrequency(lfo1Hz);
        lfo1_.setModAmountHz(lfoModHz_[0]);
        lfo1_.setThresholdDb(lfoThresholdDb_[0]);
        lfo1_.setWaveform(lfoWaveform_[0]);
        current[idx(Source::kLfo1)] = lfo1_.process(read(Destination::kLfo1In));

        auto lfo2Hz = effectiveLfoHz(1);
        lfo2_.setFrequency(lfo2Hz);
        lfo2_.setModAmountHz(lfoModHz_[1]);
        lfo2_.setThresholdDb(lfoThresholdDb_[1]);
        lfo2_.setWaveform(lfoWaveform_[1]);
        current[idx(Source::kLfo2)] = lfo2_.process(read(Destination::kLfo2In));

        env1_.setAttackMs(envAttackMs_[0]);
        env1_.setDecayMs(envDecayMs_[0]);
        env1_.setThresholdDb(envThresholdDb_[0]);
        env1_.setRatio(envRatio_[0]);
        auto env1Out = env1_.process(read(Destination::kEnv1In));
        current[idx(Source::kEnvelope1)] = env1Out.envelope;
        current[idx(Source::kDucker1)] = env1Out.ducker;

        env2_.setAttackMs(envAttackMs_[1]);
        env2_.setDecayMs(envDecayMs_[1]);
        env2_.setThresholdDb(envThresholdDb_[1]);
        env2_.setRatio(envRatio_[1]);
        auto env2Out = env2_.process(read(Destination::kEnv2In));
        current[idx(Source::kEnvelope2)] = env2Out.envelope;
        current[idx(Source::kDucker2)] = env2Out.ducker;

        auto wetLeft = current[idx(patch_[static_cast<std::size_t>(Destination::kLeftOut)])];
        auto wetRight = current[idx(patch_[static_cast<std::size_t>(Destination::kRightOut)])];

        left = lerp(dryLeft, wetLeft, mix_);
        right = lerp(right, wetRight, mix_);

        previous_ = current;
    }

  private:
    static std::size_t index2(int i) { return static_cast<std::size_t>(i == 1 ? 1 : 0); }
    static std::size_t index4(int i) { return static_cast<std::size_t>(std::clamp(i, 0, 3)); }
    static std::size_t idx(Source s) { return static_cast<std::size_t>(s); }

    float read(Destination destination) const
    {
        auto source = patch_[static_cast<std::size_t>(destination)];
        return previous_[static_cast<std::size_t>(source)];
    }

    float beatsToMs(float beats24) const { return (beats24 / 24.0f) * (60000.0f / bpm_); }

    float effectiveLfoHz(int lfo) const
    {
        auto beats = lfoBpmBeats_[index2(lfo)];
        if (beats <= 0.0f)
        {
            return lfoFreqHz_[index2(lfo)];
        }
        auto periodSeconds = (beats / 24.0f) * (60.0f / bpm_);
        return periodSeconds > 0.0f ? 1.0f / periodSeconds : lfoFreqHz_[index2(lfo)];
    }

    float processDelay(int which, DelayLine& line, Destination inDest, Destination modDest)
    {
        auto i = index2(which);
        auto baseMs = delayMs_[i] + beatsToMs(delayBpmBeats_[i]);
        auto modulatedMs = baseMs + delayModMs_[i] * read(modDest);
        auto delaySamples =
          std::clamp(modulatedMs * 0.001f * sampleRate_, 0.0f, static_cast<float>(kDelayCapacitySamples) - 2.0f);
        auto out = line.readLinear(delaySamples);
        if (delayLoop_[i])
        {
            line.write(out);
        }
        else
        {
            line.write(read(inDest) + delayFeedback_[i] * out);
        }
        return out;
    }

    float processFilter(int which, StateVariableFilter& filter, Destination inDest, Destination modDest)
    {
        auto i = index2(which);
        auto cutoff = std::clamp(filterCutoffHz_[i] + filterModHz_[i] * read(modDest), 1.0f, 7000.0f);
        filter.setCutoff(cutoff);
        filter.setQ(filterQ_[i]);
        auto out = filter.process(read(inDest));
        switch (filterType_[i])
        {
            case FilterType::kLowpass:
                return out.lowpass;
            case FilterType::kBandpass:
                return out.bandpass;
            case FilterType::kHighpass:
                return out.highpass;
        }
        return out.lowpass;
    }

    float sampleRate_ = 48000.0f;
    float bpm_ = 120.0f;
    float knob1_ = 0.0f, knob2_ = 0.0f;
    float mix_ = 1.0f;

    DelayLine delay1_, delay2_;
    std::array<float, 2> delayMs_{}, delayBpmBeats_{}, delayFeedback_{}, delayModMs_{};
    std::array<bool, 2> delayLoop_{};

    StateVariableFilter filter1_, filter2_;
    std::array<float, 2> filterCutoffHz_{}, filterQ_{}, filterModHz_{};
    std::array<FilterType, 2> filterType_{};

    MultiWaveLFO lfo1_, lfo2_;
    std::array<float, 2> lfoFreqHz_{}, lfoBpmBeats_{}, lfoThresholdDb_{}, lfoModHz_{};
    std::array<MultiWaveLFO::Waveform, 2> lfoWaveform_{};

    EnvelopeDucker env1_, env2_;
    std::array<float, 2> envAttackMs_{}, envDecayMs_{}, envThresholdDb_{}, envRatio_{};

    std::array<float, 2> ampModAmount_{}, ampModOffset_{};
    std::array<float, 4> mixAAmount_{}, mixBAmount_{};
    std::array<float, 2> modScaleAmount_{};

    NoiseGenerator noise_;

    std::array<Source, static_cast<std::size_t>(Destination::kCount)> patch_{};
    std::array<float, static_cast<std::size_t>(Source::kCount)> previous_{};
};
}

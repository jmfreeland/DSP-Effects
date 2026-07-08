#pragma once

#include "dsp/DelayLine.h"
#include "dsp/EnvelopeDucker.h"
#include "dsp/Math.h"
#include "dsp/MultiWaveLFO.h"
#include "dsp/NoiseGenerator.h"
#include "dsp/OnePole.h"
#include "dsp/PitchShiftVoice.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Eventide H3000-inspired "mod factory|two" (Algorithm 123), per that
 * algorithm's own manual page: "This algorithm is a cousin to algorithm
 * #122, mod factory|one. This too is a 'modular' effects processing
 * algorithm... The main building blocks are a pair of sweepable,
 * filtered delays, a pair of detuning pitch shifters, one low-frequency
 * oscillator, one envelope detector, and two amplitude modulators."
 *
 * Same genuine patch-bay shape as mod factory|one (see ModFactoryOne.h
 * for the shared design notes: the one-sample-latency
 * `setPatch(Destination, Source)` technique, `MultiWaveLFO`,
 * `EnvelopeDucker`), just with a smaller module set (one LFO and one
 * envelope detector instead of two each) traded for two new ones this
 * cousin algorithm adds:
 *
 * - **Filtered delays**: identical to mod factory|one's own delays, plus
 *   a settable high-frequency rolloff (`OnePoleLowpass`, reused) applied
 *   to each delay's output - "for warm, natural sounding delays."
 * - **Detuners**: "optimized for small amounts of pitch shifting," per
 *   the manual's own description a splice-based pitch shifter with its
 *   own input delay - exactly the shape `PitchShiftVoice` (Delay +
 *   PitchShifter) already is, reused here unchanged rather than hand-
 *   rolled again. The manual's own Fadelength and Splice Length are two
 *   distinct parameters describing a crossfade portion and a segment
 *   length; `PitchShifter`'s own single grain-length parameter
 *   (`setGrainSeconds`, whose triangular window spans the *entire*
 *   grain) doesn't separate those two - Splice Length maps onto
 *   `setGrainSeconds()` and Fadelength is a documented simplification
 *   not modeled separately.
 *
 * The patch matrix here is 28 destinations x 22 sources (mod
 * factory|one's own is 28 x 26 - two fewer modules removes four
 * sources).
 */
class ModFactoryTwo
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
        kDly1Ctmd,
        kDly2In,
        kDly2Mod,
        kDly2Ctmd,
        kDtune1In,
        kDtune1Mod,
        kDtune2In,
        kDtune2Mod,
        kEnvIn,
        kLfoIn,
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
        kDetune1,
        kDetune2,
        kDucker,
        kEnvelope,
        kLfo,
        kModKnob,
        kNoiseGen,
        kFullscale,
        kMinusFullscale,
        kModScale1,
        kModScale2,
        kCount
    };

    static constexpr float kMaxDelaySeconds = 0.65f;
    static constexpr float kMaxDetuneDelaySeconds = 0.7f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kDelayCapacitySamples =
      static_cast<std::size_t>(kMaxDelaySeconds * kMaxSampleRate);
    static constexpr std::size_t kDetuneCapacitySamples =
      PitchShiftVoice::requiredWorkingBufferSize(kMaxDetuneDelaySeconds, kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return 2 * kDelayCapacitySamples + 2 * kDetuneCapacitySamples;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        std::size_t offset = 0;
        delay1_.setBuffer(workingBuffer.subspan(offset, kDelayCapacitySamples));
        offset += kDelayCapacitySamples;
        delay2_.setBuffer(workingBuffer.subspan(offset, kDelayCapacitySamples));
        offset += kDelayCapacitySamples;
        detune1_.prepare(sampleRate_, workingBuffer.subspan(offset, kDetuneCapacitySamples), kMaxDetuneDelaySeconds);
        offset += kDetuneCapacitySamples;
        detune2_.prepare(sampleRate_, workingBuffer.subspan(offset, kDetuneCapacitySamples), kMaxDetuneDelaySeconds);
        lfo_.prepare(sampleRate_);
        env_.prepare(sampleRate_);

        setBpm(120.0f);
        setModKnob(0.0f);
        setMix(100.0f);

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
        setDelayHighcutHz(0, 20000.0f);
        setDelayHighcutHz(1, 20000.0f);
        setDelayHighcutModHz(0, 0.0f);
        setDelayHighcutModHz(1, 0.0f);

        setDetuneCents(0, -10.0f);
        setDetuneCents(1, 10.0f);
        setDetuneDelayMs(0, 20.0f);
        setDetuneDelayMs(1, 20.0f);
        setDetuneBpmBeats(0, 0.0f);
        setDetuneBpmBeats(1, 0.0f);
        setDetuneModAmountCents(0, 0.0f);
        setDetuneModAmountCents(1, 0.0f);
        setDetuneSpliceLengthMs(0, 150.0f);
        setDetuneSpliceLengthMs(1, 150.0f);

        setLfoFrequency(1.0f);
        setLfoBpmBeats(0.0f);
        setLfoWaveform(MultiWaveLFO::Waveform::kSine);
        setLfoThresholdDb(-20.0f);
        setLfoModAmount(0.0f);

        setEnvAttackMs(5.0f);
        setEnvDecayMs(100.0f);
        setEnvThresholdDb(-20.0f);
        setEnvRatio(4.0f);

        setAmpModAmount(0, 100.0f);
        setAmpModAmount(1, 100.0f);
        setAmpModOffset(0, 0.0f);
        setAmpModOffset(1, 0.0f);

        // Mixer 1 and 2 both default to a full A+B sum (used by the
        // default chorus patch below); Mixer 3/4 are left available but
        // unused, matching Patch Factory's own convention.
        for (int i = 0; i < 2; ++i)
        {
            setMixAAmount(i, 100.0f);
            setMixBAmount(i, 100.0f);
        }
        for (int i = 2; i < 4; ++i)
        {
            setMixAAmount(i, 0.0f);
            setMixBAmount(i, 0.0f);
        }
        setModScaleAmount(0, 100.0f);
        setModScaleAmount(1, 100.0f);

        // Default demonstrative patch: a chorus - Left Input split
        // between the two detuners (+/-10 cents, per the manual's own
        // "for a moderate chorus effect the left and right channels are
        // usually shifted plus and minus ten cents"), mixed back together.
        setPatch(Destination::kDtune1In, Source::kLeftInput);
        setPatch(Destination::kDtune2In, Source::kLeftInput);
        setPatch(Destination::kMix1aIn, Source::kLeftInput);
        setPatch(Destination::kMix1bIn, Source::kDetune1);
        setPatch(Destination::kMix2aIn, Source::kLeftInput);
        setPatch(Destination::kMix2bIn, Source::kDetune2);
        setPatch(Destination::kLeftOut, Source::kMixer1);
        setPatch(Destination::kRightOut, Source::kMixer2);

        reset();
    }

    void setPatch(Destination destination, Source source)
    {
        patch_[static_cast<std::size_t>(destination)] = source;
    }

    // #32: 30-200 beats/min, shared by every tempo-syncable module.
    void setBpm(float bpm) { bpm_ = std::clamp(bpm, 30.0f, 200.0f); }
    // #30: 0-100%, the Mod Knob's own settable modulation output.
    void setModKnob(float percent0to100) { modKnob_ = clamp01(percent0to100 / 100.0f); }
    // #31: 0-100%, dry/wet.
    void setMix(float percent0to100) { mix_ = clamp01(percent0to100 / 100.0f); }

    // #47-60: filtered delays (0=delay1, 1=delay2).
    void setDelayMs(int delay, float ms) { delayMs_[index2(delay)] = std::clamp(ms, 0.0f, 650.0f); }
    void setDelayBpmBeats(int delay, float beats24) { delayBpmBeats_[index2(delay)] = std::clamp(beats24, 0.0f, 96.0f); }
    void setDelayFeedback(int delay, float percent) { delayFeedback_[index2(delay)] = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }
    void setDelayLoop(int delay, bool loop) { delayLoop_[index2(delay)] = loop; }
    void setDelayModMs(int delay, float ms) { delayModMs_[index2(delay)] = std::clamp(ms, -500.0f, 500.0f); }
    void setDelayHighcutHz(int delay, float hz)
    {
        highcutHz_[index2(delay)] = std::clamp(hz, 1.0f, 20000.0f);
    }
    void setDelayHighcutModHz(int delay, float hz) { highcutModHz_[index2(delay)] = std::clamp(hz, 0.0f, 20000.0f); }

    // #33-44: detuners (0=detune1, 1=detune2).
    void setDetuneCents(int detuner, float cents) { detuneCents_[index2(detuner)] = std::clamp(cents, -100.0f, 100.0f); }
    void setDetuneDelayMs(int detuner, float ms) { detuneDelayMs_[index2(detuner)] = std::clamp(ms, 0.0f, 700.0f); }
    void setDetuneBpmBeats(int detuner, float beats24) { detuneBpmBeats_[index2(detuner)] = std::clamp(beats24, 0.0f, 96.0f); }
    void setDetuneModAmountCents(int detuner, float cents) { detuneModCents_[index2(detuner)] = std::clamp(cents, -1200.0f, 1200.0f); }
    void setDetuneSpliceLengthMs(int detuner, float ms)
    {
        detuneSpliceMs_[index2(detuner)] = std::clamp(ms, 1.0f, 700.0f);
        detuneVoice(detuner).setGrainSeconds(std::clamp(ms * 0.001f, 0.01f, 0.3f));
    }

    // #61-65: the LFO.
    void setLfoFrequency(float hz) { lfoFreqHz_ = std::clamp(hz, 0.0f, 300.0f); }
    void setLfoBpmBeats(float beats24) { lfoBpmBeats_ = std::clamp(beats24, 0.0f, 96.0f); }
    void setLfoWaveform(MultiWaveLFO::Waveform waveform) { lfoWaveform_ = waveform; }
    void setLfoThresholdDb(float db) { lfoThresholdDb_ = std::clamp(db, -40.0f, 0.0f); }
    void setLfoModAmount(float hz) { lfoModHz_ = std::clamp(hz, 0.0f, 300.0f); }

    // #66-69: the envelope detector.
    void setEnvAttackMs(float ms) { envAttackMs_ = std::clamp(ms, 0.0f, 1000.0f); }
    void setEnvDecayMs(float ms) { envDecayMs_ = std::clamp(ms, 0.0f, 1000.0f); }
    void setEnvThresholdDb(float db) { envThresholdDb_ = std::clamp(db, -40.0f, 0.0f); }
    void setEnvRatio(float ratioToOne) { envRatio_ = std::clamp(ratioToOne, 1.0f, 100.0f); }

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
        highcut1_.reset();
        highcut2_.reset();
        detune1_.reset();
        detune2_.reset();
        lfo_.reset();
        env_.reset();
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
        current[idx(Source::kModKnob)] = modKnob_;
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

        current[idx(Source::kDelay1)] =
          processDelay(0, delay1_, highcut1_, Destination::kDly1In, Destination::kDly1Mod, Destination::kDly1Ctmd);
        current[idx(Source::kDelay2)] =
          processDelay(1, delay2_, highcut2_, Destination::kDly2In, Destination::kDly2Mod, Destination::kDly2Ctmd);

        current[idx(Source::kDetune1)] = processDetune(0, detune1_, Destination::kDtune1In, Destination::kDtune1Mod);
        current[idx(Source::kDetune2)] = processDetune(1, detune2_, Destination::kDtune2In, Destination::kDtune2Mod);

        auto lfoHz = effectiveLfoHz();
        lfo_.setFrequency(lfoHz);
        lfo_.setModAmountHz(lfoModHz_);
        lfo_.setThresholdDb(lfoThresholdDb_);
        lfo_.setWaveform(lfoWaveform_);
        current[idx(Source::kLfo)] = lfo_.process(read(Destination::kLfoIn));

        env_.setAttackMs(envAttackMs_);
        env_.setDecayMs(envDecayMs_);
        env_.setThresholdDb(envThresholdDb_);
        env_.setRatio(envRatio_);
        auto envOut = env_.process(read(Destination::kEnvIn));
        current[idx(Source::kEnvelope)] = envOut.envelope;
        current[idx(Source::kDucker)] = envOut.ducker;

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

    PitchShiftVoice& detuneVoice(int which) { return which == 1 ? detune2_ : detune1_; }

    float read(Destination destination) const
    {
        auto source = patch_[static_cast<std::size_t>(destination)];
        return previous_[static_cast<std::size_t>(source)];
    }

    float beatsToMs(float beats24) const { return (beats24 / 24.0f) * (60000.0f / bpm_); }

    float effectiveLfoHz() const
    {
        if (lfoBpmBeats_ <= 0.0f)
        {
            return lfoFreqHz_;
        }
        auto periodSeconds = (lfoBpmBeats_ / 24.0f) * (60.0f / bpm_);
        return periodSeconds > 0.0f ? 1.0f / periodSeconds : lfoFreqHz_;
    }

    float processDelay(int which, DelayLine& line, OnePoleLowpass& highcut, Destination inDest, Destination modDest,
                        Destination ctModDest)
    {
        auto i = index2(which);
        auto baseMs = delayMs_[i] + beatsToMs(delayBpmBeats_[i]);
        auto modulatedMs = baseMs + delayModMs_[i] * read(modDest);
        auto delaySamples =
          std::clamp(modulatedMs * 0.001f * sampleRate_, 0.0f, static_cast<float>(kDelayCapacitySamples) - 2.0f);
        auto rawOut = line.readLinear(delaySamples);

        auto cutoffHz = std::clamp(highcutHz_[i] + highcutModHz_[i] * read(ctModDest), 20.0f, 20000.0f);
        highcut.setCoefficient(onePoleLowpassCoefficient(cutoffHz, sampleRate_));
        auto out = highcut.process(rawOut);

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

    float processDetune(int which, PitchShiftVoice& voice, Destination inDest, Destination modDest)
    {
        auto i = index2(which);
        auto totalMs = detuneDelayMs_[i] + beatsToMs(detuneBpmBeats_[i]);
        voice.setDelaySeconds(totalMs * 0.001f);
        auto totalCents = detuneCents_[i] + detuneModCents_[i] * read(modDest);
        voice.setSemitones(totalCents / 100.0f);
        return voice.process(read(inDest));
    }

    float sampleRate_ = 48000.0f;
    float bpm_ = 120.0f;
    float modKnob_ = 0.0f;
    float mix_ = 1.0f;

    DelayLine delay1_, delay2_;
    OnePoleLowpass highcut1_, highcut2_;
    std::array<float, 2> delayMs_{}, delayBpmBeats_{}, delayFeedback_{}, delayModMs_{};
    std::array<float, 2> highcutHz_{}, highcutModHz_{};
    std::array<bool, 2> delayLoop_{};

    PitchShiftVoice detune1_, detune2_;
    std::array<float, 2> detuneCents_{}, detuneDelayMs_{}, detuneBpmBeats_{}, detuneModCents_{}, detuneSpliceMs_{};

    MultiWaveLFO lfo_;
    float lfoFreqHz_ = 1.0f, lfoBpmBeats_ = 0.0f, lfoThresholdDb_ = -20.0f, lfoModHz_ = 0.0f;
    MultiWaveLFO::Waveform lfoWaveform_ = MultiWaveLFO::Waveform::kSine;

    EnvelopeDucker env_;
    float envAttackMs_ = 5.0f, envDecayMs_ = 100.0f, envThresholdDb_ = -20.0f, envRatio_ = 4.0f;

    std::array<float, 2> ampModAmount_{}, ampModOffset_{};
    std::array<float, 4> mixAAmount_{}, mixBAmount_{};
    std::array<float, 2> modScaleAmount_{};

    NoiseGenerator noise_;

    std::array<Source, static_cast<std::size_t>(Destination::kCount)> patch_{};
    std::array<float, static_cast<std::size_t>(Source::kCount)> previous_{};
};
}

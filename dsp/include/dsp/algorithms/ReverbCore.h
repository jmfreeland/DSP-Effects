#pragma once

#include "dsp/Crossover.h"
#include "dsp/Decay.h"
#include "dsp/DelayLine.h"
#include "dsp/Diffuser.h"
#include "dsp/Envelope.h"
#include "dsp/FeedbackMatrix.h"
#include "dsp/LFO.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * The reverb core shared by all five of the PCM81's 4-Voice/6-Voice
 * reverb algorithms (Concert Hall, Plate, Chamber, Inverse, Infinite) -
 * the manual describes them as one common signal path (Diffusion ->
 * PreDelay -> an 8-line Householder FDN tank with independent low/mid
 * decay rates, damping, and gentle modulation, plus a pair of
 * early-reflection taps) wrapped by a per-algorithm character and one or
 * two unique controls. This class implements that common path; each
 * concrete algorithm (dsp/algorithms/ConcertHall.h etc.) derives from it,
 * inheriting every generic control, and adds only what the manual calls
 * out as that algorithm's own (e.g. Plate's Attack, Chamber's
 * Shape/Spread). Two protected hooks exist for that: applyPreEcho() (the
 * Plate/Chamber/Infinite-only recirculating EkoDly/EkoFbk pre-echo) and
 * effectiveDiffusion() (lets a subclass shape diffusion dynamically, e.g.
 * Plate's Attack).
 *
 * Topology (mirrors the PCM81 user guide's reverb-core block diagram):
 *
 *   input -> RvbIn -> PreDelay -> Diffusion (series allpass chain) -> tank input
 *   L,R -> independent early-reflection taps (RefDly/RefLvl per channel) -> output
 *
 *   8-line Householder FDN tank, per line:
 *     tapped   = delay.read(length * Size +/- Spin/Chorus wobble)
 *     damped   = onePoleLowpass(tapped)              // Rt HC
 *     low,high = crossover(damped)                   // split at Crossover Hz
 *     decayed  = low*lowGain + high*midGain           // Low Rt / Mid Rt
 *   householderMix(decayed across all 8 lines), blended toward unmixed
 *   per-line values as tank energy drops (Definition)
 *   delay.write(diffused input + decayed)
 *   tank output * RvbOut, blended against early reflections by Depth
 *
 * This models the *style* of Lexicon's reverb cores (dense prime-length
 * diffusion/FDN, independently-decaying low/mid bands, early reflections
 * ahead of a damped tail, gentle modulation to avoid a metallic ring) -
 * it is an original implementation, not a reverse-engineered copy. See
 * docs/lexicon-pcm81-reference.md for the primary-source parameter
 * definitions this is built from, and docs/lexicon-pcm81-hall.md for the
 * remaining gaps against it (including which controls here, like
 * Definition and Depth, are original reconstructions of a *described*
 * behavior rather than a verified match).
 */
class ReverbCore
{
  public:
    static constexpr int kNumDiffusers = 4;
    static constexpr int kNumLines = 8;
    static constexpr int kModMargin = 16;

    // Pre-delay and early-reflection capacities are sized for 48kHz (the
    // Endless's native rate) regardless of the sample rate passed to
    // prepare() - the same simplification already used for tank lengths.
    static constexpr int kPreDelayCapacitySamples = 44650;        // ~930ms @ 48kHz
    static constexpr int kEarlyReflectionCapacitySamples = 57600; // 1.2s @ 48kHz

    static constexpr std::array<int, kNumDiffusers> kDiffuserLengths = { 211, 431, 751, 1091 };
    static constexpr std::array<int, kNumLines> kLineLengths =
      { 977, 1301, 1663, 2063, 2521, 3037, 3643, 4357 };

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        std::size_t total = 0;
        for (auto length : kDiffuserLengths)
        {
            total += static_cast<std::size_t>(length);
        }
        for (auto length : kLineLengths)
        {
            total += static_cast<std::size_t>(length) + kModMargin;
        }
        total += kPreDelayCapacitySamples;
        total += 2 * static_cast<std::size_t>(kEarlyReflectionCapacitySamples);
        return total;
    }

    virtual ~ReverbCore() = default;

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        for (int i = 0; i < kNumDiffusers; ++i)
        {
            auto length = static_cast<std::size_t>(kDiffuserLengths[i]);
            diffuser_.setStageBuffer(i, workingBuffer.subspan(offset, length));
            offset += length;
        }

        for (int i = 0; i < kNumLines; ++i)
        {
            auto length = static_cast<std::size_t>(kLineLengths[i]) + kModMargin;
            lines_[i].setBuffer(workingBuffer.subspan(offset, length));
            offset += length;
        }

        preDelayLine_.setBuffer(workingBuffer.subspan(offset, kPreDelayCapacitySamples));
        offset += kPreDelayCapacitySamples;

        earlyReflectionLineLeft_.setBuffer(
          workingBuffer.subspan(offset, kEarlyReflectionCapacitySamples));
        offset += kEarlyReflectionCapacitySamples;
        earlyReflectionLineRight_.setBuffer(
          workingBuffer.subspan(offset, kEarlyReflectionCapacitySamples));
        offset += kEarlyReflectionCapacitySamples;

        // Spin (slow, tail-wide movement) and Chorus (faster, per-line
        // delay randomization to kill metallic ringing) are split across
        // alternating lines rather than both applied everywhere - simpler,
        // and still gives every line some liveliness.
        static constexpr std::array<float, kNumLines> spinRatesHz =
          { 0.083f, 0.0f, 0.13f, 0.0f, 0.19f, 0.0f, 0.11f, 0.0f };
        static constexpr std::array<float, kNumLines> chorusRatesHz =
          { 0.0f, 0.6f, 0.0f, 0.9f, 0.0f, 1.3f, 0.0f, 0.7f };
        for (int i = 0; i < kNumLines; ++i)
        {
            spinLfo_[i].setFrequency(spinRatesHz[i], sampleRate_);
            spinLfo_[i].setPhase(static_cast<float>(i) / static_cast<float>(kNumLines));
            chorusLfo_[i].setFrequency(chorusRatesHz[i], sampleRate_);
            chorusLfo_[i].setPhase(static_cast<float>(i) / static_cast<float>(kNumLines) + 0.37f);
        }

        // Slow envelope follower (~20Hz smoothing) driving Definition.
        energyFollower_.setCoefficient(onePoleLowpassCoefficient(20.0f, sampleRate_));

        setDiffusion(0.6f);
        setSize(1.0f);
        setDecaySeconds(2.5f);
        setLowRatio(1.0f);
        setCrossoverFrequency(400.0f);
        setDamping(0.5f);
        setLink(false);
        setDefinition(0.0f);
        setDepth(0.5f);
        setRvbIn(1.0f);
        setRvbOut(1.0f);
        setPreDelaySeconds(0.0f);
        setEarlyReflectionLevel(0.2f, 0.2f);
        setEarlyReflectionDelaySeconds(0.03f, 0.03f);
        setSpin(0.5f);
        setChorus(0.3f);
        setMix(0.35f);
        reset();
    }

    // Master reverb time (RT60 for mid/high frequencies), in seconds.
    void setDecaySeconds(float seconds)
    {
        decaySeconds_ = std::max(seconds, 0.05f);
        updateDecayGains();
    }

    // Multiplier of decaySeconds() for low-frequency decay (Lexicon
    // recommends <=1.5 for a natural-sounding hall).
    void setLowRatio(float ratio)
    {
        lowRatio_ = std::max(ratio, 0.05f);
        updateDecayGains();
    }

    // Frequency (Hz) where decay hands off from the low-band rate to the
    // mid-band rate.
    void setCrossoverFrequency(float hz)
    {
        crossoverHz_ = hz;
        for (auto& c : crossover_)
        {
            c.setFrequency(crossoverHz_, sampleRate_);
        }
    }

    // 0 (bright/undamped) .. 1 (dark) - a single-pole high-cut on the
    // reverberated signal (Rt HC), independent of the low/mid split.
    void setDamping(float amount)
    {
        auto hz = mapLinear(1.0f - clamp01(amount), 1000.0f, 20000.0f);
        for (auto& d : damping_)
        {
            d.setCoefficient(onePoleLowpassCoefficient(hz, sampleRate_));
        }
    }

    // 0 (no diffusion) .. 1 (maximum initial echo density).
    void setDiffusion(float amount) { diffusionAmount_ = clamp01(amount); }

    // 0 (small room) .. 1 (large hall) - scales the tank's effective delay
    // lengths. Per the source material, changing Size briefly mutes the
    // wet signal rather than clicking.
    void setSize(float sizeNormalized)
    {
        auto newScale = mapLinear(sizeNormalized, 0.4f, 1.0f);
        if (std::fabs(newScale - sizeScale_) > 0.001f)
        {
            sizeMuteEnvelope_.trigger(0.0f, 1.0f, sampleRate_ * 0.03f);
        }
        sizeScale_ = newScale;
        updateDecayGains();
    }

    // When on, decay time scales together with Size instead of staying
    // independent of it.
    void setLink(bool linked)
    {
        linkEnabled_ = linked;
        updateDecayGains();
    }

    // 0 (no effect) .. 1: as the tank's own energy drops, progressively
    // reduces cross-line mixing so the late tail reads as discrete,
    // repeating echoes rather than a smooth wash. An original
    // reconstruction of "echo density buildup rate in the latter part of
    // the decay" driven by an energy envelope rather than elapsed time.
    void setDefinition(float amount) { definitionAmount_ = clamp01(amount); }

    // 0 (front: early reflections prominent) .. 1 (rear: diffuse tank
    // prominent) - an original reconstruction of "front-to-rear listener
    // perspective" as an early-reflection/tank balance.
    void setDepth(float amount) { depth_ = clamp01(amount); }

    // 0..1 level into the reverb core (independent of the dry signal used
    // for the final Mix blend).
    void setRvbIn(float level) { rvbInGain_ = clamp01(level); }

    // 0..1 level out of the tank specifically (early reflections are
    // unaffected, matching the source material).
    void setRvbOut(float level) { rvbOutGain_ = clamp01(level); }

    // Gap between input and the onset of reverberation, 0..0.93s.
    void setPreDelaySeconds(float seconds)
    {
        preDelaySamples_ = std::clamp(seconds * sampleRate_, 0.0f,
                                       static_cast<float>(kPreDelayCapacitySamples - 2));
    }

    // 0..1 level of each channel's independent early-reflection tap.
    void setEarlyReflectionLevel(float left, float right)
    {
        earlyReflectionLevelLeft_ = clamp01(left);
        earlyReflectionLevelRight_ = clamp01(right);
    }

    // Delay of each channel's independent early-reflection tap, 0..1.2s.
    void setEarlyReflectionDelaySeconds(float left, float right)
    {
        earlyReflectionDelaySamplesLeft_ =
          std::clamp(left * sampleRate_, 0.0f, static_cast<float>(kEarlyReflectionCapacitySamples - 2));
        earlyReflectionDelaySamplesRight_ = std::clamp(
          right * sampleRate_, 0.0f, static_cast<float>(kEarlyReflectionCapacitySamples - 2));
    }

    // 0..1: slow, tail-wide timbral movement so the tank doesn't settle
    // into a static, metallic set of resonances.
    void setSpin(float amount) { spinDepth_ = mapLinear(amount, 0.0f, 4.0f); }

    // 0..1: faster, per-line delay-time randomization for the same reason,
    // at a different character than Spin.
    void setChorus(float amount) { chorusDepth_ = mapLinear(amount, 0.0f, 6.0f); }

    // 0 (fully dry) .. 1 (fully wet).
    void setMix(float wet) { wet_ = clamp01(wet); }

    // While frozen, the tank recirculates near-losslessly and dry input is
    // excluded from it, so whatever is already ringing sustains
    // indefinitely (matches Lexicon's own "Infinite" freeze behavior).
    void setFrozen(bool frozen) { frozen_ = frozen; }

    void reset()
    {
        diffuser_.reset();
        for (auto& l : lines_)
        {
            l.reset();
        }
        for (auto& d : damping_)
        {
            d.reset();
        }
        for (auto& c : crossover_)
        {
            c.reset();
        }
        preDelayLine_.reset();
        earlyReflectionLineLeft_.reset();
        earlyReflectionLineRight_.reset();
        sizeMuteEnvelope_.reset();
        energyFollower_.reset();
    }

    void process(std::span<float> left, std::span<float> right)
    {
        for (std::size_t n = 0; n < left.size(); ++n)
        {
            processSample(left[n], right[n]);
        }
    }

    // Single-sample step, so a Graph can interleave this reverb Block with
    // other per-sample components (e.g. delay Voices) without needing
    // whole-block scratch buffers.
    void processSample(float& left, float& right)
    {
        auto earlyTapLeft = earlyReflectionLineLeft_.readLinear(earlyReflectionDelaySamplesLeft_);
        earlyReflectionLineLeft_.write(left);
        auto earlyTapRight = earlyReflectionLineRight_.readLinear(earlyReflectionDelaySamplesRight_);
        earlyReflectionLineRight_.write(right);

        auto preEchoLeft = left;
        auto preEchoRight = right;
        applyPreEcho(preEchoLeft, preEchoRight);

        auto dry = 0.5f * (preEchoLeft + preEchoRight) * rvbInGain_;

        auto preDelayed = preDelayLine_.readLinear(preDelaySamples_);
        preDelayLine_.write(dry);

        diffuser_.setDiffusion(effectiveDiffusion(diffusionAmount_));
        auto diffused = diffuser_.process(preDelayed);
        if (frozen_)
        {
            diffused = 0.0f;
        }

        std::array<float, kNumLines> tapped{};
        for (int i = 0; i < kNumLines; ++i)
        {
            auto modValue =
              spinLfo_[i].nextSine() * spinDepth_ + chorusLfo_[i].nextSine() * chorusDepth_;
            auto delaySamples = static_cast<float>(kLineLengths[i]) * sizeScale_ + modValue;
            tapped[i] = lines_[i].readLinear(delaySamples);
        }

        std::array<float, kNumLines> decayed{};
        for (int i = 0; i < kNumLines; ++i)
        {
            // While frozen, bypass the damping filter too - otherwise
            // Rt HC keeps eroding the tail every sample even though the
            // decay gains below are pushed to near-unity, so "freeze"
            // would still audibly decay over a few seconds instead of
            // ringing indefinitely (the behavior Infinite is built to
            // validate - see dsp/algorithms/Infinite.h).
            auto damped = frozen_ ? tapped[i] : damping_[i].process(tapped[i]);
            auto bands = crossover_[i].process(damped);
            auto lowGain = frozen_ ? 0.9999f : lowGain_[static_cast<std::size_t>(i)];
            auto midGain = frozen_ ? 0.9999f : midGain_[static_cast<std::size_t>(i)];
            decayed[i] = bands.low * lowGain + bands.high * midGain;
        }

        auto premix = decayed;
        householderMix(decayed);
        if (definitionAmount_ > 0.0f)
        {
            float energySum = 0.0f;
            for (auto v : decayed)
            {
                energySum += std::fabs(v);
            }
            auto envelope = energyFollower_.process(energySum);
            auto normalized = clamp01(envelope / 0.15f);
            auto mixAmount = 1.0f - definitionAmount_ * (1.0f - normalized);
            for (int i = 0; i < kNumLines; ++i)
            {
                decayed[i] = lerp(premix[i], decayed[i], mixAmount);
            }
        }

        static constexpr std::array<float, kNumLines> inputSign = { 1, -1, 1, -1, 1, -1, 1, -1 };
        for (int i = 0; i < kNumLines; ++i)
        {
            auto lineInput = diffused * inputSign[i] * 0.5f + decayed[i];
            lines_[i].write(lineInput);
        }

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        for (int i = 0; i < kNumLines; ++i)
        {
            auto tap = tapped[i];
            wetLeft += tap;
            wetRight += (i % 2 == 0) ? -tap : tap;
        }
        wetLeft *= 0.35f * rvbOutGain_;
        wetRight *= 0.35f * rvbOutGain_;

        auto earlyGain = 2.0f * (1.0f - depth_);
        auto tankGain = 2.0f * depth_;
        wetLeft = wetLeft * tankGain + earlyTapLeft * earlyReflectionLevelLeft_ * earlyGain;
        wetRight = wetRight * tankGain + earlyTapRight * earlyReflectionLevelRight_ * earlyGain;

        shapeWetOutput(wetLeft, wetRight);

        auto muteGain = sizeMuteEnvelope_.next();
        wetLeft *= muteGain;
        wetRight *= muteGain;

        left = lerp(left, wetLeft, wet_);
        right = lerp(right, wetRight, wet_);
    }

  protected:
    // Recirculating pre-echo (Lexicon's EkoDly/EkoFbk), only meaningful
    // for Plate/Chamber/Infinite per the manual - default no-op so Concert
    // Hall and Inverse are unaffected. Called on the raw L/R input, ahead
    // of the mono mix feeding PreDelay/Diffusion/the tank.
    virtual void applyPreEcho(float&, float&) {}

    // Lets a subclass shape the effective Diffusion coefficient
    // dynamically (e.g. Plate's Attack sharpening initial diffusion for
    // the first ~50ms). Called every sample; default passes the set
    // Diffusion amount through unchanged.
    virtual float effectiveDiffusion(float baseAmount) { return baseAmount; }

    // Lets a subclass scale the final wet output over time (e.g.
    // Chamber/Infinite's Shape+Spread onset swell). Called once per
    // sample via shapeWetOutput()'s default implementation; passes the
    // wet signal through unchanged unless overridden.
    virtual float outputEnvelope() { return 1.0f; }

    // Lets a subclass replace how the wet output is shaped entirely,
    // rather than just by a single scalar (e.g. Inverse's independent
    // Low Slope/Mid Slope needs to split wetLeft/wetRight into bands and
    // shape each separately). Deliberately a *read-path only* hook: it
    // must not be used to starve the recirculating feedback (the values
    // written back into the tank, computed earlier in processSample()
    // via lowGain_/midGain_, are unaffected by this), because a
    // near-zero gain there would prevent the tank from ever holding
    // enough energy to later "reveal" - which is exactly what a rising
    // envelope needs to do. Default multiplies by outputEnvelope().
    virtual void shapeWetOutput(float& wetLeft, float& wetRight)
    {
        auto gain = outputEnvelope();
        wetLeft *= gain;
        wetRight *= gain;
    }

    float sampleRate() const { return sampleRate_; }

  private:
    void updateDecayGains()
    {
        auto effectiveDecay = linkEnabled_ ? decaySeconds_ * sizeScale_ : decaySeconds_;
        for (int i = 0; i < kNumLines; ++i)
        {
            auto lineLengthSamples = static_cast<float>(kLineLengths[i]) * sizeScale_;
            midGain_[i] = rt60ToGain(lineLengthSamples, sampleRate_, effectiveDecay);
            lowGain_[i] = rt60ToGain(lineLengthSamples, sampleRate_, effectiveDecay * lowRatio_);
        }
    }

    float sampleRate_ = 48000.0f;
    float wet_ = 0.35f;
    bool frozen_ = false;

    DiffuserChain<kNumDiffusers> diffuser_;
    float diffusionAmount_ = 0.6f;
    std::array<DelayLine, kNumLines> lines_;
    std::array<OnePoleLowpass, kNumLines> damping_;
    std::array<Crossover, kNumLines> crossover_;
    std::array<LFO, kNumLines> spinLfo_;
    std::array<LFO, kNumLines> chorusLfo_;
    std::array<float, kNumLines> midGain_{};
    std::array<float, kNumLines> lowGain_{};
    OnePoleLowpass energyFollower_;

    DelayLine preDelayLine_;
    DelayLine earlyReflectionLineLeft_;
    DelayLine earlyReflectionLineRight_;
    LinearRamp sizeMuteEnvelope_;

    float sizeScale_ = 0.0f;
    float decaySeconds_ = 2.5f;
    float lowRatio_ = 1.0f;
    float crossoverHz_ = 400.0f;
    bool linkEnabled_ = false;
    float definitionAmount_ = 0.0f;
    float depth_ = 0.5f;
    float rvbInGain_ = 1.0f;
    float rvbOutGain_ = 1.0f;
    float preDelaySamples_ = 0.0f;
    float earlyReflectionLevelLeft_ = 0.2f;
    float earlyReflectionLevelRight_ = 0.2f;
    float earlyReflectionDelaySamplesLeft_ = 0.0f;
    float earlyReflectionDelaySamplesRight_ = 0.0f;
    float spinDepth_ = 0.0f;
    float chorusDepth_ = 0.0f;
};
}

#include "dsp/algorithms/Chamber.h"
#include "dsp/algorithms/Infinite.h"
#include "dsp/algorithms/Inverse.h"
#include "dsp/algorithms/Plate.h"
#include "dsp/graphs/ConcertHallAlgorithm.h"
#include "dsp/graphs/DiatonicShiftAlgorithm.h"
#include "dsp/graphs/DualShiftAlgorithm.h"
#include "dsp/graphs/LayeredShiftAlgorithm.h"
#include "dsp/graphs/ReverseShiftAlgorithm.h"
#include "dsp/graphs/StereoShiftAlgorithm.h"
#include "host/WavWriter.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
constexpr int kSampleRate = 48000;

struct RunResult
{
    bool ok = true;
};

void checkFinite(const std::vector<float>& left, const std::vector<float>& right, RunResult& result)
{
    for (auto v : left)
    {
        if (!std::isfinite(v))
        {
            std::fprintf(stderr, "FAIL: non-finite sample in left channel\n");
            result.ok = false;
            break;
        }
    }
    for (auto v : right)
    {
        if (!std::isfinite(v))
        {
            std::fprintf(stderr, "FAIL: non-finite sample in right channel\n");
            result.ok = false;
            break;
        }
    }
}

void printDecayCurve(const std::vector<float>& left, const std::vector<float>& right, int seconds)
{
    for (int s = 0; s < seconds; ++s)
    {
        auto start = static_cast<std::size_t>(s) * kSampleRate;
        auto windowSize = static_cast<std::size_t>(kSampleRate) / 10; // 100ms
        if (start + windowSize > left.size())
        {
            break;
        }
        double sum = 0.0;
        for (std::size_t i = start; i < start + windowSize; ++i)
        {
            sum += static_cast<double>(left[i]) * left[i] + static_cast<double>(right[i]) * right[i];
        }
        auto rms = std::sqrt(sum / (2.0 * windowSize));
        auto db = rms > 0.0 ? 20.0 * std::log10(rms) : -999.0;
        std::printf("  t=%2ds  RMS=%.6f  (%.1f dBFS)\n", s, rms, db);
    }
}

RunResult renderConcertHall(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::ConcertHallAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::ConcertHallAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setDecaySeconds(3.0f);
    engine.setLowRatio(1.3f);
    engine.setCrossoverFrequency(400.0f);
    engine.setDamping(0.4f);
    engine.setDiffusion(0.65f);
    engine.setSize(1.0f);
    engine.setPreDelaySeconds(0.02f);
    engine.setEarlyReflectionLevel(0.2f, 0.2f);
    engine.setSpin(0.5f);
    engine.setChorus(0.3f);
    engine.setDefinition(0.2f);
    engine.setDepth(0.5f);
    engine.setMix(1.0f);

    // Impulse response: unit impulse into an otherwise silent buffer.
    {
        const int seconds = 6;
        std::vector<float> left(kSampleRate * seconds, 0.0f);
        std::vector<float> right(kSampleRate * seconds, 0.0f);
        left[0] = 1.0f;
        right[0] = 1.0f;

        engine.process(left, right);
        checkFinite(left, right, result);

        std::printf("concert_hall impulse response decay:\n");
        printDecayCurve(left, right, seconds);

        auto path = outDir + "/concert_hall_impulse.wav";
        if (!host::writeStereoWav(path, left, right, kSampleRate))
        {
            std::fprintf(stderr, "FAIL: could not write %s\n", path.c_str());
            result.ok = false;
        }
        else
        {
            std::printf("wrote %s\n", path.c_str());
        }
    }

    // Test tone: a short dry chord-like burst so the tail is audible against
    // sustained material, not just a click.
    {
        engine.reset();
        engine.setDecaySeconds(3.0f);
        engine.setLowRatio(1.3f);
        engine.setDamping(0.4f);
        engine.setMix(0.4f);

        const int seconds = 5;
        const int burstSamples = kSampleRate / 2; // 500ms tone burst
        std::vector<float> left(kSampleRate * seconds, 0.0f);
        std::vector<float> right(kSampleRate * seconds, 0.0f);

        const float freqs[3] = { 220.0f, 277.18f, 329.63f }; // A3 minor-ish triad
        for (int i = 0; i < burstSamples; ++i)
        {
            float sample = 0.0f;
            for (float freq : freqs)
            {
                sample += std::sin(2.0f * 3.14159265f * freq * static_cast<float>(i) / kSampleRate);
            }
            sample *= 0.15f;
            // Simple fade-in/out envelope on the burst to avoid clicks.
            float envelope = 1.0f;
            const int fadeSamples = kSampleRate / 50;
            if (i < fadeSamples)
            {
                envelope = static_cast<float>(i) / fadeSamples;
            }
            else if (i > burstSamples - fadeSamples)
            {
                envelope = static_cast<float>(burstSamples - i) / fadeSamples;
            }
            left[i] = sample * envelope;
            right[i] = sample * envelope;
        }

        engine.process(left, right);
        checkFinite(left, right, result);

        auto path = outDir + "/concert_hall_tone.wav";
        if (!host::writeStereoWav(path, left, right, kSampleRate))
        {
            std::fprintf(stderr, "FAIL: could not write %s\n", path.c_str());
            result.ok = false;
        }
        else
        {
            std::printf("wrote %s\n", path.c_str());
        }
    }

    return result;
}

RunResult renderPlate(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::algorithms::Plate::requiredWorkingBufferSize());
    dsp::algorithms::Plate engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setDecaySeconds(2.2f);
    engine.setLowRatio(1.0f);
    engine.setCrossoverFrequency(400.0f);
    engine.setPreDelaySeconds(0.01f);
    engine.setEarlyReflectionLevel(0.25f, 0.25f);
    engine.setMix(1.0f);

    // Impulse response: unit impulse into an otherwise silent buffer.
    {
        const int seconds = 5;
        std::vector<float> left(kSampleRate * seconds, 0.0f);
        std::vector<float> right(kSampleRate * seconds, 0.0f);
        left[0] = 1.0f;
        right[0] = 1.0f;

        engine.process(left, right);
        checkFinite(left, right, result);

        std::printf("plate impulse response decay:\n");
        printDecayCurve(left, right, seconds);

        auto path = outDir + "/plate_impulse.wav";
        if (!host::writeStereoWav(path, left, right, kSampleRate))
        {
            std::fprintf(stderr, "FAIL: could not write %s\n", path.c_str());
            result.ok = false;
        }
        else
        {
            std::printf("wrote %s\n", path.c_str());
        }
    }

    // Test tone: a short dry chord-like burst so the tail is audible
    // against sustained material, not just a click, and so Attack's
    // effect on a real onset is audible.
    {
        engine.reset();
        engine.setMix(0.4f);

        const int seconds = 4;
        const int burstSamples = kSampleRate / 2; // 500ms tone burst
        std::vector<float> left(kSampleRate * seconds, 0.0f);
        std::vector<float> right(kSampleRate * seconds, 0.0f);

        const float freqs[3] = { 220.0f, 277.18f, 329.63f }; // A3 minor-ish triad
        for (int i = 0; i < burstSamples; ++i)
        {
            float sample = 0.0f;
            for (float freq : freqs)
            {
                sample += std::sin(2.0f * 3.14159265f * freq * static_cast<float>(i) / kSampleRate);
            }
            sample *= 0.15f;
            float envelope = 1.0f;
            const int fadeSamples = kSampleRate / 50;
            if (i < fadeSamples)
            {
                envelope = static_cast<float>(i) / fadeSamples;
            }
            else if (i > burstSamples - fadeSamples)
            {
                envelope = static_cast<float>(burstSamples - i) / fadeSamples;
            }
            left[i] = sample * envelope;
            right[i] = sample * envelope;
        }

        engine.process(left, right);
        checkFinite(left, right, result);

        auto path2 = outDir + "/plate_tone.wav";
        if (!host::writeStereoWav(path2, left, right, kSampleRate))
        {
            std::fprintf(stderr, "FAIL: could not write %s\n", path2.c_str());
            result.ok = false;
        }
        else
        {
            std::printf("wrote %s\n", path2.c_str());
        }
    }

    return result;
}

RunResult renderChamber(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::algorithms::Chamber::requiredWorkingBufferSize());
    dsp::algorithms::Chamber engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setDecaySeconds(2.8f);
    engine.setLowRatio(1.0f);
    engine.setCrossoverFrequency(400.0f);
    engine.setPreDelaySeconds(0.01f);
    engine.setEarlyReflectionLevel(0.2f, 0.2f);
    engine.setMix(1.0f);

    // Impulse response: unit impulse into an otherwise silent buffer.
    {
        const int seconds = 5;
        std::vector<float> left(kSampleRate * seconds, 0.0f);
        std::vector<float> right(kSampleRate * seconds, 0.0f);
        left[0] = 1.0f;
        right[0] = 1.0f;

        engine.process(left, right);
        checkFinite(left, right, result);

        std::printf("chamber impulse response decay:\n");
        printDecayCurve(left, right, seconds);

        auto path = outDir + "/chamber_impulse.wav";
        if (!host::writeStereoWav(path, left, right, kSampleRate))
        {
            std::fprintf(stderr, "FAIL: could not write %s\n", path.c_str());
            result.ok = false;
        }
        else
        {
            std::printf("wrote %s\n", path.c_str());
        }
    }

    // Test tone: a short dry chord-like burst so the Shape/Spread swell
    // on the onset is audible against the sustained tail.
    {
        engine.reset();
        engine.setMix(0.4f);

        const int seconds = 4;
        const int burstSamples = kSampleRate / 2; // 500ms tone burst
        std::vector<float> left(kSampleRate * seconds, 0.0f);
        std::vector<float> right(kSampleRate * seconds, 0.0f);

        const float freqs[3] = { 220.0f, 277.18f, 329.63f }; // A3 minor-ish triad
        for (int i = 0; i < burstSamples; ++i)
        {
            float sample = 0.0f;
            for (float freq : freqs)
            {
                sample += std::sin(2.0f * 3.14159265f * freq * static_cast<float>(i) / kSampleRate);
            }
            sample *= 0.15f;
            float envelope = 1.0f;
            const int fadeSamples = kSampleRate / 50;
            if (i < fadeSamples)
            {
                envelope = static_cast<float>(i) / fadeSamples;
            }
            else if (i > burstSamples - fadeSamples)
            {
                envelope = static_cast<float>(burstSamples - i) / fadeSamples;
            }
            left[i] = sample * envelope;
            right[i] = sample * envelope;
        }

        engine.process(left, right);
        checkFinite(left, right, result);

        auto path2 = outDir + "/chamber_tone.wav";
        if (!host::writeStereoWav(path2, left, right, kSampleRate))
        {
            std::fprintf(stderr, "FAIL: could not write %s\n", path2.c_str());
            result.ok = false;
        }
        else
        {
            std::printf("wrote %s\n", path2.c_str());
        }
    }

    return result;
}

RunResult renderInfinite(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::algorithms::Infinite::requiredWorkingBufferSize());
    dsp::algorithms::Infinite engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setMix(1.0f);

    // Impulse, then freeze partway through and confirm the tail holds
    // near-losslessly rather than continuing to decay to silence.
    {
        const int seconds = 6;
        std::vector<float> left(kSampleRate * seconds, 0.0f);
        std::vector<float> right(kSampleRate * seconds, 0.0f);
        left[0] = 1.0f;
        right[0] = 1.0f;

        const int freezeAtSample = kSampleRate; // freeze 1s in
        for (int n = 0; n < static_cast<int>(left.size()); ++n)
        {
            if (n == freezeAtSample)
            {
                engine.setFrozen(true);
            }
            engine.processSample(left[n], right[n]);
        }
        checkFinite(left, right, result);

        std::printf("infinite impulse response (frozen at t=1s):\n");
        printDecayCurve(left, right, seconds);

        auto path = outDir + "/infinite_impulse.wav";
        if (!host::writeStereoWav(path, left, right, kSampleRate))
        {
            std::fprintf(stderr, "FAIL: could not write %s\n", path.c_str());
            result.ok = false;
        }
        else
        {
            std::printf("wrote %s\n", path.c_str());
        }
    }

    return result;
}

RunResult renderInverse(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::algorithms::Inverse::requiredWorkingBufferSize());
    dsp::algorithms::Inverse engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setDuration(1.5f);
    engine.setLowSlope(-0.4f);
    engine.setMidSlope(-0.4f);
    engine.setDiffusion(0.6f);
    engine.setMix(1.0f);

    // Impulse response: unit impulse into an otherwise silent buffer -
    // shows the decay-slope envelope's shape and hard cutoff at Duration.
    {
        const int seconds = 4;
        std::vector<float> left(kSampleRate * seconds, 0.0f);
        std::vector<float> right(kSampleRate * seconds, 0.0f);
        left[0] = 1.0f;
        right[0] = 1.0f;

        engine.process(left, right);
        checkFinite(left, right, result);

        std::printf("inverse impulse response (Duration=1.5s, decay slope):\n");
        printDecayCurve(left, right, seconds);

        auto path = outDir + "/inverse_impulse.wav";
        if (!host::writeStereoWav(path, left, right, kSampleRate))
        {
            std::fprintf(stderr, "FAIL: could not write %s\n", path.c_str());
            result.ok = false;
        }
        else
        {
            std::printf("wrote %s\n", path.c_str());
        }
    }

    // Same again with a rising (gated-reverse) envelope, for comparison.
    {
        engine.reset();
        engine.setLowSlope(0.6f);
        engine.setMidSlope(0.6f);

        const int seconds = 4;
        std::vector<float> left(kSampleRate * seconds, 0.0f);
        std::vector<float> right(kSampleRate * seconds, 0.0f);
        left[0] = 1.0f;
        right[0] = 1.0f;

        engine.process(left, right);
        checkFinite(left, right, result);

        std::printf("inverse impulse response (Duration=1.5s, rise slope):\n");
        printDecayCurve(left, right, seconds);

        auto path = outDir + "/inverse_rise_impulse.wav";
        if (!host::writeStereoWav(path, left, right, kSampleRate))
        {
            std::fprintf(stderr, "FAIL: could not write %s\n", path.c_str());
            result.ok = false;
        }
        else
        {
            std::printf("wrote %s\n", path.c_str());
        }
    }

    return result;
}

RunResult renderDiatonicShift(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::DiatonicShiftAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::DiatonicShiftAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setKey(0); // C major
    engine.setScale(dsp::Scale::kMajor);
    engine.setLeftVoice(dsp::HarmonicInterval::kThirdUp);
    engine.setRightVoice(dsp::HarmonicInterval::kFifthUp);
    // Feedback stays at 0 for this render: once nonzero, the shifted
    // Voice outputs mix back into the shared mono input ahead of the
    // pitch tracker (matching the real algorithm's own topology, see
    // docs/eventide-diatonic-shift.md), and this engine's simple
    // autocorrelation tracker - unlike the real hardware's own tunable
    // "Source: polyphonic/solo" tracking, not implemented here - isn't
    // robust to that self-generated second pitch. That's a documented
    // simplification, not a bug, but it would just look like noise in a
    // single-note demo render, so it's isolated out here to keep this
    // render's story clean: pitch tracking driving per-note-correct
    // diatonic harmony.
    engine.setLeftFeedback(0.0f);
    engine.setRightFeedback(0.0f);
    engine.setLeftMix(1.0f);
    engine.setRightMix(1.0f);

    // A sustained D (293.66Hz, the 2nd scale degree in C major), then
    // silence - real-time pitch tracking should recognize D and harmonize
    // Left Voice a diatonic 3rd up (F, 3 semitones - not the 4 semitones a
    // fixed transposition from the root would give) and Right Voice a
    // diatonic 5th up (A, 7 semitones), proving the shift is derived from
    // the actual tracked note rather than a fixed interval.
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate; ++n)
    {
        auto sample = 0.4f * std::sin(2.0f * 3.14159265f * 293.66f * n / kSampleRate);
        left[static_cast<std::size_t>(n)] = sample;
        right[static_cast<std::size_t>(n)] = sample;
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("diatonic shift tone burst (D=293.66Hz, C major, Left=+3rd, Right=+5th):\n");
    std::printf("  tracked frequency: %.1fHz\n", engine.trackedFrequencyHz());
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/diatonic_shift_burst.wav";
    if (!host::writeStereoWav(path, left, right, kSampleRate))
    {
        std::fprintf(stderr, "FAIL: could not write %s\n", path.c_str());
        result.ok = false;
    }
    else
    {
        std::printf("wrote %s\n", path.c_str());
    }

    return result;
}

RunResult renderLayeredShift(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::LayeredShiftAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::LayeredShiftAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setLeftCents(400.0f);  // a major 3rd up
    engine.setRightCents(700.0f); // a perfect 5th up
    engine.setLeftFeedback(0.0f);
    engine.setRightFeedback(0.0f);
    engine.setLeftMix(1.0f);
    engine.setRightMix(1.0f);

    // A sustained 220Hz (A3) tone into the Left input only - the manual's
    // own Description names "the left input" as the sole source (see
    // dsp/algorithms/LayeredShift.h) - producing a fixed +400/+700 cent
    // shift on Left/Right Voice: no pitch tracking, so the shift amount is
    // constant regardless of the input note (unlike Diatonic Shift).
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate; ++n)
    {
        left[static_cast<std::size_t>(n)] = 0.4f * std::sin(2.0f * 3.14159265f * 220.0f * n / kSampleRate);
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("layered shift tone burst (Left In=220Hz, Left=+400c, Right=+700c):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/layered_shift_burst.wav";
    if (!host::writeStereoWav(path, left, right, kSampleRate))
    {
        std::fprintf(stderr, "FAIL: could not write %s\n", path.c_str());
        result.ok = false;
    }
    else
    {
        std::printf("wrote %s\n", path.c_str());
    }

    return result;
}

RunResult renderDualShift(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::DualShiftAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::DualShiftAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setLeftCents(-1200.0f);  // an octave down
    engine.setRightCents(1200.0f);  // an octave up
    engine.setLeftFeedback(0.0f);
    engine.setRightFeedback(0.0f);
    engine.setLeftMix(1.0f);
    engine.setRightMix(1.0f);

    // Independent tones into Left (220Hz) and Right (330Hz) - Dual Shift's
    // two channels never interact (see dsp/algorithms/DualShift.h), so
    // Left should come out an octave below 220Hz and Right an octave
    // above 330Hz, regardless of each other.
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate; ++n)
    {
        left[static_cast<std::size_t>(n)] = 0.4f * std::sin(2.0f * 3.14159265f * 220.0f * n / kSampleRate);
        right[static_cast<std::size_t>(n)] = 0.4f * std::sin(2.0f * 3.14159265f * 330.0f * n / kSampleRate);
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("dual shift tone burst (Left In=220Hz/-1oct, Right In=330Hz/+1oct):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/dual_shift_burst.wav";
    if (!host::writeStereoWav(path, left, right, kSampleRate))
    {
        std::fprintf(stderr, "FAIL: could not write %s\n", path.c_str());
        result.ok = false;
    }
    else
    {
        std::printf("wrote %s\n", path.c_str());
    }

    return result;
}

RunResult renderStereoShift(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::StereoShiftAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::StereoShiftAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setCents(700.0f); // a perfect 5th up, shared by both channels
    engine.setFeedback(0.0f);
    engine.setMix(1.0f);

    // A true stereo pair (220Hz Left, 220Hz Right, in phase) shifted by
    // the same shared interval on both channels - Stereo Shift's whole
    // point is that one set of controls drives a genuine stereo pair
    // identically (see dsp/algorithms/StereoShift.h), unlike Dual Shift's
    // independently-settable channels.
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate; ++n)
    {
        auto sample = 0.4f * std::sin(2.0f * 3.14159265f * 220.0f * n / kSampleRate);
        left[static_cast<std::size_t>(n)] = sample;
        right[static_cast<std::size_t>(n)] = sample;
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("stereo shift tone burst (L=R=220Hz, shared +700c):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/stereo_shift_burst.wav";
    if (!host::writeStereoWav(path, left, right, kSampleRate))
    {
        std::fprintf(stderr, "FAIL: could not write %s\n", path.c_str());
        result.ok = false;
    }
    else
    {
        std::printf("wrote %s\n", path.c_str());
    }

    return result;
}

RunResult renderReverseShift(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::ReverseShiftAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::ReverseShiftAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setLeftLengthSeconds(0.15f);
    engine.setRightLengthSeconds(0.15f);
    engine.setLeftCents(0.0f);
    engine.setRightCents(0.0f);
    engine.setLeftFeedback(0.0f);
    engine.setRightFeedback(0.0f);
    engine.setLeftMix(1.0f);
    engine.setRightMix(1.0f);

    // A short tone burst (0.3s) followed by silence - each 150ms splice
    // should come back reversed (see dsp/algorithms/ReverseShift.h), so
    // this is mainly a finite-output/stability check rather than a
    // frequency-accuracy one (reversing a steady sine is inaudible as a
    // pitch difference by design, unlike the smooth-shift algorithms).
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate / 3; ++n)
    {
        auto sample = 0.4f * std::sin(2.0f * 3.14159265f * 220.0f * n / kSampleRate);
        left[static_cast<std::size_t>(n)] = sample;
        right[static_cast<std::size_t>(n)] = sample;
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("reverse shift tone burst (220Hz for 0.3s, 150ms splice length):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/reverse_shift_burst.wav";
    if (!host::writeStereoWav(path, left, right, kSampleRate))
    {
        std::fprintf(stderr, "FAIL: could not write %s\n", path.c_str());
        result.ok = false;
    }
    else
    {
        std::printf("wrote %s\n", path.c_str());
    }

    return result;
}
}

int main(int argc, char** argv)
{
    std::string algorithm = "concert_hall";
    std::string outDir = "out";

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.rfind("--out=", 0) == 0)
        {
            outDir = arg.substr(6);
        }
        else
        {
            algorithm = arg;
        }
    }

    std::filesystem::create_directories(outDir);

    RunResult result;
    if (algorithm == "concert_hall")
    {
        result = renderConcertHall(outDir);
    }
    else if (algorithm == "plate")
    {
        result = renderPlate(outDir);
    }
    else if (algorithm == "chamber")
    {
        result = renderChamber(outDir);
    }
    else if (algorithm == "infinite")
    {
        result = renderInfinite(outDir);
    }
    else if (algorithm == "inverse")
    {
        result = renderInverse(outDir);
    }
    else if (algorithm == "diatonic_shift")
    {
        result = renderDiatonicShift(outDir);
    }
    else if (algorithm == "layered_shift")
    {
        result = renderLayeredShift(outDir);
    }
    else if (algorithm == "dual_shift")
    {
        result = renderDualShift(outDir);
    }
    else if (algorithm == "stereo_shift")
    {
        result = renderStereoShift(outDir);
    }
    else if (algorithm == "reverse_shift")
    {
        result = renderReverseShift(outDir);
    }
    else
    {
        std::fprintf(stderr, "Unknown algorithm '%s'\n", algorithm.c_str());
        return 1;
    }

    return result.ok ? 0 : 1;
}

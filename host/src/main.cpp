#include "dsp/algorithms/Chamber.h"
#include "dsp/algorithms/Infinite.h"
#include "dsp/algorithms/Inverse.h"
#include "dsp/algorithms/Plate.h"
#include "dsp/graphs/BandDelayAlgorithm.h"
#include "dsp/graphs/ConcertHallAlgorithm.h"
#include "dsp/graphs/DenseRoomAlgorithm.h"
#include "dsp/graphs/DiatonicShiftAlgorithm.h"
#include "dsp/graphs/DualDigiplexAlgorithm.h"
#include "dsp/graphs/DualShiftAlgorithm.h"
#include "dsp/graphs/LayeredShiftAlgorithm.h"
#include "dsp/graphs/LongDigiplexAlgorithm.h"
#include "dsp/graphs/MultiShiftAlgorithm.h"
#include "dsp/graphs/ModFactoryOneAlgorithm.h"
#include "dsp/graphs/PatchFactoryAlgorithm.h"
#include "dsp/graphs/PhaserAlgorithm.h"
#include "dsp/graphs/ReverbFactoryAlgorithm.h"
#include "dsp/graphs/ReverseShiftAlgorithm.h"
#include "dsp/graphs/StereoShiftAlgorithm.h"
#include "dsp/graphs/StringModellerAlgorithm.h"
#include "dsp/graphs/StudioSamplerAlgorithm.h"
#include "dsp/graphs/StutterAlgorithm.h"
#include "dsp/graphs/SweptCombsAlgorithm.h"
#include "dsp/graphs/SweptReverbAlgorithm.h"
#include "dsp/graphs/TimesqueezeAlgorithm.h"
#include "dsp/graphs/UltraTapAlgorithm.h"
#include "dsp/graphs/VocoderAlgorithm.h"
#include "host/WavWriter.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <span>
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

RunResult renderSweptCombs(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::SweptCombsAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::SweptCombsAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setMix(0.6f);

    // An impulse into six independently-swept feedback comb lines should
    // produce a dense, wandering cloud of repeats rather than a single
    // clean echo - the RMS/decay curve is the main signal here (this
    // algorithm has no single "correct frequency" to check the way the
    // pitch-shift algorithms do).
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    left[0] = right[0] = 0.8f;

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("swept combs impulse response:\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/swept_combs_burst.wav";
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

RunResult renderSweptReverb(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::SweptReverbAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::SweptReverbAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setMix(0.7f);
    engine.setFeedback(0.7f);

    // An impulse into the six-line Householder-mixed feedback network
    // (see dsp/algorithms/SweptReverb.h) should build into a dense,
    // continuous reverb tail rather than six discrete echoes, then decay
    // - the RMS/decay curve is the main signal here.
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    left[0] = right[0] = 0.8f;

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("swept reverb impulse response:\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/swept_reverb_burst.wav";
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

RunResult renderReverbFactory(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::ReverbFactoryAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::ReverbFactoryAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setMix(0.7f);
    engine.setOnDecaySeconds(2.5f);
    engine.setOffDecaySeconds(0.6f);
    engine.setGateThreshold(0.1f);
    engine.setGateTimeSeconds(0.3f);

    // A loud 100ms burst should trigger the Gate open (On decay/EQ, the
    // longer 2.5s tail), then the Gate closes after Gate Time elapses and
    // the tail continues at the shorter Off decay (0.6s) - see
    // dsp/algorithms/ReverbFactory.h.
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate / 10; ++n)
    {
        left[static_cast<std::size_t>(n)] = right[static_cast<std::size_t>(n)] = 0.5f;
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("reverb factory gated burst (0.5 amplitude for 0.1s, On=2.5s/Off=0.6s decay):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/reverb_factory_burst.wav";
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

RunResult renderUltraTap(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::UltraTapAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::UltraTapAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setMix(1.0f);

    // An impulse through the diffusor (4 cascaded Allpasses) then the
    // 12-tap cumulative delay line should spread into a dense field of
    // delays over time rather than staying a single click - see
    // dsp/algorithms/UltraTap.h.
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    left[0] = right[0] = 0.8f;

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("ultra-tap impulse response:\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/ultra_tap_burst.wav";
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

RunResult renderLongDigiplex(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::LongDigiplexAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::LongDigiplexAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setDelaySeconds(0.4f);
    engine.setFeedback(0.5f);
    engine.setMix(1.0f);

    // A short burst into the Left input only should come back as a
    // cascade of repeats spaced 0.4s apart, decaying at Feedback=0.5, on
    // both output channels equally - see dsp/algorithms/LongDigiplex.h.
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate / 10; ++n)
    {
        left[static_cast<std::size_t>(n)] = 0.5f;
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("long digiplex tone burst (0.4s delay, 0.5 feedback):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/long_digiplex_burst.wav";
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

RunResult renderDualDigiplex(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::DualDigiplexAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::DualDigiplexAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setGlide(50.0f, false);
    engine.setLeftDelaySeconds(0.2f);
    engine.setRightDelaySeconds(0.5f);
    engine.setLeftFeedback(0.4f);
    engine.setRightFeedback(0.4f);
    engine.setLeftMix(1.0f);
    engine.setRightMix(1.0f);
    engine.setStereoInput(true);

    // Independent bursts into Left (short delay) and Right (long delay)
    // should echo at their own independent times - Dual Digiplex's two
    // channels never interact, see dsp/algorithms/DualDigiplex.h.
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate / 10; ++n)
    {
        left[static_cast<std::size_t>(n)] = 0.5f;
        right[static_cast<std::size_t>(n)] = 0.5f;
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("dual digiplex tone burst (L delay=0.2s, R delay=0.5s, feedback=0.4):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/dual_digiplex_burst.wav";
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

RunResult renderPatchFactory(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::PatchFactoryAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::PatchFactoryAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setLeftMix(1.0f);
    engine.setRightMix(1.0f);

    // Factory-default patch: a burst into Left Input should reach Left
    // Output via Scale2->Sum2->Delay1->Filter1(Lowpass), and Right Output
    // via the pitch shifter and Filter2(Lowpass) - see
    // dsp/algorithms/PatchFactory.h for the full default routing.
    const int seconds = 1;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate / 10; ++n)
    {
        left[static_cast<std::size_t>(n)] = 0.5f;
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("patch factory tone burst (factory default patch):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/patch_factory_burst.wav";
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

RunResult renderStutter(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::StutterAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::StutterAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setLeftMix(1.0f);
    engine.setRightMix(1.0f);
    engine.setLength1(0.1f);
    engine.setCount1(4);

    // A steady tone, allowed to play normally for the first second, then
    // stuttered for the second - the tone should sound "normal" up to
    // the trigger, then repeat a short captured window (see
    // dsp/algorithms/Stutter.h and dsp/StutterCapture.h).
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate * seconds; ++n)
    {
        auto sample =
          0.5f * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(n) / static_cast<float>(kSampleRate));
        left[static_cast<std::size_t>(n)] = sample;
        right[static_cast<std::size_t>(n)] = sample;
    }

    std::span<float> firstHalfLeft(left.data(), static_cast<std::size_t>(kSampleRate));
    std::span<float> firstHalfRight(right.data(), static_cast<std::size_t>(kSampleRate));
    engine.process(firstHalfLeft, firstHalfRight);

    engine.triggerStutter1();

    std::span<float> secondHalfLeft(left.data() + kSampleRate, static_cast<std::size_t>(kSampleRate));
    std::span<float> secondHalfRight(right.data() + kSampleRate, static_cast<std::size_t>(kSampleRate));
    engine.process(secondHalfLeft, secondHalfRight);

    checkFinite(left, right, result);

    std::printf("stutter tone burst (normal for 1s, then stutter1 triggered, Length=0.1s, Count=4):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/stutter_burst.wav";
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

RunResult renderTimesqueeze(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::TimesqueezeAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::TimesqueezeAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setTimePercent(100.0f); // tape sped up 2x -> compensate down an octave
    engine.setPitchRatio(1.0f);

    const int seconds = 1;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate * seconds; ++n)
    {
        auto sample =
          0.5f * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(n) / static_cast<float>(kSampleRate));
        left[static_cast<std::size_t>(n)] = sample;
        right[static_cast<std::size_t>(n)] = sample;
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("timesqueeze tone burst (Time=100%%, compensating pitch shift down an octave):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/timesqueeze_burst.wav";
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

RunResult renderDenseRoom(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::DenseRoomAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::DenseRoomAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setMix(1.0f);
    engine.setRevTimeSeconds(2.5f);

    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    left[0] = 0.6f;

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("dense room impulse (RevTime=2.5s):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/dense_room_impulse.wav";
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

RunResult renderVocoder(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::VocoderAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::VocoderAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setMix(1.0f);

    // Left (synthesis) = noise ("harmonically rich," per the manual's
    // own Hint), Right (analysis) = a steady tone standing in for a
    // voice - the vocoder should impress the analysis envelope onto the
    // synthesis signal (see dsp/algorithms/Vocoder.h).
    const int seconds = 1;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    uint32_t rngState = 0xC0FFEEu;
    for (int n = 0; n < kSampleRate * seconds; ++n)
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        auto noise = static_cast<float>(static_cast<int32_t>(rngState)) / 2147483648.0f;
        left[static_cast<std::size_t>(n)] = noise * 0.5f;
        right[static_cast<std::size_t>(n)] =
          0.6f * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(n) / static_cast<float>(kSampleRate));
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("vocoder (noise synthesis input, tone analysis input):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/vocoder_burst.wav";
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

RunResult renderMultiShift(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::MultiShiftAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::MultiShiftAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setLeftCents(700.0f);
    engine.setRightCents(-700.0f);
    engine.setMix(1.0f);

    const int seconds = 1;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate * seconds; ++n)
    {
        auto sample =
          0.4f * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(n) / static_cast<float>(kSampleRate));
        left[static_cast<std::size_t>(n)] = sample;
        right[static_cast<std::size_t>(n)] = sample;
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("multi-shift (L=+700c, R=-700c):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/multi_shift_burst.wav";
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

RunResult renderBandDelay(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::BandDelayAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::BandDelayAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setMix(1.0f);
    engine.setFeedback(20.0f);

    const int seconds = 1;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int i = 0; i < 4; ++i)
    {
        left[static_cast<std::size_t>(i)] = (i % 2 == 0) ? 0.8f : -0.8f;
    }
    right = left;

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("band delay (8-tap impulse, Feedback=20%%):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/band_delay_burst.wav";
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

RunResult renderStringModeller(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::StringModellerAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::StringModellerAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setMix(100.0f);
    engine.setHighAmt(0.0f);
    engine.setBandAmt(0.0f);
    engine.setLowAmt(0.0f);
    engine.setInAmt(0.0f);
    engine.setChorus(40.0f);
    engine.setDecay(75.0f);
    engine.setBright(55.0f);

    // Silence, then a manual "pluck" (trigger()) of all six strings - no
    // continuous noise stimulation at all, so the six strings' Karplus-Strong
    // ringing is the only thing audible (see dsp/algorithms/StringModeller.h
    // for why triggering stands in for the manual's MIDI note-on).
    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);

    engine.trigger();
    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("string modeller (six strings plucked, guitar tuning, Decay=75%%):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/string_modeller_pluck.wav";
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

RunResult renderPhaser(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::PhaserAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::PhaserAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setMix(70.0f);
    engine.setSweepMode(dsp::algorithms::Phaser::SweepMode::kLfo);
    engine.setSweepRate(70.0f);
    engine.setSweepBottom(10.0f);
    engine.setSweepTop(80.0f);

    // A steady tone swept by the LFO - the notches should audibly (and
    // measurably, in RMS) move over time rather than sitting still.
    const int seconds = 1;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate * seconds; ++n)
    {
        auto sample =
          0.4f * std::sin(2.0f * 3.14159265f * 800.0f * static_cast<float>(n) / static_cast<float>(kSampleRate));
        left[static_cast<std::size_t>(n)] = sample;
        right[static_cast<std::size_t>(n)] = sample;
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("phaser (800Hz tone, LFO sweep, Mix=70%%):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/phaser_sweep.wav";
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

RunResult renderStudioSampler(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::StudioSamplerAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::StudioSamplerAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);
    engine.setMix(100.0f);
    engine.setShiftMode(0, dsp::SamplerVoice::ShiftMode::kConstantLength);
    engine.setShiftMode(1, dsp::SamplerVoice::ShiftMode::kConstantLength);
    engine.setPitchCents(0, 1200.0f); // left: shift up an octave, same duration
    engine.setPitchCents(1, 0.0f);    // right: pitch unchanged
    engine.setTimePercent(0, 100.0f);
    engine.setTimePercent(1, 200.0f); // right: play back twice as fast, same pitch

    // Record half a second of a 300Hz tone into both channels, stop, then
    // play both back - Left demonstrates independent Pitch (up an
    // octave, same length), Right demonstrates independent Time (2x
    // speed, same pitch).
    const int recordSeconds = 1;
    std::vector<float> recordLeft(kSampleRate * recordSeconds, 0.0f);
    std::vector<float> recordRight(kSampleRate * recordSeconds, 0.0f);
    for (int n = 0; n < kSampleRate * recordSeconds; ++n)
    {
        auto sample =
          0.4f * std::sin(2.0f * 3.14159265f * 300.0f * static_cast<float>(n) / static_cast<float>(kSampleRate));
        recordLeft[static_cast<std::size_t>(n)] = sample;
        recordRight[static_cast<std::size_t>(n)] = sample;
    }
    engine.record(0);
    engine.record(1);
    engine.process(recordLeft, recordRight);
    engine.stop(0);
    engine.stop(1);
    engine.play(0);
    engine.play(1);

    const int playSeconds = 1;
    std::vector<float> left(kSampleRate * playSeconds, 0.0f);
    std::vector<float> right(kSampleRate * playSeconds, 0.0f);
    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("studio sampler (300Hz recorded, Left=Pitch+1200c, Right=Time200%%):\n");
    printDecayCurve(left, right, playSeconds);

    auto path = outDir + "/studio_sampler_playback.wav";
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

RunResult renderModFactoryOne(const std::string& outDir)
{
    RunResult result;

    static std::vector<float> working(dsp::graphs::ModFactoryOneAlgorithm::requiredWorkingBufferSize());
    dsp::graphs::ModFactoryOneAlgorithm engine;
    engine.prepare(static_cast<float>(kSampleRate), working);

    using MF1 = dsp::algorithms::ModFactoryOne;
    // A manual-flanger patch, exactly matching the module doc's own
    // example: LFO 1 modulates Delay 1, mixed with the dry signal.
    engine.setPatch(MF1::Destination::kDly1In, MF1::Source::kLeftInput);
    engine.setPatch(MF1::Destination::kDly1Mod, MF1::Source::kLfo1);
    engine.setDelayMs(0, 8.0f);
    engine.setDelayModMs(0, 6.0f);
    engine.setDelayFeedback(0, 20.0f);
    engine.setLfoFrequency(0, 0.3f);
    engine.setLfoWaveform(0, dsp::MultiWaveLFO::Waveform::kTriangle);
    engine.setPatch(MF1::Destination::kMix1aIn, MF1::Source::kLeftInput);
    engine.setPatch(MF1::Destination::kMix1bIn, MF1::Source::kDelay1);
    engine.setMixAAmount(0, 50.0f);
    engine.setMixBAmount(0, 50.0f);
    engine.setPatch(MF1::Destination::kLeftOut, MF1::Source::kMixer1);
    engine.setPatch(MF1::Destination::kRightOut, MF1::Source::kMixer1);
    engine.setMix(100.0f);

    const int seconds = 2;
    std::vector<float> left(kSampleRate * seconds, 0.0f);
    std::vector<float> right(kSampleRate * seconds, 0.0f);
    for (int n = 0; n < kSampleRate * seconds; ++n)
    {
        auto sample =
          0.4f * std::sin(2.0f * 3.14159265f * 300.0f * static_cast<float>(n) / static_cast<float>(kSampleRate));
        left[static_cast<std::size_t>(n)] = sample;
        right[static_cast<std::size_t>(n)] = sample;
    }

    engine.process(left, right);
    checkFinite(left, right, result);

    std::printf("mod factory one (manual flanger patch: LFO1 -> Delay1 mod, Delay1+dry mixed):\n");
    printDecayCurve(left, right, seconds);

    auto path = outDir + "/mod_factory_one_flanger.wav";
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
    else if (algorithm == "swept_combs")
    {
        result = renderSweptCombs(outDir);
    }
    else if (algorithm == "swept_reverb")
    {
        result = renderSweptReverb(outDir);
    }
    else if (algorithm == "reverb_factory")
    {
        result = renderReverbFactory(outDir);
    }
    else if (algorithm == "ultra_tap")
    {
        result = renderUltraTap(outDir);
    }
    else if (algorithm == "long_digiplex")
    {
        result = renderLongDigiplex(outDir);
    }
    else if (algorithm == "dual_digiplex")
    {
        result = renderDualDigiplex(outDir);
    }
    else if (algorithm == "patch_factory")
    {
        result = renderPatchFactory(outDir);
    }
    else if (algorithm == "stutter")
    {
        result = renderStutter(outDir);
    }
    else if (algorithm == "timesqueeze")
    {
        result = renderTimesqueeze(outDir);
    }
    else if (algorithm == "dense_room")
    {
        result = renderDenseRoom(outDir);
    }
    else if (algorithm == "vocoder")
    {
        result = renderVocoder(outDir);
    }
    else if (algorithm == "multi_shift")
    {
        result = renderMultiShift(outDir);
    }
    else if (algorithm == "band_delay")
    {
        result = renderBandDelay(outDir);
    }
    else if (algorithm == "string_modeller")
    {
        result = renderStringModeller(outDir);
    }
    else if (algorithm == "phaser")
    {
        result = renderPhaser(outDir);
    }
    else if (algorithm == "studio_sampler")
    {
        result = renderStudioSampler(outDir);
    }
    else if (algorithm == "mod_factory_one")
    {
        result = renderModFactoryOne(outDir);
    }
    else
    {
        std::fprintf(stderr, "Unknown algorithm '%s'\n", algorithm.c_str());
        return 1;
    }

    return result.ok ? 0 : 1;
}

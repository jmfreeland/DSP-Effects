#include "dsp/algorithms/Chamber.h"
#include "dsp/algorithms/Plate.h"
#include "dsp/graphs/ConcertHallAlgorithm.h"
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
    else
    {
        std::fprintf(stderr, "Unknown algorithm '%s'\n", algorithm.c_str());
        return 1;
    }

    return result.ok ? 0 : 1;
}

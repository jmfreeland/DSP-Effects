#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>

namespace host
{
// Minimal stereo 16-bit PCM WAV writer for offline listening/inspection of
// dsp/ algorithms. Not part of the embedded build.
inline bool writeStereoWav(const std::string& path,
                           std::span<const float> left,
                           std::span<const float> right,
                           int sampleRate)
{
    auto numFrames = std::min(left.size(), right.size());

    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr)
    {
        return false;
    }

    auto writeU32 = [&](std::uint32_t v) { std::fwrite(&v, sizeof(v), 1, f); };
    auto writeU16 = [&](std::uint16_t v) { std::fwrite(&v, sizeof(v), 1, f); };

    const std::uint16_t numChannels = 2;
    const std::uint16_t bitsPerSample = 16;
    const std::uint32_t byteRate =
      static_cast<std::uint32_t>(sampleRate) * numChannels * (bitsPerSample / 8);
    const std::uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    const std::uint32_t dataSize = static_cast<std::uint32_t>(numFrames) * blockAlign;

    std::fwrite("RIFF", 1, 4, f);
    writeU32(36 + dataSize);
    std::fwrite("WAVE", 1, 4, f);

    std::fwrite("fmt ", 1, 4, f);
    writeU32(16);
    writeU16(1); // PCM
    writeU16(numChannels);
    writeU32(static_cast<std::uint32_t>(sampleRate));
    writeU32(byteRate);
    writeU16(blockAlign);
    writeU16(bitsPerSample);

    std::fwrite("data", 1, 4, f);
    writeU32(dataSize);

    for (std::size_t i = 0; i < numFrames; ++i)
    {
        auto toInt16 = [](float sample)
        {
            auto clamped = std::clamp(sample, -1.0f, 1.0f);
            return static_cast<std::int16_t>(clamped * 32767.0f);
        };
        std::int16_t l = toInt16(left[i]);
        std::int16_t r = toInt16(right[i]);
        std::fwrite(&l, sizeof(l), 1, f);
        std::fwrite(&r, sizeof(r), 1, f);
    }

    std::fclose(f);
    return true;
}
}

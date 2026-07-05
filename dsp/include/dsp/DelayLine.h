#pragma once

#include <cstddef>
#include <span>

namespace dsp
{
/**
 * Non-owning circular delay line.
 *
 * Storage is provided by the caller (e.g. a slice of a Patch's working
 * buffer, or a fixed-size array held by an engine) so this type never
 * allocates. Capacity is fixed for the lifetime of the buffer assignment;
 * the maximum readable delay is buffer().size() - 1 samples.
 */
class DelayLine
{
  public:
    void setBuffer(std::span<float> buffer)
    {
        buffer_ = buffer;
        writeIndex_ = 0;
        for (auto& s : buffer_)
        {
            s = 0.0f;
        }
    }

    std::size_t size() const { return buffer_.size(); }

    void write(float sample)
    {
        buffer_[writeIndex_] = sample;
        writeIndex_ = writeIndex_ + 1 == buffer_.size() ? 0 : writeIndex_ + 1;
    }

    // Integer-delay read. delaySamples == 0 returns the most recently written sample.
    // delaySamples is clamped to the buffer's addressable range.
    float read(std::size_t delaySamples) const
    {
        auto capacity = buffer_.size();
        auto d = delaySamples >= capacity ? capacity - 1 : delaySamples;
        auto idx = (writeIndex_ + capacity - 1 - d) % capacity;
        return buffer_[idx];
    }

    // Fractional-delay read via linear interpolation between adjacent taps.
    float readLinear(float delaySamples) const
    {
        auto delayFloor = static_cast<std::size_t>(delaySamples);
        auto frac = delaySamples - static_cast<float>(delayFloor);
        auto a = read(delayFloor);
        auto b = read(delayFloor + 1);
        return a + (b - a) * frac;
    }

    void reset()
    {
        for (auto& s : buffer_)
        {
            s = 0.0f;
        }
        writeIndex_ = 0;
    }

  private:
    std::span<float> buffer_;
    std::size_t writeIndex_ = 0;
};
}

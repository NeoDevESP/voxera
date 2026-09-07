#pragma once
#include <algorithm>
#include <vector>

namespace voxera
{
// One gain step per frame, shared by every channel.
template <typename Buffer, typename Smoother>
void applyLinkedGain(Buffer& buffer, Smoother& gain)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float g = gain.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer(ch)[i] *= g;
    }
}

// Storage is allocated only during prepare. One call per channel per frame.
class IntegerDelay
{
public:
    void prepare(int channels, int delaySamples)
    {
        length = std::max(1, delaySamples + 1);
        delay = std::max(0, delaySamples);
        storage.assign(static_cast<size_t>(channels), std::vector<float>(static_cast<size_t>(length), 0.0f));
        position = 0;
    }
    void reset()
    {
        for (auto& channel : storage) std::fill(channel.begin(), channel.end(), 0.0f);
        position = 0;
    }
    // Changes the read offset within storage already allocated by prepare, so a
    // latency switch costs an integer write rather than a reallocation on the
    // audio thread. Prepare for the largest delay the chain can ask for.
    void setDelay(int delaySamples) noexcept
    {
        delay = std::max(0, std::min(delaySamples, length - 1));
    }

    int getDelay() const noexcept { return delay; }

    float process(int channel, float input) noexcept
    {
        auto& data = storage[static_cast<size_t>(channel)];
        data[static_cast<size_t>(position)] = input;
        return data[static_cast<size_t>((position + length - delay) % length)];
    }
    void advance() noexcept { position = (position + 1) % length; }
private:
    std::vector<std::vector<float>> storage;
    int position = 0, length = 1, delay = 0;
};
}

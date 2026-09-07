#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

/*
    Allocation-free in the audio path.
    4-point cubic Hermite interpolation gives smoother modulation than
    integer/linear reads, which matters for the doubler and tempo changes.
*/
class FractionalDelay
{
public:
    void prepare(double sampleRate, double maximumDelaySeconds)
    {
        sr = sampleRate;
        const auto wanted = static_cast<size_t>(
            std::ceil(maximumDelaySeconds * sampleRate)) + 8u;

        buffer.assign(juce::jmax<size_t>(wanted, 16u), 0.0f);
        writeIndex = 0;
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    void push(float sample) noexcept
    {
        buffer[writeIndex] = sample;
        if (++writeIndex >= buffer.size())
            writeIndex = 0;
    }

    float read(float delaySamples) const noexcept
    {
        if (buffer.empty())
            return 0.0f;

        const float maxDelay = static_cast<float>(buffer.size() - 4u);
        const float d = juce::jlimit(1.0f, maxDelay, delaySamples);

        float readPos = static_cast<float>(writeIndex) - 1.0f - d;
        const float size = static_cast<float>(buffer.size());

        while (readPos < 0.0f)
            readPos += size;
        while (readPos >= size)
            readPos -= size;

        const int i1 = static_cast<int>(std::floor(readPos));
        const float t = readPos - static_cast<float>(i1);

        const float y0 = atWrapped(i1 - 1);
        const float y1 = atWrapped(i1);
        const float y2 = atWrapped(i1 + 1);
        const float y3 = atWrapped(i1 + 2);

        // Cubic Hermite interpolation.
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    float msToSamples(float ms) const noexcept
    {
        return ms * 0.001f * static_cast<float>(sr);
    }

private:
    float atWrapped(int index) const noexcept
    {
        const int n = static_cast<int>(buffer.size());
        while (index < 0) index += n;
        while (index >= n) index -= n;
        return buffer[static_cast<size_t>(index)];
    }

    double sr = 48000.0;
    std::vector<float> buffer;
    size_t writeIndex = 0;
};

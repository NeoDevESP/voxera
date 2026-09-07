#pragma once
#include <JuceHeader.h>
#include "RealtimeUtilities.h"
#include <atomic>
#include <cmath>
#include <vector>

namespace voxera
{
// Minimum of the last N pushed values in amortised constant time. The wedge
// holds only values that can still win, so the front is always the answer.
class SlidingMinimum
{
public:
    void prepare(int windowLength)
    {
        length = std::max(1, windowLength);
        // One spare slot keeps an empty ring distinguishable from a full one.
        values.assign(static_cast<size_t>(length + 1), 1.0f);
        stamps.assign(static_cast<size_t>(length + 1), 0);
        reset();
    }

    void reset()
    {
        std::fill(values.begin(), values.end(), 1.0f);
        head = tail = 0;
        counter = 0;
    }

    float push(float value) noexcept
    {
        const size_t capacity = values.size();

        // A new value smaller than the entries behind it makes them unreachable.
        while (tail != head)
        {
            const size_t back = (tail + capacity - 1) % capacity;
            if (values[back] < value) break;
            tail = back;
        }

        values[tail] = value;
        stamps[tail] = counter;
        tail = (tail + 1) % capacity;

        if (stamps[head] + length <= counter)
            head = (head + 1) % capacity;

        ++counter;
        return values[head];
    }

private:
    std::vector<float> values;
    std::vector<long long> stamps;
    size_t head = 0, tail = 0;
    long long counter = 0;
    int length = 1;
};

/*  Look-ahead brickwall limiter.

    The audio is delayed by the look-ahead while the gain is derived from the
    undelayed signal, so the gain reduction for a peak is already applied by the
    time that peak reaches the output. Taking the sliding minimum over the
    look-ahead window is what makes that guarantee hold: the gain starts falling
    the moment a peak enters the window rather than when it arrives.

    This is a safety stage, not a loudness tool. It sits last so saturation,
    spatial feedback and output gain cannot push the chain past the ceiling.
*/
class Limiter
{
public:
    void prepare(double sr, int channels)
    {
        sampleRate = (std::isfinite(sr) && sr > 0.0) ? sr : 48000.0;
        numChannels = juce::jmax(1, channels);
        lookaheadSamples = juce::jmax(1, static_cast<int>(std::lround(sampleRate * lookaheadSeconds)));

        delay.prepare(numChannels, lookaheadSamples);
        minimum.prepare(lookaheadSamples);

        // Six time constants inside the look-ahead: the gain has settled before
        // the peak that asked for it arrives.
        attackCoeff = 1.0f - std::exp(-6.0f / static_cast<float>(lookaheadSamples));
        setReleaseMs(releaseMs);

        gain = 1.0f;
        reductionDb.store(0.0f, std::memory_order_relaxed);
    }

    void reset()
    {
        delay.reset();
        minimum.reset();
        gain = 1.0f;
        reductionDb.store(0.0f, std::memory_order_relaxed);
    }

    void setEnabled(bool shouldLimit) noexcept { enabled = shouldLimit; }
    void setCeilingDb(float db) noexcept { ceiling = juce::Decibels::decibelsToGain(db); }

    void setReleaseMs(float ms)
    {
        releaseMs = juce::jmax(1.0f, ms);
        releaseCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * releaseMs * 0.001f));
    }

    // Constant regardless of the enabled state: the delay always runs so the
    // reported latency never changes while the host is playing.
    int getLatencySamples() const noexcept { return lookaheadSamples; }
    float getReductionDb() const noexcept { return reductionDb.load(std::memory_order_relaxed); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        if (channels <= 0 || buffer.getNumSamples() == 0) return;

        float worst = 1.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                peak = juce::jmax(peak, std::abs(buffer.getReadPointer(ch)[i]));

            const float target = (enabled && peak > ceiling) ? ceiling / peak : 1.0f;
            const float floorGain = minimum.push(target);

            gain += (floorGain < gain ? attackCoeff : releaseCoeff) * (floorGain - gain);

            for (int ch = 0; ch < channels; ++ch)
            {
                auto& sample = buffer.getWritePointer(ch)[i];
                const float limited = delay.process(ch, sample) * gain;
                sample = enabled ? juce::jlimit(-ceiling, ceiling, limited) : limited;
            }

            delay.advance();
            worst = juce::jmin(worst, gain);
        }

        reductionDb.store(juce::Decibels::gainToDecibels(worst), std::memory_order_relaxed);
    }

private:
    static constexpr double lookaheadSeconds = 0.0015;

    IntegerDelay delay;
    SlidingMinimum minimum;
    std::atomic<float> reductionDb { 0.0f };

    double sampleRate = 48000.0;
    float ceiling = 1.0f, gain = 1.0f;
    float attackCoeff = 1.0f, releaseCoeff = 1.0f, releaseMs = 80.0f;
    int lookaheadSamples = 1, numChannels = 1;
    bool enabled = true;
};
}

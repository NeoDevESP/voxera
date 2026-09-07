#pragma once
#include <JuceHeader.h>
#include "RealtimeUtilities.h"
#include <cmath>
#include <memory>

class Saturator
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        capacity = juce::jmax(1, static_cast<int>(spec.maximumBlockSize));
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
            static_cast<size_t>(spec.numChannels), 2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampling->initProcessing(static_cast<size_t>(capacity));
        latency = static_cast<int>(std::lround(oversampling->getLatencyInSamples()));
        dry.setSize(static_cast<int>(spec.numChannels), capacity);
        dryDelay.prepare(static_cast<int>(spec.numChannels), latency);
        driveDb.reset(spec.sampleRate * 4.0, 0.030);
        mix.reset(spec.sampleRate, 0.030);
        driveDb.setCurrentAndTargetValue(0.0f);
        mix.setCurrentAndTargetValue(0.0f);
    }

    void reset()
    {
        if (oversampling) oversampling->reset();
        dryDelay.reset();
        dry.clear();
    }
    void setDriveDb(float db) { driveDb.setTargetValue(db); }
    void setMix(float wet) { mix.setTargetValue(juce::jlimit(0.0f, 1.0f, wet)); }
    float getLatencySamples() const noexcept { return static_cast<float>(latency); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (!oversampling) return;
        juce::dsp::AudioBlock<float> whole(buffer);
        // Hosts may exceed their advertised maximum block size.
        for (int offset = 0; offset < buffer.getNumSamples(); offset += capacity)
        {
            const int count = juce::jmin(capacity, buffer.getNumSamples() - offset);
            auto block = whole.getSubBlock(static_cast<size_t>(offset), static_cast<size_t>(count));
            for (int i = 0; i < count; ++i)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    dry.getWritePointer(ch)[i] = dryDelay.process(ch, block.getChannelPointer(static_cast<size_t>(ch))[i]);
                dryDelay.advance();
            }
            auto up = oversampling->processSamplesUp(block);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                const float drive = juce::Decibels::decibelsToGain(driveDb.getNextValue());
                const float norm = std::tanh(drive);
                for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
                {
                    auto& value = up.getChannelPointer(ch)[i];
                    value = std::tanh(value * drive) / norm;
                }
            }
            oversampling->processSamplesDown(block);
            for (int i = 0; i < count; ++i)
            {
                const float wet = mix.getNextValue();
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    auto& value = block.getChannelPointer(static_cast<size_t>(ch))[i];
                    value = wet * value + (1.0f - wet) * dry.getReadPointer(ch)[i];
                }
            }
        }
    }
private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> dry;
    voxera::IntegerDelay dryDelay;
    juce::SmoothedValue<float> driveDb, mix;
    int capacity = 1, latency = 0;
};

#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>

struct Metering
{
    void analyse(const juce::AudioBuffer<float>& buffer)
    {
        float peakValue = 0.0f;
        double sumSq = 0.0;
        int n = 0;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float s = data[i];
                peakValue = juce::jmax(peakValue, std::abs(s));
                sumSq += static_cast<double>(s) * static_cast<double>(s);
                ++n;
            }
        }

        const float rmsValue = n > 0 ? static_cast<float>(std::sqrt(sumSq / n)) : 0.0f;
        peakDb.store(juce::Decibels::gainToDecibels(peakValue, -120.0f));
        rmsDb.store(juce::Decibels::gainToDecibels(rmsValue, -120.0f));
    }

    std::atomic<float> peakDb { -120.0f };
    std::atomic<float> rmsDb  { -120.0f };
};

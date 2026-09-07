#pragma once
#include <JuceHeader.h>
#include <cmath>

// Stereo-linked sample-based RMS follower. Signal below -55 dBFS is not boosted.
class AutoGain
{
public:
    void prepare(double sampleRate)
    {
        detectorCoeff = static_cast<float>(std::exp(-1.0 / (0.050 * sampleRate)));
        attackCoeff = static_cast<float>(std::exp(-1.0 / (0.080 * sampleRate)));
        releaseCoeff = static_cast<float>(std::exp(-1.0 / (0.500 * sampleRate)));
        reset();
    }
    void reset() { energy = currentDb = 0.0f; }
    void setEnabled(bool value) { enabled = value; }
    void setTargetDb(float db) { targetDb = db; }
    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = buffer.getNumChannels();
        if (channels == 0) return;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float power = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                const float x = buffer.getReadPointer(ch)[i]; power += x * x;
            }
            energy = detectorCoeff * energy + (1.0f - detectorCoeff) * power / static_cast<float>(channels);
            const float rmsDb = 10.0f * std::log10(juce::jmax(1.0e-12f, energy));
            const float wanted = enabled && rmsDb > -55.0f
                ? juce::jlimit(-12.0f, 18.0f, targetDb - rmsDb) : 0.0f;
            const float coeff = wanted < currentDb ? attackCoeff : releaseCoeff;
            currentDb = wanted + coeff * (currentDb - wanted);
            const float gain = juce::Decibels::decibelsToGain(currentDb);
            for (int ch = 0; ch < channels; ++ch) buffer.getWritePointer(ch)[i] *= gain;
        }
    }
private:
    bool enabled = true;
    float targetDb = -18.0f, energy = 0.0f, currentDb = 0.0f;
    float detectorCoeff = 0.0f, attackCoeff = 0.0f, releaseCoeff = 0.0f;
};

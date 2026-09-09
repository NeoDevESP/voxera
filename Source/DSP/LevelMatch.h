#pragma once
#include <JuceHeader.h>
#include <cmath>

namespace voxera {
// Stereo-linked RMS comparison aid. Bounded, gated, and smoothed; not a LUFS meter.
class LevelMatch {
public:
    void prepare(double rate) {
        sr = rate; inputPower = outputPower = 0.0;
        gain.reset(rate, 0.25); gain.setCurrentAndTargetValue(1.0f);
    }
    void process(juce::AudioBuffer<float>& wet, const juce::AudioBuffer<float>& dry, bool enabled) {
        const int count = wet.getNumSamples(), channels = wet.getNumChannels();
        double a = 0.0, b = 0.0;
        for (int c = 0; c < channels; ++c) for (int i = 0; i < count; ++i) {
            const double x = dry.getSample(c, i), y = wet.getSample(c, i);
            a += x*x; b += y*y;
        }
        const double n = juce::jmax(1, count * channels);
        a /= n; b /= n;
        if (a > 1.0e-6 && b > 1.0e-8) {
            const double k = 1.0 - std::exp(-count / (sr * 2.0));
            inputPower += k * (a - inputPower); outputPower += k * (b - outputPower);
        }
        const float db = inputPower > 1.0e-10 && outputPower > 1.0e-10
            ? juce::jlimit(-36.0f, 9.0f, static_cast<float>(10.0 * std::log10(inputPower / outputPower))) : 0.0f;
        gain.setTargetValue(enabled ? juce::Decibels::decibelsToGain(db) : 1.0f);
        for (int i = 0; i < count; ++i) {
            const float g = gain.getNextValue();
            for (int c = 0; c < channels; ++c) wet.getWritePointer(c)[i] *= g;
        }
    }
private:
    double sr = 48000.0, inputPower = 0.0, outputPower = 0.0;
    juce::SmoothedValue<float> gain;
};
}

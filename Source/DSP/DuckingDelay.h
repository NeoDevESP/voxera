#pragma once
#include <JuceHeader.h>
#include "FractionalDelay.h"
#include <cmath>

class DuckingDelay
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        left.prepare(sr, 4.0);
        right.prepare(sr, 4.0);

        delaySamples.reset(sr, 0.080);
        delaySamples.setCurrentAndTargetValue(static_cast<float>(sr * 0.5));

        feedbackSmoothed.reset(sr, 0.040);
        feedbackSmoothed.setCurrentAndTargetValue(0.28f);

        const float cutoff = 7000.0f;
        dampingCoeff = 1.0f - std::exp(
            -juce::MathConstants<float>::twoPi * cutoff / static_cast<float>(sr));

        reset();
    }

    void reset()
    {
        left.reset();
        right.reset();
        lowpassL = lowpassR = 0.0f;
    }

    void setTempoAndDivision(double bpm, int divisionIndex)
    {
        const double safeBpm = juce::jlimit(40.0, 240.0, bpm);
        const double quarterMs = 60000.0 / safeBpm;

        double beats = 1.0;
        switch (divisionIndex)
        {
            case 0: beats = 0.5;       break; // 1/8
            case 1: beats = 1.0;       break; // 1/4
            case 2: beats = 1.5;       break; // dotted 1/4
            case 3: beats = 1.0 / 3.0; break; // 1/8 triplet
            case 4: beats = 2.0;       break; // 1/2
            default: break;
        }

        const double seconds = juce::jlimit(0.045, 3.9, quarterMs * beats * 0.001);
        delaySamples.setTargetValue(static_cast<float>(seconds * sr));
    }

    void setFeedback(float feedback01)
    {
        // Deliberately capped below unity for unconditional stability.
        feedbackSmoothed.setTargetValue(
            juce::jlimit(0.0f, 0.78f, feedback01 * 0.78f));
    }

    void processSample(float inL, float inR, float& wetL, float& wetR) noexcept
    {
        const float d = delaySamples.getNextValue();
        const float fb = feedbackSmoothed.getNextValue();

        // read() is relative to the last written frame; this read precedes push().
        const float tapL = left.read(d - 1.0f);
        const float tapR = right.read(d - 1.0f);

        // Darken repetitions so the delay moves behind the lead vocal.
        lowpassL += dampingCoeff * (tapL - lowpassL);
        lowpassR += dampingCoeff * (tapR - lowpassR);

        wetL = lowpassL;
        wetR = lowpassR;

        // Cross-feedback -> ping-pong behaviour on a stereo bus.
        left.push(inL + lowpassR * fb);
        right.push(inR + lowpassL * fb);
    }

private:
    double sr = 48000.0;
    FractionalDelay left, right;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySamples;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedbackSmoothed;

    float lowpassL = 0.0f, lowpassR = 0.0f;
    float dampingCoeff = 0.2f;
};

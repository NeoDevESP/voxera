#pragma once
#include <JuceHeader.h>
#include "FractionalDelay.h"
#include <array>
#include <vector>
#include <cmath>

class FDNReverb
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;

        // Mutually different delays reduce obvious periodicity.
        constexpr std::array<double, 4> delayMs { 43.7, 53.1, 61.7, 71.9 };

        for (size_t i = 0; i < lines.size(); ++i)
        {
            auto samples = static_cast<size_t>(
                std::round(delayMs[i] * 0.001 * sr));

            // Keep the lengths odd and non-identical.
            if ((samples & 1u) == 0u)
                ++samples;

            lines[i].data.assign(juce::jmax<size_t>(samples, 17u), 0.0f);
            lines[i].index = 0;
            lines[i].damping = 0.0f;
            lineDelaySeconds[i] =
                static_cast<float>(lines[i].data.size() / sr);
        }

        preDelay.prepare(sr, 0.100);

        preDelaySamples.reset(sr, 0.080);
        preDelaySamples.setCurrentAndTargetValue(
            static_cast<float>(0.012 * sr));

        damping.reset(sr, 0.080);
        damping.setCurrentAndTargetValue(0.42f);

        for (auto& g : feedback)
        {
            g.reset(sr, 0.100);
            g.setCurrentAndTargetValue(0.75f);
        }

        setSpace(0.35f);
        reset();
    }

    void reset()
    {
        for (auto& line : lines)
        {
            std::fill(line.data.begin(), line.data.end(), 0.0f);
            line.index = 0;
            line.damping = 0.0f;
        }

        preDelay.reset();
    }

    void setSpace(float space01)
    {
        const float s = juce::jlimit(0.0f, 1.0f, space01);

        // 0.65s -> 4.8s RT60.
        const float rt60 = 0.65f + 4.15f * std::pow(s, 1.35f);

        for (size_t i = 0; i < feedback.size(); ++i)
        {
            // Gain required for -60 dB after RT60:
            // g = 10^(-3 * delay_seconds / RT60)
            const float g = std::pow(
                10.0f, -3.0f * lineDelaySeconds[i] / juce::jmax(0.1f, rt60));

            feedback[i].setTargetValue(juce::jlimit(0.0f, 0.985f, g));
        }

        const float predelayMs = 8.0f + 30.0f * s;
        preDelaySamples.setTargetValue(
            predelayMs * 0.001f * static_cast<float>(sr));

        // Larger spaces become a little darker.
        damping.setTargetValue(0.56f - 0.25f * s);
    }

    void processSample(float inL, float inR, float& wetL, float& wetR) noexcept
    {
        const float mono = 0.5f * (inL + inR);
        const float side = 0.5f * (inL - inR);

        preDelay.push(mono);
        const float input = preDelay.read(preDelaySamples.getNextValue());

        std::array<float, 4> y {};
        for (size_t i = 0; i < lines.size(); ++i)
            y[i] = lines[i].data[lines[i].index];

        // Energy-normalised 4x4 Hadamard matrix.
        std::array<float, 4> h {
            0.5f * ( y[0] + y[1] + y[2] + y[3]),
            0.5f * ( y[0] - y[1] + y[2] - y[3]),
            0.5f * ( y[0] + y[1] - y[2] - y[3]),
            0.5f * ( y[0] - y[1] - y[2] + y[3])
        };

        const float d = damping.getNextValue();

        for (size_t i = 0; i < lines.size(); ++i)
        {
            // One-pole damping inside the feedback loop.
            lines[i].damping += d * (h[i] - lines[i].damping);

            const float injection =
                input * injectionSigns[i] * 0.24f
                + side * sideSigns[i] * 0.08f;

            const float write =
                injection + lines[i].damping * feedback[i].getNextValue();

            lines[i].data[lines[i].index] = write;

            if (++lines[i].index >= lines[i].data.size())
                lines[i].index = 0;
        }

        // Different orthogonal-ish projections decorrelate L/R.
        wetL = 0.28f * ( y[0] + y[1] - y[2] + y[3]);
        wetR = 0.28f * ( y[0] - y[1] + y[2] + y[3]);
    }

private:
    struct Line
    {
        std::vector<float> data;
        size_t index = 0;
        float damping = 0.0f;
    };

    double sr = 48000.0;
    std::array<Line, 4> lines;
    std::array<float, 4> lineDelaySeconds {};

    const std::array<float, 4> injectionSigns { 1.0f, 1.0f, -1.0f, 1.0f };
    const std::array<float, 4> sideSigns      { 1.0f, -1.0f, 1.0f, -1.0f };

    FractionalDelay preDelay;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> preDelaySamples;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> damping;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, 4> feedback;
};

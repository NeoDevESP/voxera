#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace voxera
{
/*  Cubic soft clipper with first-order antiderivative anti-aliasing.

    Clipping is what buys the last few dB of loudness without asking the limiter
    to pull down: peaks are rounded off where the ear barely registers it, so the
    average level can come up underneath.

    Any clipper generates harmonics past Nyquist, which fold back as inharmonic
    aliasing. The usual answer is to oversample, which costs CPU and latency.
    ADAA gets most of the way there for neither: instead of evaluating the curve
    at a point, it integrates it across the step from the previous sample and
    divides by the step's width. That average is what the sample should have
    been, and its high-frequency content is far weaker than a point evaluation's.
    When two consecutive samples are nearly equal the quotient loses precision,
    so the midpoint of the curve is used instead, which is what it converges to.

    A cubic is chosen over tanh because its antiderivative is a polynomial, so
    the whole stage stays a handful of multiplies.
*/
class SoftClip
{
public:
    void prepare(double sampleRate, int channels)
    {
        numChannels = juce::jlimit(1, 2, channels);
        previousX.fill(0.0f);
        previousF.fill(antiderivative(0.0f));
        amount.reset(sampleRate, 0.030);
        amount.setCurrentAndTargetValue(0.0f);
    }

    void reset()
    {
        previousX.fill(0.0f);
        previousF.fill(antiderivative(0.0f));
        amount.setCurrentAndTargetValue(amount.getTargetValue());
    }

    void setAmount(float normalised) { amount.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        if (channels <= 0 || buffer.getNumSamples() == 0) return;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float wet = amount.getNextValue();
            const float drive = 1.0f + 4.0f * wet;
            // Normalising by the curve at full scale keeps a peak at 1 mapping
            // to 1, so the drive raises everything below it rather than the top.
            const float normalise = 1.0f / shape(drive);

            for (int ch = 0; ch < channels; ++ch)
            {
                auto& sample = buffer.getWritePointer(ch)[i];
                const float dry = sample;
                const float x = dry * drive;

                auto& x0 = previousX[static_cast<size_t>(ch)];
                auto& f0 = previousF[static_cast<size_t>(ch)];
                const float f1 = antiderivative(x);
                const float step = x - x0;

                const float clipped = (std::abs(step) > 1.0e-5f)
                    ? (f1 - f0) / step
                    : shape(0.5f * (x + x0));

                x0 = x; f0 = f1;

                // At amount zero this is exactly the input, whatever the curve did.
                sample = dry + wet * (clipped * normalise - dry);
            }
        }
    }

private:
    // Cubic soft clip, flat beyond unity input.
    static float shape(float x) noexcept
    {
        if (x <= -1.0f) return -2.0f / 3.0f;
        if (x >= 1.0f) return 2.0f / 3.0f;
        return x - x * x * x / 3.0f;
    }

    // Its antiderivative, continuous at the two knees.
    static float antiderivative(float x) noexcept
    {
        const float a = std::abs(x);
        if (a >= 1.0f) return 2.0f / 3.0f * a - 0.25f;
        return x * x * 0.5f - x * x * x * x / 12.0f;
    }

    std::array<float, 2> previousX {}, previousF {};
    juce::SmoothedValue<float> amount;
    int numChannels = 2;
};
}

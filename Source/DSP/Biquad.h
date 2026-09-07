#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

class Biquad
{
public:
    void prepare(double sampleRate, int channels)
    {
        sr = sampleRate;
        numChannels = juce::jlimit(1, 2, channels);
        reset();
        setIdentity();
    }

    void reset()
    {
        z1.fill(0.0f);
        z2.fill(0.0f);
    }

    void setIdentity()
    {
        b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
        a1 = 0.0f; a2 = 0.0f;
    }

    void setPeaking(double frequency, double q, double gainDb)
    {
        const double f = juce::jlimit(20.0, 0.49 * sr, frequency);
        const double Q = juce::jmax(0.05, q);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * f / sr;
        const double alpha = std::sin(w0) / (2.0 * Q);
        const double cw = std::cos(w0);

        const double B0 = 1.0 + alpha * A;
        const double B1 = -2.0 * cw;
        const double B2 = 1.0 - alpha * A;
        const double A0 = 1.0 + alpha / A;
        const double A1 = -2.0 * cw;
        const double A2 = 1.0 - alpha / A;

        setNormalized(B0, B1, B2, A0, A1, A2);
    }

    void setLowShelf(double frequency, double gainDb, double slope = 1.0)
    {
        const double f = juce::jlimit(20.0, 0.49 * sr, frequency);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double S = juce::jlimit(0.1, 2.0, slope);
        const double w0 = juce::MathConstants<double>::twoPi * f / sr;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);
        const double alpha = sw / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
        const double beta = 2.0 * std::sqrt(A) * alpha;

        const double B0 = A * ((A + 1.0) - (A - 1.0) * cw + beta);
        const double B1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
        const double B2 = A * ((A + 1.0) - (A - 1.0) * cw - beta);
        const double A0 = (A + 1.0) + (A - 1.0) * cw + beta;
        const double A1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
        const double A2 = (A + 1.0) + (A - 1.0) * cw - beta;

        setNormalized(B0, B1, B2, A0, A1, A2);
    }

    void setHighShelf(double frequency, double gainDb, double slope = 1.0)
    {
        const double f = juce::jlimit(20.0, 0.49 * sr, frequency);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double S = juce::jlimit(0.1, 2.0, slope);
        const double w0 = juce::MathConstants<double>::twoPi * f / sr;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);
        const double alpha = sw / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
        const double beta = 2.0 * std::sqrt(A) * alpha;

        const double B0 = A * ((A + 1.0) + (A - 1.0) * cw + beta);
        const double B1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
        const double B2 = A * ((A + 1.0) + (A - 1.0) * cw - beta);
        const double A0 = (A + 1.0) - (A - 1.0) * cw + beta;
        const double A1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
        const double A2 = (A + 1.0) - (A - 1.0) * cw - beta;

        setNormalized(B0, B1, B2, A0, A1, A2);
    }


    // Constant 0 dB peak band-pass, used only by the Smart EQ detector.
    void setBandPass(double frequency, double q = 1.0)
    {
        const double w = juce::MathConstants<double>::twoPi * juce::jlimit(20.0, sr * 0.45, frequency) / sr;
        const double alpha = std::sin(w) / (2.0 * q);
        setNormalized(alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * std::cos(w), 1.0 - alpha);
    }

    void setHighPass(double frequency, double q = 0.70710678)
    {
        const double f = juce::jlimit(20.0, 0.49 * sr, frequency);
        const double Q = juce::jmax(0.05, q);
        const double w0 = juce::MathConstants<double>::twoPi * f / sr;
        const double alpha = std::sin(w0) / (2.0 * Q);
        const double cw = std::cos(w0);

        const double B0 = (1.0 + cw) * 0.5;
        const double B1 = -(1.0 + cw);
        const double B2 = (1.0 + cw) * 0.5;
        const double A0 = 1.0 + alpha;
        const double A1 = -2.0 * cw;
        const double A2 = 1.0 - alpha;

        setNormalized(B0, B1, B2, A0, A1, A2);
    }

    void setLowPass(double frequency, double q = 0.70710678)
    {
        const double f = juce::jlimit(20.0, 0.49 * sr, frequency);
        const double Q = juce::jmax(0.05, q);
        const double w0 = juce::MathConstants<double>::twoPi * f / sr;
        const double alpha = std::sin(w0) / (2.0 * Q);
        const double cw = std::cos(w0);

        const double B0 = (1.0 - cw) * 0.5;
        const double B1 = 1.0 - cw;
        const double B2 = (1.0 - cw) * 0.5;
        const double A0 = 1.0 + alpha;
        const double A1 = -2.0 * cw;
        const double A2 = 1.0 - alpha;

        setNormalized(B0, B1, B2, A0, A1, A2);
    }

    inline float processSample(int channel, float x) noexcept
    {
        const int ch = juce::jlimit(0, numChannels - 1, channel);
        const float y = b0 * x + z1[(size_t)ch];
        z1[(size_t)ch] = b1 * x - a1 * y + z2[(size_t)ch];
        z2[(size_t)ch] = b2 * x - a2 * y;
        return y;
    }

private:
    void setNormalized(double B0, double B1, double B2,
                       double A0, double A1, double A2)
    {
        const double invA0 = 1.0 / A0;
        b0 = static_cast<float>(B0 * invA0);
        b1 = static_cast<float>(B1 * invA0);
        b2 = static_cast<float>(B2 * invA0);
        a1 = static_cast<float>(A1 * invA0);
        a2 = static_cast<float>(A2 * invA0);
    }

    double sr = 48000.0;
    int numChannels = 2;

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;

    std::array<float, 2> z1 {};
    std::array<float, 2> z2 {};
};

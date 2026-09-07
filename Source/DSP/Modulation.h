#pragma once
#include <JuceHeader.h>
#include "FractionalDelay.h"
#include <array>
#include <cmath>

namespace voxera
{
/*  Flanger and phaser.

    Chorus is deliberately absent: the six-voice doubler upstream already is one,
    and a better one than a single modulated tap would be. What these two add is
    the thing a chorus cannot do — feedback, and therefore resonance.

    A flanger sweeps a delay short enough that the copy interferes with the
    original rather than being heard beside it, producing a comb whose teeth
    move. A phaser instead cascades all-pass sections, which leave the magnitude
    alone and rotate phase; summed with the dry signal that rotation becomes
    notches, but far fewer and less regularly spaced than a comb's. That is why
    a phaser sounds like movement and a flanger sounds like a jet.

    Both are summed with the dry signal here, because neither exists as an effect
    without it: the interference is the effect, and a wet-only flanger is just a
    slightly late copy.
*/
class Modulation
{
public:
    enum Type { off = 0, flanger, phaser, numTypes };

    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);

        for (auto& line : delays) line.prepare(sr, 0.030);
        for (auto& channel : allpass)
            for (auto& section : channel) section = 0.0f;

        feedbackState.fill(0.0f);

        rate.reset(sr, 0.050);
        depth.reset(sr, 0.050);
        mix.reset(sr, 0.050);
        rate.setCurrentAndTargetValue(0.4f);
        depth.setCurrentAndTargetValue(0.5f);
        mix.setCurrentAndTargetValue(0.0f);
        reset();
    }

    void reset()
    {
        for (auto& line : delays) line.reset();
        for (auto& channel : allpass)
            for (auto& section : channel) section = 0.0f;
        feedbackState.fill(0.0f);
        // A quarter turn apart, so the two channels sweep out of step and the
        // effect has width instead of moving the whole image together.
        phase[0] = 0.0;
        phase[1] = juce::MathConstants<double>::halfPi;
    }

    void setType(int newType) noexcept { type = juce::jlimit(0, numTypes - 1, newType); }
    void setRateHz(float hz) { rate.setTargetValue(juce::jlimit(0.02f, 8.0f, hz)); }
    void setDepth(float normalised) { depth.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }
    void setMix(float normalised) { mix.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        if (channels <= 0 || buffer.getNumSamples() == 0 || type == off) {
            // Still advance the smoothers, or the first block after switching on
            // jumps to target instead of ramping.
            rate.skip(buffer.getNumSamples());
            depth.skip(buffer.getNumSamples());
            mix.skip(buffer.getNumSamples());
            return;
        }

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float hz = rate.getNextValue();
            const float sweep = depth.getNextValue();
            const float wet = mix.getNextValue();
            const double increment = juce::MathConstants<double>::twoPi * hz / sr;

            for (int ch = 0; ch < channels; ++ch)
            {
                auto& sample = buffer.getWritePointer(ch)[i];
                const float dry = sample;
                const float lfo = 0.5f + 0.5f * static_cast<float>(std::sin(phase[static_cast<size_t>(ch)]));
                float processed = dry;

                if (type == flanger)
                {
                    auto& line = delays[static_cast<size_t>(ch)];
                    auto& fb = feedbackState[static_cast<size_t>(ch)];

                    // 0.6 to 7 ms: short enough to comb rather than to double.
                    const float ms = 0.6f + 6.4f * lfo * sweep;
                    line.push(dry + feedbackAmount * sweep * fb);
                    fb = line.read(line.msToSamples(ms));
                    processed = fb;
                }
                else
                {
                    /*  Four all-pass sections sweeping together. Their corner
                        moves between 300 Hz and 2 kHz, which is where a voice
                        has enough energy for the notches to be audible at all.
                    */
                    const float corner = 300.0f + 1700.0f * lfo * sweep;
                    const float tanHalf = std::tan(juce::MathConstants<float>::pi
                                                   * corner / static_cast<float>(sr));
                    const float coeff = (tanHalf - 1.0f) / (tanHalf + 1.0f);

                    float x = dry + feedbackAmount * sweep * feedbackState[static_cast<size_t>(ch)];
                    for (auto& state : allpass[static_cast<size_t>(ch)])
                    {
                        const float y = coeff * x + state;
                        state = x - coeff * y;
                        x = y;
                    }
                    feedbackState[static_cast<size_t>(ch)] = x;
                    processed = x;
                }

                sample = dry + wet * processed;
                phase[static_cast<size_t>(ch)] += increment;
                if (phase[static_cast<size_t>(ch)] >= juce::MathConstants<double>::twoPi)
                    phase[static_cast<size_t>(ch)] -= juce::MathConstants<double>::twoPi;
            }
        }
    }

private:
    // Well short of unity: the resonance is the character, self-oscillation is
    // a fault.
    static constexpr float feedbackAmount = 0.55f;

    std::array<FractionalDelay, 2> delays;
    std::array<std::array<float, 4>, 2> allpass {};
    std::array<float, 2> feedbackState {};
    std::array<double, 2> phase { 0.0, juce::MathConstants<double>::halfPi };

    juce::SmoothedValue<float> rate, depth, mix;
    double sr = 48000.0;
    int numChannels = 2, type = off;
};

/*  Bus glue.

    Not another compressor in the sense the three upstream are. Those each fix
    something identifiable — peaks, syllable level, the floor. This one is set so
    low and so slow that it barely registers as gain reduction at all: a couple
    of decibels, a ratio near 2:1, an attack long enough to let every transient
    through untouched.

    What that does is make the parts move together. The whole signal breathes on
    one envelope rather than each stage having its own idea, and the result is
    heard as cohesion rather than as compression. Pushed harder it stops doing
    that and starts sounding like the compressors above it, which is why the
    range here is deliberately narrow.
*/
class Glue
{
public:
    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);

        detectorCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * detectorSeconds));
        attackCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * attackSeconds));
        releaseCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * releaseSeconds));

        amount.reset(sr, 0.050);
        amount.setCurrentAndTargetValue(0.0f);
        reset();
    }

    void reset()
    {
        detector = 0.0f;
        reduction = 0.0f;
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

            float peak = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                peak = juce::jmax(peak, std::abs(buffer.getReadPointer(ch)[i]));

            detector += detectorCoeff * (peak - detector);
            const float levelDb = juce::Decibels::gainToDecibels(detector, -96.0f);

            const float over = levelDb - thresholdDb;
            const float target = over > 0.0f ? juce::jmin(over * slope, maxReductionDb) * wet : 0.0f;

            reduction += (target > reduction ? attackCoeff : releaseCoeff) * (target - reduction);

            const float gain = juce::Decibels::decibelsToGain(-reduction + makeupDb * wet);
            for (int ch = 0; ch < channels; ++ch)
                buffer.getWritePointer(ch)[i] *= gain;
        }
    }

private:
    static constexpr float thresholdDb = -16.0f;
    static constexpr float slope = 0.45f;          // a shade under 2:1
    static constexpr float maxReductionDb = 3.0f;  // narrow on purpose
    static constexpr float makeupDb = 1.5f;
    static constexpr double detectorSeconds = 0.020;
    static constexpr double attackSeconds = 0.030;  // slow enough to pass transients
    static constexpr double releaseSeconds = 0.400;

    juce::SmoothedValue<float> amount;
    double sr = 48000.0;
    float detector = 0.0f, reduction = 0.0f;
    float detectorCoeff = 1.0f, attackCoeff = 1.0f, releaseCoeff = 1.0f;
    int numChannels = 2;
};
}

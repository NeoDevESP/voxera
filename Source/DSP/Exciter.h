#pragma once
#include <JuceHeader.h>
#include "Biquad.h"
#include <array>
#include <cmath>

namespace voxera
{
/*  Even-harmonic exciter.

    The Air shelf can only lift treble the singer actually produced; on a dull
    take there is nothing up there to raise. This generates it instead. The
    3.5-8 kHz band is squared, which places new energy at twice those
    frequencies, and the output high-pass keeps only that new material.

    Squaring rather than saturating is deliberate: it is exactly second order,
    so a band ending at 8 kHz produces nothing above 16 kHz. That stays under
    Nyquist even at 44.1 kHz, which is why this stage needs no oversampling and
    therefore adds no latency.

    Dividing by the band's own envelope makes the result first order in level, so
    the control behaves the same on a quiet verse and a loud chorus. That divisor
    is a peak follower with an instantaneous attack, which is what keeps the
    identity |generated| <= |source| true at every sample — including the first
    one after a reset, where an envelope starting from zero would otherwise
    divide by its floor and click. Between peaks the release is far slower than
    a 3.5 kHz period, so in steady state it behaves as a constant gain and the
    second-order bound above holds.
*/
class Exciter
{
public:
    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);

        split.prepare(sr, numChannels);
        split.setHighPass(splitHz, 0.707);
        band.prepare(sr, numChannels);
        band.setLowPass(bandTopHz, 0.707);
        lift.prepare(sr, numChannels);
        lift.setHighPass(liftHz, 0.707);

        releaseCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * releaseSeconds));
        broadbandCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * broadbandSeconds));
        envelope.fill(0.0f);
        broadband.fill(0.0f);

        amount.reset(sr, 0.030);
        amount.setCurrentAndTargetValue(0.0f);
    }

    void reset()
    {
        split.reset();
        band.reset();
        lift.reset();
        envelope.fill(0.0f);
        broadband.fill(0.0f);
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

            for (int ch = 0; ch < channels; ++ch)
            {
                auto& sample = buffer.getWritePointer(ch)[i];
                const float source = band.processSample(ch, split.processSample(ch, sample));

                auto& env = envelope[static_cast<size_t>(ch)];
                const float magnitude = std::abs(source);
                // Rises instantly, falls slowly: env >= magnitude always holds,
                // so the division below can never exceed |source|.
                env = juce::jmax(magnitude, env + releaseCoeff * (magnitude - env));

                // The floor only guards the divide; at that level source is
                // smaller still, so near-silence stays silent.
                const float generated = source * source / juce::jmax(env, envelopeFloor);

                /*  Held back on sibilants.

                    The band this stage squares is the band an "s" lives in, so
                    left alone it generates hardest exactly when it should be
                    generating least — and it sits downstream of the de-esser,
                    so nothing afterwards takes back what it added. Comparing
                    the band against the whole signal is enough to tell them
                    apart: a sung vowel puts most of its energy below this band
                    and a sibilant puts most of it inside.
                */
                /*  A peak follower, matching the band's, because comparing a
                    peak against a mean would inflate the ratio by however
                    crest-shaped the signal happens to be and read every loud
                    vowel as a sibilant.
                */
                auto& wide = broadband[static_cast<size_t>(ch)];
                const float wideMagnitude = std::abs(sample);
                wide = juce::jmax(wideMagnitude, wide + broadbandCoeff * (wideMagnitude - wide));
                const float sibilance = juce::jlimit(0.0f, 1.0f,
                    (env / juce::jmax(wide, envelopeFloor) - sibilanceOnset) / sibilanceRange);

                sample += wet * drive * (1.0f - sibilance) * lift.processSample(ch, generated);
            }
        }
    }

private:
    static constexpr double splitHz = 3500.0;    // bottom of the band that is squared
    static constexpr double bandTopHz = 8000.0;  // 2x this must stay under Nyquist
    static constexpr double liftHz = 7000.0;     // keeps only what the squaring created
    static constexpr double releaseSeconds = 0.030;
    static constexpr double broadbandSeconds = 0.020;
    static constexpr float envelopeFloor = 1.0e-6f;
    static constexpr float drive = 0.5f;
    /*  Where the band starts to dominate the whole signal, and over how much
        further it takes the generation all the way down.

        A sung vowel keeps most of its peak below 3.5 kHz, so the band reads
        around half the whole signal at most. A sibilant has almost nothing
        outside the band and reads near unity. Sixty per cent sits between the
        two with room on either side.
    */
    static constexpr float sibilanceOnset = 0.60f, sibilanceRange = 0.30f;

    Biquad split, band, lift;
    juce::SmoothedValue<float> amount;
    std::array<float, 2> envelope {}, broadband {};
    float releaseCoeff = 1.0f, broadbandCoeff = 1.0f;
    double sr = 48000.0;
    int numChannels = 2;
};
}

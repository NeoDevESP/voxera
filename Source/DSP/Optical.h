#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>

namespace voxera
{
/*  Optical-cell compressor, the slow half of a two-stage vocal chain.

    The fast compressor upstream catches individual peaks. This one does the
    opposite job: it works on the average over syllables and phrases, so the
    verse and the chorus arrive at the same place without either being audibly
    squeezed. Running one compressor hard enough to do both is what produces
    pumping; splitting the work across two time scales is why studio chains have
    always used two.

    What gives an optical stage its character is that a photoresistor does not
    recover exponentially. It lets go quickly at first and then far more slowly,
    and the longer it has been dark the slower that tail becomes. Modelling that
    is the whole point of this class: the release constant below is interpolated
    by how deep the current reduction is, so light touches recover promptly and
    heavy ones ease off over most of a second. That program dependence is what
    makes the stage hard to hear working.
*/
class Optical
{
public:
    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);

        detectorCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * detectorSeconds));
        attackCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * attackSeconds));
        fastRelease = 1.0f - std::exp(-1.0f / static_cast<float>(sr * fastReleaseSeconds));
        slowRelease = 1.0f - std::exp(-1.0f / static_cast<float>(sr * slowReleaseSeconds));

        amount.reset(sr, 0.050);
        amount.setCurrentAndTargetValue(0.0f);
        reset();
    }

    void reset()
    {
        detector = 0.0f;
        reduction = 0.0f;
        reductionDb.store(0.0f, std::memory_order_relaxed);
        amount.setCurrentAndTargetValue(amount.getTargetValue());
    }

    void setAmount(float normalised) { amount.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }

    float getReductionDb() const noexcept { return reductionDb.load(std::memory_order_relaxed); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        if (channels <= 0 || buffer.getNumSamples() == 0) return;

        float deepest = 0.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float wet = amount.getNextValue();

            float peak = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                peak = juce::jmax(peak, std::abs(buffer.getReadPointer(ch)[i]));

            detector += detectorCoeff * (peak - detector);
            const float levelDb = juce::Decibels::gainToDecibels(detector, -96.0f);

            // Turning the control up lowers the threshold rather than raising a
            // ratio: the stage keeps its gentle slope and simply meets the
            // signal sooner, which is how these units are actually driven.
            const float thresholdDb = -10.0f - 25.0f * wet;
            const float over = levelDb - thresholdDb;

            // Soft knee, so the onset of reduction is not itself an event.
            float target = 0.0f;
            if (over > kneeDb) target = over * slope;
            else if (over > -kneeDb) {
                const float t = (over + kneeDb) / (2.0f * kneeDb);
                target = slope * t * t * kneeDb;
            }

            if (target > reduction) {
                reduction += attackCoeff * (target - reduction);
            } else {
                // Deeper reduction releases more slowly: the optical behaviour.
                const float depth = juce::jlimit(0.0f, 1.0f, reduction / 9.0f);
                const float coeff = fastRelease + (slowRelease - fastRelease) * depth;
                reduction += coeff * (target - reduction);
            }

            // Make-up tracks the control so raising it does not drop the level.
            const float gain = juce::Decibels::decibelsToGain(-reduction + makeupDb * wet);
            for (int ch = 0; ch < channels; ++ch)
                buffer.getWritePointer(ch)[i] *= gain;

            deepest = juce::jmax(deepest, reduction);
        }

        reductionDb.store(deepest, std::memory_order_relaxed);
    }

private:
    static constexpr double detectorSeconds = 0.012;   // averages syllables, not peaks
    static constexpr double attackSeconds = 0.015;
    static constexpr double fastReleaseSeconds = 0.080;
    static constexpr double slowReleaseSeconds = 0.700;
    static constexpr float slope = 0.65f;              // about 3:1
    static constexpr float kneeDb = 6.0f;
    static constexpr float makeupDb = 5.0f;

    juce::SmoothedValue<float> amount;
    std::atomic<float> reductionDb { 0.0f };

    double sr = 48000.0;
    float detector = 0.0f, reduction = 0.0f;
    float detectorCoeff = 1.0f, attackCoeff = 1.0f, fastRelease = 1.0f, slowRelease = 1.0f;
    int numChannels = 2;
};
}

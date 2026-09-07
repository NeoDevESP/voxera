#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>

namespace voxera
{
/*  Upward compression.

    Every other dynamics stage in this chain works downwards: it finds what is
    too loud and holds it back. This one works from underneath, raising what is
    too quiet, and the result is a different thing entirely. Breaths, word tails
    and the texture inside a held note stop falling away under the track, so the
    voice reads as continuously present and close rather than as something that
    keeps receding between syllables.

    That is also why it cannot simply be more of the same. Downward compression
    applied hard enough to achieve this would flatten the peaks into a wall;
    lifting from below leaves the loud material exactly as it was.

    The hazard is obvious once stated: a stage whose job is to amplify quiet
    things will happily amplify room tone, preamp hiss and the silence between
    phrases. Two guards prevent it. The gain is capped, so no amount of quiet
    earns unlimited lift; and below `floorDb` the lift is withdrawn again along a
    taper, so genuine near-silence is left where it belongs. Placing the noise
    gate first in the chain means what reaches here is mostly voice already.

    The time constants are deliberately slow. Fast upward compression is audible
    as breathing — the noise floor rising and falling between words — which is
    the single most recognisable sign of it being overdone.
*/
class Upward
{
public:
    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);

        detectorCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * detectorSeconds));
        riseCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * riseSeconds));
        fallCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * fallSeconds));

        amount.reset(sr, 0.050);
        amount.setCurrentAndTargetValue(0.0f);
        reset();
    }

    void reset()
    {
        detector = 0.0f;
        lift = 0.0f;
        liftDb.store(0.0f, std::memory_order_relaxed);
        amount.setCurrentAndTargetValue(amount.getTargetValue());
    }

    void setAmount(float normalised) { amount.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }

    float getLiftDb() const noexcept { return liftDb.load(std::memory_order_relaxed); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        if (channels <= 0 || buffer.getNumSamples() == 0) return;

        float most = 0.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float wet = amount.getNextValue();

            float peak = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                peak = juce::jmax(peak, std::abs(buffer.getReadPointer(ch)[i]));

            detector += detectorCoeff * (peak - detector);
            const float levelDb = juce::Decibels::gainToDecibels(detector, -96.0f);

            // How far under the threshold the signal is, is how much it is owed.
            const float below = thresholdDb - levelDb;
            float target = below > 0.0f
                ? juce::jmin(below * slope, maxLiftDb) * wet
                : 0.0f;

            // Withdrawn again towards the floor, so hiss and the gaps between
            // phrases are not lifted along with the detail that sits above them.
            if (levelDb < floorDb)
                target *= juce::jlimit(0.0f, 1.0f, (levelDb - (floorDb - taperDb)) / taperDb);

            lift += (target > lift ? riseCoeff : fallCoeff) * (target - lift);

            const float gain = juce::Decibels::decibelsToGain(lift);
            for (int ch = 0; ch < channels; ++ch)
                buffer.getWritePointer(ch)[i] *= gain;

            most = juce::jmax(most, lift);
        }

        liftDb.store(most, std::memory_order_relaxed);
    }

private:
    static constexpr float thresholdDb = -28.0f;  // below this the lift begins
    static constexpr float floorDb = -52.0f;      // below this it is taken away again
    static constexpr float taperDb = 14.0f;       // over how many dB that happens
    static constexpr float slope = 0.55f;
    static constexpr float maxLiftDb = 12.0f;

    static constexpr double detectorSeconds = 0.030;
    static constexpr double riseSeconds = 0.120;   // slow, or it breathes audibly
    static constexpr double fallSeconds = 0.250;

    juce::SmoothedValue<float> amount;
    std::atomic<float> liftDb { 0.0f };

    double sr = 48000.0;
    float detector = 0.0f, lift = 0.0f;
    float detectorCoeff = 1.0f, riseCoeff = 1.0f, fallCoeff = 1.0f;
    int numChannels = 2;
};
}

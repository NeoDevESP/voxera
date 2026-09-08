#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

namespace voxera
{
/*  Compressor with selectable gain-element character.

    A textbook compressor measures the signal going in, works out how much to
    hold it back, and applies exactly that. It is correct and it sounds like
    arithmetic. The units people reach for instead are not doing that, and two
    departures account for most of the difference.

    The first is topology. In a feedback design the detector listens to the
    compressor's own output rather than its input, so the moment it starts
    reducing, the thing it is measuring gets quieter and it eases off. The
    reduction converges instead of being dictated, which is why a 1176 at 20:1
    does not sound twenty times harder than at 4:1 — the loop softens the ratio
    on its own, and by an amount that depends on the material. No amount of
    knee-shaping on a feedforward detector reproduces that, because the effect
    comes from the loop and not from the curve.

    The second is that the gain element is a physical part being asked to do
    work. A FET pinches, a valve's transconductance bends, an optocell lags and
    warms. All of them distort more the harder they are pushed, so the harmonic
    content rises and falls with the gain reduction itself. That coupling is
    what makes a compressor audible as a texture rather than only as a change in
    level, and it is why a clean compressor pushed hard just sounds squashed.
*/
class ColourCompressor
{
public:
    enum Type { clean = 0, fet, vca, variMu, numTypes };

    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);
        detectorCoeff = coefficient(0.002);
        reset();
    }

    void reset()
    {
        detector = 0.0f;
        reduction = 0.0f;
        holdDepth = 0.0f;
        feedbackState.fill(0.0f);
        reductionDb.store(0.0f, std::memory_order_relaxed);
    }

    void setType(int newType) noexcept { type = juce::jlimit(0, numTypes - 1, newType); }
    void setThresholdDb(float db) noexcept { thresholdDb = db; }
    void setRatio(float r) noexcept { ratio = juce::jmax(1.0f, r); }
    void setAttackMs(float ms) noexcept { attackMs = juce::jmax(0.02f, ms); }
    void setReleaseMs(float ms) noexcept { releaseMs = juce::jmax(5.0f, ms); }

    float getReductionDb() const noexcept { return reductionDb.load(std::memory_order_relaxed); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        if (channels <= 0 || buffer.getNumSamples() == 0) return;

        const auto& voice = character();
        // A FET's attack is measured in tens of microseconds; the control is
        // scaled into each element's own range rather than taken literally, or
        // every setting would sound the same whichever one is chosen.
        const float attack = coefficient(attackMs * 0.001 * voice.attackScale);
        const float fastRelease = coefficient(releaseMs * 0.001 * voice.releaseScale);
        const float slowRelease = coefficient(releaseMs * 0.001 * voice.releaseScale * voice.slowFactor);

        float deepest = 0.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            /*  Feedback designs detect what has already left the compressor, so
                the loop converges on a reduction instead of being handed one.
                One sample of delay is what the analogue circuit has too.
            */
            float key = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                key = juce::jmax(key, std::abs(voice.feedback
                                               ? feedbackState[static_cast<size_t>(ch)]
                                               : buffer.getReadPointer(ch)[i]));

            detector += detectorCoeff * (key - detector);
            const float levelDb = juce::Decibels::gainToDecibels(detector, -96.0f);
            const float over = levelDb - thresholdDb;

            /*  Above the knee the ratio itself rises with level on the valve and
                FET settings. A tube's transconductance falls away as it is
                driven, so it compresses harder the louder the passage gets, and
                that curve is a large part of why those units flatter a vocal
                instead of merely holding it down.
            */
            const float effectiveRatio = ratio + voice.ratioBend * juce::jmax(0.0f, over);
            const float slope = 1.0f - 1.0f / juce::jmax(1.0f, effectiveRatio);

            float target = 0.0f;
            if (over > voice.kneeDb) target = over * slope;
            else if (over > -voice.kneeDb) {
                const float t = (over + voice.kneeDb) / (2.0f * voice.kneeDb);
                target = slope * t * t * voice.kneeDb;
            }

            if (target > reduction) {
                reduction += attack * (target - reduction);
            } else {
                /*  Release that remembers. Programme dependence in these units
                    is not a second time constant chosen at random: the element
                    recovers slowly in proportion to how hard and how long it has
                    been held, which is what stops a sustained phrase pumping
                    while a single loud word still lets go quickly.
                */
                holdDepth += 0.0004f * (reduction - holdDepth);
                const float depth = juce::jlimit(0.0f, 1.0f, holdDepth / 8.0f);
                const float coeff = fastRelease + (slowRelease - fastRelease) * depth * voice.programDependence;
                reduction += coeff * (target - reduction);
            }

            const float gain = juce::Decibels::decibelsToGain(-reduction + voice.makeupPerDb * reduction);

            for (int ch = 0; ch < channels; ++ch)
            {
                auto& sample = buffer.getWritePointer(ch)[i];
                float y = sample * gain;

                /*  The gain element distorting in proportion to its own work.
                    Scaled by the reduction, so a passage that is not being
                    compressed is not being coloured either — which is the whole
                    reason this reads as texture rather than as a tone control.
                */
                if (voice.drive > 0.0f && reduction > 0.05f) {
                    const float amount = voice.drive * juce::jlimit(0.0f, 1.0f, reduction / 12.0f);
                    const float bent = std::tanh(y * (1.0f + amount * 2.0f));
                    // Asymmetry gives the even harmonics a valve has and a FET
                    // mostly does not, which is the audible difference between
                    // the two once they are both working hard.
                    const float even = voice.evenness * amount * (bent * bent - 0.5f);
                    y += amount * (bent + even - y);
                }

                feedbackState[static_cast<size_t>(ch)] = y;
                sample = std::isfinite(y) ? y : 0.0f;
            }

            deepest = juce::jmax(deepest, reduction);
        }

        reductionDb.store(deepest, std::memory_order_relaxed);
    }

private:
    struct Voice
    {
        bool feedback;
        float attackScale, releaseScale, slowFactor, programDependence;
        float kneeDb, ratioBend, drive, evenness, makeupPerDb;
    };

    const Voice& character() const
    {
        /*  Each row is one gain element, described by how it differs from the
            arithmetic one rather than by trying to be a circuit model.
        */
        static const std::array<Voice, numTypes> voices { {
            // feedback attack release  slow  prog  knee  bend  drive  even  makeup
            {  false,    1.0f,   1.0f,  1.0f, 0.0f, 2.0f, 0.00f, 0.00f, 0.0f, 0.00f },  // Clean: the textbook
            {  true,     0.08f,  0.5f,  6.0f, 0.8f, 1.0f, 0.25f, 0.35f, 0.2f, 0.25f },  // FET: fast, hard, gritty
            {  false,    1.0f,   1.0f,  4.0f, 0.6f, 3.0f, 0.05f, 0.10f, 0.3f, 0.15f },  // VCA: controlled, auto-release
            {  true,     2.5f,   2.0f,  8.0f, 1.0f, 6.0f, 0.40f, 0.30f, 0.8f, 0.35f }   // Vari-Mu: slow, valve, even
        } };
        return voices[static_cast<size_t>(type)];
    }

    float coefficient(double seconds) const
    {
        return 1.0f - std::exp(-1.0f / static_cast<float>(sr * juce::jmax(1.0e-5, seconds)));
    }

    std::atomic<float> reductionDb { 0.0f };
    std::array<float, 2> feedbackState {};

    double sr = 48000.0;
    float detector = 0.0f, reduction = 0.0f, holdDepth = 0.0f, detectorCoeff = 1.0f;
    float thresholdDb = -18.0f, ratio = 3.0f, attackMs = 8.0f, releaseMs = 90.0f;
    int numChannels = 2, type = clean;
};
}

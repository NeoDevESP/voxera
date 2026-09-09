#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

namespace voxera
{
// Behavioural gain-element flavours, inspired by documented hardware timing.
// These are not measured circuit emulations; see Docs/SOUND_ENGINEERING.md.
class ColourCompressor
{
public:
    enum Type { clean = 0, fet, vca, variMu, opto, numTypes };

    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);
        updateSidechain();
        reset();
    }

    void reset()
    {
        detector = 0.0f;
        reduction = 0.0f;
        hasProcessed = false;
        holdDepth = 0.0f;
        feedbackState.fill(0.0f);
        for (auto& channel : sidechainLow) channel.fill(0.0f);
        dcInput.fill(0.0f); dcOutput.fill(0.0f);
        mixSmooth.reset(sr, 0.02); mixSmooth.setCurrentAndTargetValue(mix);
        colourSmooth.reset(sr, 0.02); colourSmooth.setCurrentAndTargetValue(colour);
        reductionDb.store(0.0f, std::memory_order_relaxed);
    }

    void setColour(float value) noexcept { colour = juce::jlimit(0.0f, 1.0f, value); }
    void setType(int newType) noexcept { type = juce::jlimit(0, numTypes - 1, newType); }
    void setThresholdDb(float db) noexcept { thresholdDb = db; }
    void setRatio(float r) noexcept { ratio = juce::jmax(1.0f, r); }
    void setAttackMs(float ms) noexcept { attackMs = juce::jmax(0.02f, ms); }
    void setReleaseMs(float ms) noexcept { releaseMs = juce::jmax(5.0f, ms); }

    // Two-pole sidechain high-pass prevents low plosives dominating the detector.
    void setSidechainHz(float hz) noexcept
    {
        sidechainHz = juce::jlimit(20.0f, 400.0f, std::isfinite(hz) ? hz : 85.0f);
        updateSidechain();
    }

    /*  How much of the compressed signal reaches the output.

        Below one this is parallel compression, and it is the reason people
        drive these things far harder than the level alone would justify: the
        quiet detail comes up, the loud parts keep the transient the dry signal
        still has, and the distortion of a gain element working hard arrives as
        colour underneath rather than as the whole sound. Squashing flat and
        blending it under is a different result from compressing gently, even
        where both land on the same amount of gain reduction.
    */
    void setMix(float normalised) noexcept { mix = juce::jlimit(0.0f, 1.0f, normalised); }

    float getReductionDb() const noexcept { return reductionDb.load(std::memory_order_relaxed); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        if (channels <= 0 || buffer.getNumSamples() == 0) return;

        const auto& voice = character();
        // A FET's attack is measured in tens of microseconds; the control is
        // scaled into each element's own range rather than taken literally, or
        // every setting would sound the same whichever one is chosen.
        const double attackSeconds = type == fet ? juce::jlimit(0.00002, 0.0008, attackMs * 0.001 * voice.attackScale)
            : type == opto ? 0.010 : attackMs * 0.001 * voice.attackScale;
        const float attack = coefficient(attackSeconds);
        const double releaseSeconds = type == fet ? juce::jlimit(0.050, 1.1, releaseMs * 0.001 * voice.releaseScale)
            : type == opto ? 0.060 : releaseMs * 0.001 * voice.releaseScale;
        const float fastRelease = coefficient(releaseSeconds);
        const float slowRelease = coefficient(type == opto ? 3.0 : releaseSeconds * voice.slowFactor);

        const float detectorSpeed = coefficient(0.002 * voice.detectorScale);
        const float memorySpeed = coefficient(0.08);
        const float dcPole = std::exp(-2.0f * juce::MathConstants<float>::pi * 8.0f / static_cast<float>(sr));
        mixSmooth.setTargetValue(mix);
        colourSmooth.setTargetValue(colour);
        if (!hasProcessed) {
            mixSmooth.setCurrentAndTargetValue(mix);
            colourSmooth.setCurrentAndTargetValue(colour);
            hasProcessed = true;
        }
        float deepest = 0.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            /*  Feedback designs detect what has already left the compressor, so
                the loop converges on a reduction instead of being handed one.
                The digital loop uses one sample of delay.
            */
            float key = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                const auto c = static_cast<size_t>(ch);
                const float input = voice.feedback ? feedbackState[c] : buffer.getReadPointer(ch)[i];
                auto& low = sidechainLow[c];
                low[0] += sidechainCoeff * (input - low[0]);
                const float high = input - low[0];
                low[1] += sidechainCoeff * (high - low[1]);
                key = juce::jmax(key, std::abs(high - low[1]));
            }
            detector += detectorSpeed * (key - detector);
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

            holdDepth += memorySpeed * (reduction - holdDepth);
            if (target > reduction) {
                reduction += attack * (target - reduction);
            } else {
                /*  Release that remembers. Programme dependence in these units
                    is not a second time constant chosen at random: the element
                    recovers slowly in proportion to how hard and how long it has
                    been held, which is what stops a sustained phrase pumping
                    while a single loud word still lets go quickly.
                */
                const float depth = juce::jlimit(0.0f, 1.0f, holdDepth / 8.0f);
                const float coeff = fastRelease + (slowRelease - fastRelease) * depth * voice.programDependence;
                reduction += coeff * (target - reduction);
            }

            const float gain = juce::Decibels::decibelsToGain(-reduction + voice.makeupPerDb * reduction);

            const float wet = mixSmooth.getNextValue();
            const float colourAmount = colourSmooth.getNextValue();
            for (int ch = 0; ch < channels; ++ch)
            {
                auto& sample = buffer.getWritePointer(ch)[i];
                const float dry = sample;
                float y = sample * gain;

                /*  The gain element distorting in proportion to its own work.
                    Scaled by the reduction, so a passage that is not being
                    compressed is not being coloured either — which is the whole
                    reason this reads as texture rather than as a tone control.
                */
                if (voice.drive > 0.0f && reduction > 0.05f) {
                    const float amount = 2.0f * colourAmount * voice.drive * juce::jlimit(0.0f, 1.0f, reduction / 12.0f);
                    const float bent = std::tanh(y * (1.0f + amount * 2.0f));
                    // Asymmetry gives the even harmonics a valve has and a FET
                    // mostly does not, which is the audible difference between
                    // the two once they are both working hard.
                    const float even = voice.evenness * amount * (bent * bent);
                    y += amount * (bent + even - y);
                }

                /*  The loop keeps listening to the fully compressed signal, not
                    to the blend. A feedback detector fed its own diluted output
                    would measure a level that was never compressed that hard
                    and ease off accordingly, so turning the blend down would
                    quietly reduce the compression as well as its share of the
                    output — two controls in one knob, and neither doing what it
                    says.
                */
                // Remove rectification DC before feedback and before the parallel blend.
                const auto c = static_cast<size_t>(ch);
                if (voice.drive > 0.0f) {
                    const float filtered = y - dcInput[c] + dcPole * dcOutput[c];
                    dcInput[c] = y; dcOutput[c] = filtered; y = filtered;
                }
                feedbackState[c] = y;

                const float blended = dry + wet * (y - dry);
                sample = std::isfinite(blended) ? blended : 0.0f;
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
        // How long the detector averages over, relative to two milliseconds.
        float detectorScale;
    };

    const Voice& character() const
    {
        /*  Each row is one gain element, described by how it differs from the
            arithmetic one rather than by trying to be a circuit model.
        */
        static const std::array<Voice, numTypes> voices { {
            // feedback attack release  slow  prog  knee  bend  drive  even  makeup  detect
            {  false,    1.0f,   1.0f,  1.0f, 0.0f, 2.0f, 0.00f, 0.00f, 0.0f, 0.00f,  1.0f },  // Clean: the textbook
            {  true,     0.08f,  0.5f,  6.0f, 0.8f, 1.0f, 0.25f, 0.35f, 0.2f, 0.25f,  0.3f },  // FET: fast, hard, gritty
            {  false,    1.0f,   1.0f,  4.0f, 0.6f, 3.0f, 0.05f, 0.10f, 0.3f, 0.15f,  1.5f },  // VCA: controlled, auto-release
            {  true,     2.5f,   2.0f,  8.0f, 1.0f, 6.0f, 0.40f, 0.30f, 0.8f, 0.35f,  6.0f },  // Vari-Mu: slow, valve, even
            /*  Opto: a photocell, which is why its own timings are fixed below
                rather than taken from the attack and release controls.

                Labelled Vari-Mu when it arrived, one row below the entry that
                actually is. A wrong label on a table read by index is worse
                than none: everything still works, and the next person to tune
                the valve setting edits the photocell instead.
            */
            {  true,     1.0f,   1.0f,  8.0f, 1.0f, 5.0f, 0.10f, 0.20f, 0.7f, 0.25f,  2.0f }
        } };
        return voices[static_cast<size_t>(type)];
    }

    float coefficient(double seconds) const
    {
        return 1.0f - std::exp(-1.0f / static_cast<float>(sr * juce::jmax(1.0e-5, seconds)));
    }

    void updateSidechain() noexcept
    {
        sidechainCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                                         * sidechainHz / static_cast<float>(sr));
    }

    std::atomic<float> reductionDb { 0.0f };
    std::array<float, 2> feedbackState {};
    juce::SmoothedValue<float> mixSmooth, colourSmooth;
    std::array<float, 2> dcInput {}, dcOutput {};
    std::array<std::array<float, 2>, 2> sidechainLow {};   // the two poles of the detector filter

    double sr = 48000.0;
    float colour = 0.5f;
    bool hasProcessed = false;
    float detector = 0.0f, reduction = 0.0f, holdDepth = 0.0f;
    float thresholdDb = -18.0f, ratio = 3.0f, attackMs = 8.0f, releaseMs = 90.0f;
    float sidechainHz = 85.0f, sidechainCoeff = 0.0f, mix = 1.0f;
    int numChannels = 2, type = clean;
};
}

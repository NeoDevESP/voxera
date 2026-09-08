#pragma once
#include <JuceHeader.h>
#include "RealtimeUtilities.h"
#include "Biquad.h"
#include <array>
#include <cmath>
#include <memory>

class Saturator
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        capacity = juce::jmax(1, static_cast<int>(spec.maximumBlockSize));
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
            static_cast<size_t>(spec.numChannels), 2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampling->initProcessing(static_cast<size_t>(capacity));
        latency = static_cast<int>(std::lround(oversampling->getLatencyInSamples()));
        dry.setSize(static_cast<int>(spec.numChannels), capacity);
        dryDelay.prepare(static_cast<int>(spec.numChannels), latency);
        driveDb.reset(spec.sampleRate * 4.0, 0.030);
        warmth.reset(spec.sampleRate * 4.0, 0.030);
        mix.reset(spec.sampleRate, 0.030);
        driveDb.setCurrentAndTargetValue(0.0f);
        warmth.setCurrentAndTargetValue(0.0f);
        mix.setCurrentAndTargetValue(0.0f);

        // Around 12 Hz, well under anything sung.
        dcCoeff = 1.0f - juce::MathConstants<float>::twoPi * 12.0f / static_cast<float>(spec.sampleRate);
        dcX1.fill(0.0f);
        dcY1.fill(0.0f);

        /*  Emphasis around the shaper, and its exact inverse after it.

            Applying one curve to the whole spectrum is the thing no analogue
            circuit does, and it is what makes digital saturation sound like
            distortion rather than like gear. Driven flat, a low fundamental
            throws its harmonics into the midrange as mud, and sibilance is
            shaped as hard as everything else and turns harsh.

            Transformers and valves colour the bottom far more than the top. So
            the low end is lifted into the curve and the top held out of it, and
            the opposite shelves afterwards put the balance back. Because the
            two are exact inverses of each other, a signal too quiet to reach
            the curve passes through completely unchanged — the tilt costs
            nothing until the stage is actually working.
        */
        preLow.prepare(spec.sampleRate, static_cast<int>(spec.numChannels));
        preHigh.prepare(spec.sampleRate, static_cast<int>(spec.numChannels));
        postLow.prepare(spec.sampleRate, static_cast<int>(spec.numChannels));
        postHigh.prepare(spec.sampleRate, static_cast<int>(spec.numChannels));

        preLow.setLowShelf(lowHz, tiltDb);
        preHigh.setHighShelf(highHz, -tiltDb);
        postLow.setLowShelf(lowHz, -tiltDb);
        postHigh.setHighShelf(highHz, tiltDb);
    }

    void reset()
    {
        if (oversampling) oversampling->reset();
        dryDelay.reset();
        dry.clear();
        dcX1.fill(0.0f);
        dcY1.fill(0.0f);
    }
    void setDriveDb(float db) { driveDb.setTargetValue(db); }

    /*  Bias into the shaper, which is what makes it asymmetric.

        tanh is an odd function, so however hard it is driven it can only ever
        produce odd harmonics — the edge of a distorting amplifier, never the
        warmth of one. Offsetting the input so the curve is steeper on one side
        than the other breaks that symmetry, and even harmonics appear. That is
        the difference between a stage that sounds aggressive and one that
        sounds like tubes.
    */
    void setWarmth(float normalised) { warmth.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }
    void setMix(float wet) { mix.setTargetValue(juce::jlimit(0.0f, 1.0f, wet)); }
    float getLatencySamples() const noexcept { return static_cast<float>(latency); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (!oversampling) return;
        juce::dsp::AudioBlock<float> whole(buffer);
        // Hosts may exceed their advertised maximum block size.
        for (int offset = 0; offset < buffer.getNumSamples(); offset += capacity)
        {
            const int count = juce::jmin(capacity, buffer.getNumSamples() - offset);
            auto block = whole.getSubBlock(static_cast<size_t>(offset), static_cast<size_t>(count));
            for (int i = 0; i < count; ++i)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    dry.getWritePointer(ch)[i] = dryDelay.process(ch, block.getChannelPointer(static_cast<size_t>(ch))[i]);
                dryDelay.advance();
            }
            // Into the curve tilted, so the bottom is what gets coloured.
            for (int i = 0; i < count; ++i)
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                    auto& value = block.getChannelPointer(static_cast<size_t>(ch))[i];
                    value = preHigh.processSample(ch, preLow.processSample(ch, value));
                }

            auto up = oversampling->processSamplesUp(block);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                const float drive = juce::Decibels::decibelsToGain(driveDb.getNextValue());
                const float bias = maxBias * warmth.getNextValue();
                const float offset = std::tanh(bias);
                // Normalising by what full scale maps to keeps the stage at
                // unity up top whatever the bias has done to the curve, so the
                // control changes the harmonics and not the level.
                const float norm = std::tanh(drive + bias) - offset;

                for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
                {
                    auto& value = up.getChannelPointer(ch)[i];
                    value = (std::tanh(value * drive + bias) - offset) / norm;
                }
            }
            oversampling->processSamplesDown(block);

            /*  Asymmetry is the point, and asymmetry rectifies: the output now
                carries a DC component that follows the signal's own level. Left
                alone it would offset the whole chain downstream and eat headroom
                at the limiter. A valve stage loses it to a coupling capacitor;
                this is that capacitor.
            */
            for (int i = 0; i < count; ++i)
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    auto& value = block.getChannelPointer(static_cast<size_t>(ch))[i];
                    auto& x1 = dcX1[static_cast<size_t>(ch)];
                    auto& y1 = dcY1[static_cast<size_t>(ch)];
                    const float y = value - x1 + dcCoeff * y1;
                    x1 = value; y1 = y;
                    value = y;
                }

            // And back out through the inverse, restoring the balance while
            // leaving the harmonics where the tilt put them.
            for (int i = 0; i < count; ++i)
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                    auto& value = block.getChannelPointer(static_cast<size_t>(ch))[i];
                    value = postHigh.processSample(ch, postLow.processSample(ch, value));
                }

            for (int i = 0; i < count; ++i)
            {
                const float wet = mix.getNextValue();
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    auto& value = block.getChannelPointer(static_cast<size_t>(ch))[i];
                    value = wet * value + (1.0f - wet) * dry.getReadPointer(ch)[i];
                }
            }
        }
    }
private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> dry;
    voxera::IntegerDelay dryDelay;
    // Beyond this the curve is so lopsided it reads as a fault rather than as
    // warmth, and the DC blocker starts having to work hard.
    static constexpr float maxBias = 0.65f;

    // Nine decibels of tilt: enough that the bottom is clearly what is being
    // driven, gentle enough that the inverse afterwards does not have to undo
    // anything the ear would notice.
    static constexpr double lowHz = 320.0, highHz = 3800.0, tiltDb = 9.0;

    Biquad preLow, preHigh, postLow, postHigh;

    juce::SmoothedValue<float> driveDb, warmth, mix;
    std::array<float, 2> dcX1 {}, dcY1 {};
    float dcCoeff = 0.999f;
    int capacity = 1, latency = 0;
};

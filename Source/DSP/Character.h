#pragma once
#include <JuceHeader.h>
#include "Biquad.h"
#include <cmath>

namespace voxera
{
/*  Voice character: formant offset plus a spectral tilt.

    These are timbres, not impersonations. Each one is a shift of the vocal
    tract's resonances together with a shelf pair — the same two things that
    separate a large singer from a small one, or a voice heard down a phone line
    from one in the room. None of them is derived from, trained on, or aimed at
    any particular person's voice, and none of them can produce one.

    The formant offset is handed back rather than applied here, because the pitch
    engine's shifter is what already knows how to move formants without moving
    pitch. Duplicating that with filters would fight it.

    Coefficients are recomputed once per block from smoothed gains instead of per
    sample: recalculating a shelf every sample is wasteful, and stepping straight
    to a new set on a character change would click.
*/
class Character
{
public:
    enum Type { neutral = 0, bright, dark, ghost, robot, demon, numTypes };

    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);

        low.prepare(sr, numChannels);
        high.prepare(sr, numChannels);

        for (auto* value : { &lowDb, &highDb, &formant }) {
            value->reset(sr, 0.080);
            value->setCurrentAndTargetValue(0.0f);
        }
        applyCoefficients(0.0f, 0.0f);
    }

    void reset()
    {
        low.reset();
        high.reset();
    }

    void setType(int type)
    {
        const auto& voice = table()[static_cast<size_t>(juce::jlimit(0, numTypes - 1, type))];
        lowDb.setTargetValue(voice.lowDb);
        highDb.setTargetValue(voice.highDb);
        formant.setTargetValue(voice.formantSemitones);
    }

    // Added to the user's own Formant setting by the caller.
    float formantOffsetSemitones() const noexcept { return formant.getCurrentValue(); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        const int count = buffer.getNumSamples();
        if (channels <= 0 || count == 0) return;

        const float lowTarget = lowDb.skip(count);
        const float highTarget = highDb.skip(count);
        formant.skip(count);

        applyCoefficients(lowTarget, highTarget);

        // Below a fortieth of a dB the shelves are identities; skipping them
        // keeps the neutral setting bit-exact rather than merely close.
        if (std::abs(lowTarget) < 0.025f && std::abs(highTarget) < 0.025f) return;

        for (int i = 0; i < count; ++i)
            for (int ch = 0; ch < channels; ++ch) {
                auto& sample = buffer.getWritePointer(ch)[i];
                sample = high.processSample(ch, low.processSample(ch, sample));
            }
    }

private:
    struct Voice { float formantSemitones, lowDb, highDb; };

    static const std::array<Voice, numTypes>& table()
    {
        //                          formant   low     high
        static const std::array<Voice, numTypes> voices { {
            {  0.0f,   0.0f,   0.0f },   // neutral
            {  2.0f,  -2.0f,   4.0f },   // bright: smaller tract, lifted top
            { -3.0f,   2.5f,  -4.5f },   // dark:   larger tract, closed top
            {  5.0f,  -6.0f,   3.0f },   // ghost:  thin and airy, no weight
            {  1.0f, -12.0f, -10.0f },   // robot:  narrow band, telephone-like
            { -9.0f,   5.0f,  -6.0f }    // demon:  far larger tract, heavy
        } };
        return voices;
    }

    void applyCoefficients(float lowGainDb, float highGainDb)
    {
        low.setLowShelf(lowShelfHz, lowGainDb);
        high.setHighShelf(highShelfHz, highGainDb);
    }

    static constexpr double lowShelfHz = 260.0;
    static constexpr double highShelfHz = 3600.0;

    Biquad low, high;
    juce::SmoothedValue<float> lowDb, highDb, formant;
    double sr = 48000.0;
    int numChannels = 2;
};
}

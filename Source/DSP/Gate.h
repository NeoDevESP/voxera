#pragma once
#include <JuceHeader.h>
#include "RealtimeUtilities.h"
#include <array>
#include <atomic>
#include <cmath>

namespace voxera
{
/*  Look-ahead noise gate for room tone, hiss and headphone bleed.

    Two decisions keep this from doing the damage gates usually do to vocals.

    The first is look-ahead: the audio is delayed while the level is measured on
    the undelayed signal, so the gate has already opened by the time a consonant
    arrives. A gate without it always removes the first few milliseconds of every
    word, which is precisely the part that carries the consonant.

    The second is the hold. Final consonants — s, t, k, f — are quiet, and they
    land after the vowel that opened the gate. Closing on level alone truncates
    them and leaves the take sounding clipped. So once open, the gate stays open
    for `holdSeconds` after the level falls, and only then releases slowly. The
    separate, lower close threshold stops it chattering on a decaying note.
*/
class Gate
{
public:
    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);

        lookaheadSamples = juce::jmax(1, static_cast<int>(std::lround(sr * lookaheadSeconds)));
        delay.prepare(numChannels, lookaheadSamples);

        holdSamples = static_cast<int>(std::lround(sr * holdSeconds));
        detectorCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * detectorSeconds));
        attackCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * attackSeconds));
        releaseCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * releaseSeconds));

        reset();
    }

    void reset()
    {
        delay.reset();
        detector = 0.0f;
        // Starts open: a gate that has not heard anything yet must not swallow
        // the first word while it works out where the noise floor is.
        gain = 1.0f;
        holdCounter = holdSamples;
        reductionDb.store(0.0f, std::memory_order_relaxed);
    }

    void setEnabled(bool shouldGate) noexcept { enabled = shouldGate; }

    void setThresholdDb(float db) noexcept
    {
        openLevel = juce::Decibels::decibelsToGain(db);
        closeLevel = juce::Decibels::decibelsToGain(db - hysteresisDb);
    }

    // Constant: the delay runs whether or not the gate is enabled.
    int getLatencySamples() const noexcept { return lookaheadSamples; }
    float getReductionDb() const noexcept { return reductionDb.load(std::memory_order_relaxed); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        if (channels <= 0 || buffer.getNumSamples() == 0) return;

        float worst = 1.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                peak = juce::jmax(peak, std::abs(buffer.getReadPointer(ch)[i]));

            detector += detectorCoeff * (peak - detector);

            if (detector > openLevel) holdCounter = holdSamples;
            else if (detector < closeLevel && holdCounter > 0) --holdCounter;

            const float target = (!enabled || holdCounter > 0) ? 1.0f : 0.0f;
            gain += (target > gain ? attackCoeff : releaseCoeff) * (target - gain);

            for (int ch = 0; ch < channels; ++ch)
            {
                auto& sample = buffer.getWritePointer(ch)[i];
                sample = delay.process(ch, sample) * gain;
            }

            delay.advance();
            worst = juce::jmin(worst, gain);
        }

        /*  Stored as a positive number of decibels of reduction, which is
            what the name says and what every other stage here reports.

            It used to store the gain change instead, so a reduction was
            negative while the compressor and the optical stage next to it made
            it positive — two conventions behind one accessor name. The editor
            worked around that with a minus sign at one call site out of four,
            which meant the inconsistency was invisible until something else
            read the same values and got them backwards.
        */
        reductionDb.store(-juce::Decibels::gainToDecibels(worst, -96.0f), std::memory_order_relaxed);
    }

private:
    static constexpr double lookaheadSeconds = 0.003;
    static constexpr double holdSeconds = 0.180;   // covers a trailing consonant
    static constexpr double detectorSeconds = 0.004;
    static constexpr double attackSeconds = 0.001;
    static constexpr double releaseSeconds = 0.120;
    static constexpr float hysteresisDb = 8.0f;

    IntegerDelay delay;
    std::atomic<float> reductionDb { 0.0f };

    double sr = 48000.0;
    float detector = 0.0f, gain = 1.0f;
    float openLevel = 0.0f, closeLevel = 0.0f;
    float detectorCoeff = 1.0f, attackCoeff = 1.0f, releaseCoeff = 1.0f;
    int lookaheadSamples = 1, holdSamples = 0, holdCounter = 0, numChannels = 1;
    bool enabled = false;
};
}

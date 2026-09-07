#pragma once
#include <JuceHeader.h>
#include "FractionalDelay.h"
#include "DuckingDelay.h"
#include "FDNReverb.h"
#include "Biquad.h"
#include <array>
#include <cmath>

class SpatialEngine
{
public:
    void prepare(double sampleRate, int channels)
    {
        sr = sampleRate;
        bypassBlend.reset(sr, 0.020);
        bypassBlend.setCurrentAndTargetValue(enabled ? 1.0f : 0.0f);
        numChannels = juce::jlimit(1, 2, channels);

        doubler.prepare(sr, 0.080);
        for (size_t v = 0; v < numDoubleVoices; ++v) {
            doubleVoices[v].increment =
                juce::MathConstants<double>::twoPi * doubleVoices[v].rateHz / sr;
            // Spread the starting phases too, so the voices are already apart
            // at the first sample instead of converging out of a shared start.
            doublePhase[v] = juce::MathConstants<double>::twoPi * static_cast<double>(v) / numDoubleVoices;
        }

        delay.prepare(sr);
        reverb.prepare(sr);

        doubleHP.prepare(sr, 2);
        doubleLP.prepare(sr, 2);
        doubleHP.setHighPass(170.0, 0.707);
        doubleLP.setLowPass(12000.0, 0.707);

        reverbHP.prepare(sr, 2);
        reverbAir.prepare(sr, 2);
        applyReverbTone();

        width.reset(sr, 0.050);
        doubleAmount.reset(sr, 0.050);
        delayAmount.reset(sr, 0.050);
        spaceAmount.reset(sr, 0.080);
        duckAmount.reset(sr, 0.050);

        width.setCurrentAndTargetValue(0.65f);
        doubleAmount.setCurrentAndTargetValue(0.22f);
        delayAmount.setCurrentAndTargetValue(0.12f);
        spaceAmount.setCurrentAndTargetValue(0.18f);
        duckAmount.setCurrentAndTargetValue(0.65f);

        // Envelope: fast attack, relaxed release.
        envAttack = std::exp(-1.0f / static_cast<float>(0.005 * sr));
        envRelease = std::exp(-1.0f / static_cast<float>(0.140 * sr));

        envelope = 0.0f;

        reset();
    }

    void reset()
    {
        bypassBlend.setCurrentAndTargetValue(enabled ? 1.0f : 0.0f);
        doubler.reset();
        for (size_t v = 0; v < numDoubleVoices; ++v)
            doublePhase[v] = juce::MathConstants<double>::twoPi * static_cast<double>(v) / numDoubleVoices;
        delay.reset();
        reverb.reset();
        doubleHP.reset();
        doubleLP.reset();
        envelope = 0.0f;
    }

    void setEnabled(bool v) noexcept { enabled = v; }
    void setTempo(double bpm) noexcept { tempo = juce::jlimit(40.0, 240.0, bpm); }
    void setDivision(int index) noexcept { division = juce::jlimit(0, 4, index); }

    void setWidth(float v01) noexcept
    {
        width.setTargetValue(juce::jlimit(0.0f, 1.0f, v01));
    }

    void setDouble(float v01) noexcept
    {
        doubleAmount.setTargetValue(juce::jlimit(0.0f, 1.0f, v01));
    }

    void setDelay(float v01) noexcept
    {
        delayAmount.setTargetValue(juce::jlimit(0.0f, 1.0f, v01));
    }

    void setDelayFeedback(float v01) noexcept
    {
        delay.setFeedback(juce::jlimit(0.0f, 1.0f, v01));
    }

    void setSpace(float v01) noexcept
    {
        const float s = juce::jlimit(0.0f, 1.0f, v01);
        spaceAmount.setTargetValue(s);
        reverb.setSpace(s);
    }

    /*  Tone controls for the reverb tail alone, not for the dry voice.

        A full-range reverb under a vocal is what makes a mix sound smeared: the
        tail's low end sits exactly where the singer's fundamental and the bass
        already are, and none of it is doing anything a listener can locate. Low
        Body cuts further into it, so the space is audible without the mix
        thickening. Air lifts the top of the tail instead, which reads as size
        rather than as mud.
    */
    void setReverbBody(float v01) noexcept
    {
        const float clamped = juce::jlimit(0.0f, 1.0f, v01);
        if (std::abs(clamped - body) < 1.0e-4f) return;
        body = clamped;
        applyReverbTone();
    }

    void setReverbAirDb(float db) noexcept
    {
        const float clamped = juce::jlimit(-8.0f, 8.0f, db);
        if (std::abs(clamped - airDb) < 1.0e-3f) return;
        airDb = clamped;
        applyReverbTone();
    }

    void setDuck(float v01) noexcept
    {
        duckAmount.setTargetValue(juce::jlimit(0.0f, 1.0f, v01));
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumSamples() == 0)
            return;

        bypassBlend.setTargetValue(enabled ? 1.0f : 0.0f);
        delay.setTempoAndDivision(tempo, division);

        const int channels = buffer.getNumChannels();
        auto* leftOut = buffer.getWritePointer(0);
        auto* rightOut = channels > 1 ? buffer.getWritePointer(1) : nullptr;


        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float dryL = leftOut[i];
            const float dryR = rightOut != nullptr ? rightOut[i] : dryL;
            const float mono = 0.5f * (dryL + dryR);

            // ---------------- Voice activity / ducking ----------------
            const float detector = juce::jmax(std::abs(dryL), std::abs(dryR));
            const float coeff = detector > envelope ? envAttack : envRelease;
            envelope = coeff * envelope + (1.0f - coeff) * detector;

            const float envDb = juce::Decibels::gainToDecibels(envelope, -100.0f);
            const float activity = juce::jlimit(0.0f, 1.0f, (envDb + 44.0f) / 26.0f);
            const float duckDb = -18.0f * duckAmount.getNextValue() * activity;
            const float wetDuck = juce::Decibels::decibelsToGain(duckDb);

            // ---------------- Modulated doubler ----------------
            // One line, read at every voice's tap: they are all copies of the
            // same mono signal, so a second buffer would hold identical data.
            doubler.push(mono);

            float dblL = 0.0f, dblR = 0.0f;
            for (size_t v = 0; v < numDoubleVoices; ++v)
            {
                const auto& voice = doubleVoices[v];
                auto& phase = doublePhase[v];

                const float ms = voice.baseMs + voice.depthMs * static_cast<float>(std::sin(phase));
                const float tap = doubler.read(doubler.msToSamples(ms));

                // Equal-power pan from a position in [-1, 1].
                const float angle = 0.25f * juce::MathConstants<float>::pi * (voice.pan + 1.0f);
                dblL += tap * std::cos(angle);
                dblR += tap * std::sin(angle);

                phase += voice.increment;
                if (phase >= juce::MathConstants<double>::twoPi)
                    phase -= juce::MathConstants<double>::twoPi;
            }

            dblL *= doubleNormalise;
            dblR *= doubleNormalise;

            dblL = doubleHP.processSample(0, dblL);
            dblR = doubleHP.processSample(1, dblR);
            dblL = doubleLP.processSample(0, dblL);
            dblR = doubleLP.processSample(1, dblR);

            // ---------------- Tempo-synchronised ping-pong delay ----------------
            float delL = 0.0f, delR = 0.0f;
            delay.processSample(dryL, dryR, delL, delR);

            // ---------------- 4-line FDN reverb ----------------
            float revL = 0.0f, revR = 0.0f;
            reverb.processSample(dryL, dryR, revL, revR);

            // Shaped before it is mixed, so only the tail is affected and the
            // dry voice keeps its own low end intact.
            revL = reverbAir.processSample(0, reverbHP.processSample(0, revL));
            revR = reverbAir.processSample(1, reverbHP.processSample(1, revR));

            const float dblMix = 0.34f * doubleAmount.getNextValue();
            const float delMix = 0.42f * delayAmount.getNextValue();
            const float space = spaceAmount.getNextValue();
            const float revMix = 0.38f * std::pow(space, 1.15f);

            // Doubles duck only a little; delay/reverb get out of the way much more.
            float wetL =
                dblL * dblMix * (0.72f + 0.28f * wetDuck)
                + delL * delMix * wetDuck
                + revL * revMix * wetDuck;

            float wetR =
                dblR * dblMix * (0.72f + 0.28f * wetDuck)
                + delR * delMix * wetDuck
                + revR * revMix * wetDuck;

            // Wet-only M/S width. 50% ≈ unity side, 100% ≈ 2x side.
            const float widthValue = width.getNextValue();
            const float mid = 0.5f * (wetL + wetR);
            const float side = 0.5f * (wetL - wetR);
            const float sideScale = 2.0f * widthValue;

            wetL = mid + side * sideScale;
            wetR = mid - side * sideScale;

            const float wet = bypassBlend.getNextValue();
            leftOut[i] = dryL + wet * wetL;

            if (rightOut != nullptr)
                rightOut[i] = dryR + wet * wetR;
            else
                leftOut[i] = dryL + wet * 0.5f * (wetL + wetR);
        }
    }

private:
    juce::SmoothedValue<float> bypassBlend;
    double sr = 48000.0;
    int numChannels = 2;
    bool enabled = true;
    double tempo = 120.0;
    int division = 1;

    /*  Six voices rather than two.

        A two-voice doubler is heard as two takes: the ear locates each copy and
        counts them. Past about four decorrelated copies it stops being able to,
        and the same processing reads as one wide, thick voice instead — which
        is the effect that is wanted, and why it has to be subtle to work.

        Everything below 30 ms stays inside the window where a delayed copy
        fuses with the original instead of being heard as an echo.

        The rates are the part that matters most. Any two LFOs whose rates form
        a simple ratio drift into alignment periodically, and when several
        copies swing together the result pulses audibly. These are all primes
        over a hundred, so no two ever line up and the movement stays as a
        texture rather than becoming an event.
    */
    struct DoubleVoice { float baseMs, depthMs, rateHz, pan; double increment; };

    static constexpr size_t numDoubleVoices = 6;
    std::array<DoubleVoice, numDoubleVoices> doubleVoices { {
        //  base   depth   rate   pan
        {   11.3f,  1.4f,  0.19f, -1.00f, 0.0 },
        {   17.9f,  1.9f,  0.31f,  0.70f, 0.0 },
        {   23.1f,  1.5f,  0.23f, -0.60f, 0.0 },
        {   14.7f,  2.2f,  0.41f,  1.00f, 0.0 },
        {   27.3f,  1.7f,  0.29f, -0.35f, 0.0 },
        {   20.5f,  2.0f,  0.37f,  0.40f, 0.0 }
    } };
    std::array<double, numDoubleVoices> doublePhase {};
    // Incoherent sources sum in power, so this keeps six of them at the level
    // the two-voice version delivered rather than three times it.
    static constexpr float doubleNormalise = 0.577f;   // 1 / sqrt(3)

    FractionalDelay doubler;
    DuckingDelay delay;
    void applyReverbTone()
    {
        // Full body still cuts at 90 Hz: nothing a vocal reverb puts below that
        // is worth the mix space it takes.
        reverbHP.setHighPass(90.0 + 260.0 * (1.0 - static_cast<double>(body)), 0.707);
        reverbAir.setHighShelf(6000.0, static_cast<double>(airDb));
    }

    Biquad reverbHP, reverbAir;
    float body = 0.5f, airDb = 0.0f;
    FDNReverb reverb;

    Biquad doubleHP, doubleLP;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> width;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> doubleAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delayAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> spaceAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> duckAmount;

    float envelope = 0.0f;
    float envAttack = 0.99f, envRelease = 0.999f;
};

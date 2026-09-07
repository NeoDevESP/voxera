#pragma once
#include <JuceHeader.h>
#include "FractionalDelay.h"
#include "DuckingDelay.h"
#include "FDNReverb.h"
#include "Biquad.h"
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

        doubleLeft.prepare(sr, 0.080);
        doubleRight.prepare(sr, 0.080);

        delay.prepare(sr);
        reverb.prepare(sr);

        doubleHP.prepare(sr, 2);
        doubleLP.prepare(sr, 2);
        doubleHP.setHighPass(170.0, 0.707);
        doubleLP.setLowPass(12000.0, 0.707);

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

        phaseL = 0.0;
        phaseR = 0.37;
        envelope = 0.0f;

        reset();
    }

    void reset()
    {
        bypassBlend.setCurrentAndTargetValue(enabled ? 1.0f : 0.0f);
        doubleLeft.reset();
        doubleRight.reset();
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

        const double incL = juce::MathConstants<double>::twoPi * 0.17 / sr;
        const double incR = juce::MathConstants<double>::twoPi * 0.23 / sr;

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
            doubleLeft.push(mono);
            doubleRight.push(mono);

            const float modLms = 13.7f + 1.65f * static_cast<float>(std::sin(phaseL));
            const float modRms = 21.3f + 2.15f * static_cast<float>(std::sin(phaseR));

            float dblL = doubleLeft.read(doubleLeft.msToSamples(modLms));
            float dblR = doubleRight.read(doubleRight.msToSamples(modRms));

            dblL = doubleHP.processSample(0, dblL);
            dblR = doubleHP.processSample(1, dblR);
            dblL = doubleLP.processSample(0, dblL);
            dblR = doubleLP.processSample(1, dblR);

            phaseL += incL;
            phaseR += incR;
            if (phaseL >= juce::MathConstants<double>::twoPi) phaseL -= juce::MathConstants<double>::twoPi;
            if (phaseR >= juce::MathConstants<double>::twoPi) phaseR -= juce::MathConstants<double>::twoPi;

            // ---------------- Tempo-synchronised ping-pong delay ----------------
            float delL = 0.0f, delR = 0.0f;
            delay.processSample(dryL, dryR, delL, delR);

            // ---------------- 4-line FDN reverb ----------------
            float revL = 0.0f, revR = 0.0f;
            reverb.processSample(dryL, dryR, revL, revR);

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

    FractionalDelay doubleLeft, doubleRight;
    DuckingDelay delay;
    FDNReverb reverb;

    Biquad doubleHP, doubleLP;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> width;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> doubleAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delayAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> spaceAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> duckAmount;

    double phaseL = 0.0, phaseR = 0.37;
    float envelope = 0.0f;
    float envAttack = 0.99f, envRelease = 0.999f;
};

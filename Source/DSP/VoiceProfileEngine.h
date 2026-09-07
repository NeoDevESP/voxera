#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

/*
    Real-time voice profiler.

    The profile is deliberately compact:
      - average RMS / crest
      - low-mid density
      - presence density
      - sibilance density
      - spectral brightness proxy
      - pitch confidence and range

    Analysis runs on fixed biquad bands + scalar statistics.
    No FFT allocation, no model inference, no locks.
*/
class VoiceProfileEngine
{
public:
    struct Profile
    {
        float avgRmsDb = -60.0f;
        float crestDb = 0.0f;
        float lowMid = 0.0f;
        float presence = 0.0f;
        float sibilance = 0.0f;
        float brightness = 0.0f;
        float pitchConfidence = 0.0f;
        float pitchRangeSemitones = 0.0f;
        bool ready = false;
    };

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        cLow = coeff(500.0f);
        cMid = coeff(4000.0f);
        cHigh = coeff(8000.0f);
        cAir = coeff(12000.0f);
        reset();
    }

    void reset()
    {
        active = false;
        elapsedSamples = 0;
        voicedFrames = 0;

        sumSq = 0.0;
        peak = 0.0f;
        sampleCount = 0;

        lowState = midState = highState = airState = 0.0f;
        lowEnergy = midEnergy = highEnergy = airEnergy = totalEnergy = 0.0;

        pitchConfSum = 0.0;
        minPitchHz = 1.0e9f;
        maxPitchHz = 0.0f;

        current = {};
    }

    void startCapture(float seconds = 8.0f)
    {
        reset();
        active = true;
        targetSamples = static_cast<int64_t>(juce::jmax(2.0f, seconds) * sr);
    }

    bool isCapturing() const noexcept { return active; }

    void pushSample(float mono) noexcept
    {
        if (!active)
            return;

        const float ax = std::abs(mono);
        sumSq += static_cast<double>(mono) * static_cast<double>(mono);
        peak = juce::jmax(peak, ax);
        ++sampleCount;
        ++elapsedSamples;

        // Simple one-pole band proxies at ~500 Hz, 4 kHz, 8 kHz, 12 kHz.

        lowState += cLow * (mono - lowState);
        midState += cMid * (mono - midState);
        highState += cHigh * (mono - highState);
        airState += cAir * (mono - airState);

        const float lowBand = lowState;
        const float presenceBand = midState - lowState;
        const float sibilantBand = highState - midState;
        const float airBand = airState - highState;

        lowEnergy += static_cast<double>(lowBand) * lowBand;
        midEnergy += static_cast<double>(presenceBand) * presenceBand;
        highEnergy += static_cast<double>(sibilantBand) * sibilantBand;
        airEnergy += static_cast<double>(airBand) * airBand;
        totalEnergy += static_cast<double>(mono) * mono;

        if (elapsedSamples >= targetSamples)
            finalise();
    }

    void pushPitch(float hz, float confidence) noexcept
    {
        if (!active || hz <= 0.0f || confidence < 0.5f)
            return;

        pitchConfSum += confidence;
        minPitchHz = juce::jmin(minPitchHz, hz);
        maxPitchHz = juce::jmax(maxPitchHz, hz);
        ++voicedFrames;
    }

    Profile getProfile() const noexcept { return current; }
    void restoreProfile(const Profile& p) noexcept { reset(); current = p; }
    float getProgress() const noexcept {
        return targetSamples > 0 ? juce::jlimit(0.0f, 1.0f,
            static_cast<float>(elapsedSamples) / static_cast<float>(targetSamples)) : 0.0f;
    }

private:
    float coeff(float hz) const noexcept
    {
        return 1.0f - std::exp(
            -juce::MathConstants<float>::twoPi * hz / static_cast<float>(sr));
    }

    void finalise() noexcept
    {
        active = false;

        const double n = static_cast<double>(juce::jmax<int64_t>(1, sampleCount));
        const float rms = static_cast<float>(std::sqrt(sumSq / n));
        const float rmsDb = juce::Decibels::gainToDecibels(rms, -120.0f);
        const float peakDb = juce::Decibels::gainToDecibels(peak, -120.0f);

        const double denom = juce::jmax(1.0e-12, totalEnergy);

        current.avgRmsDb = rmsDb;
        current.crestDb = peakDb - rmsDb;
        current.lowMid = static_cast<float>(lowEnergy / denom);
        current.presence = static_cast<float>(midEnergy / denom);
        current.sibilance = static_cast<float>(highEnergy / denom);
        current.brightness = static_cast<float>((highEnergy + airEnergy) / denom);
        current.pitchConfidence =
            voicedFrames > 0 ? static_cast<float>(pitchConfSum / voicedFrames) : 0.0f;

        if (voicedFrames > 2 && minPitchHz > 0.0f && maxPitchHz > minPitchHz)
            current.pitchRangeSemitones =
                12.0f * std::log2(maxPitchHz / minPitchHz);

        current.ready = sampleCount > static_cast<int64_t>(sr * 2.0)
            && rmsDb > -55.0f;
    }

    float cLow = 0.0f, cMid = 0.0f, cHigh = 0.0f, cAir = 0.0f;
    double sr = 48000.0;
    bool active = false;
    int64_t elapsedSamples = 0;
    int64_t targetSamples = 0;
    int64_t sampleCount = 0;

    double sumSq = 0.0;
    float peak = 0.0f;

    float lowState = 0.0f, midState = 0.0f, highState = 0.0f, airState = 0.0f;
    double lowEnergy = 0.0, midEnergy = 0.0, highEnergy = 0.0, airEnergy = 0.0, totalEnergy = 0.0;

    int voicedFrames = 0;
    double pitchConfSum = 0.0;
    float minPitchHz = 1.0e9f;
    float maxPitchHz = 0.0f;

    Profile current {};
};

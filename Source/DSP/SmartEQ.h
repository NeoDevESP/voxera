#pragma once
#include "Biquad.h"
#include <atomic>

// A conservative local spectral-excess detector, not a learned voice model.
// Input-only analysis, linked stereo powers, no allocation or added sample delay.
class SmartEQ
{
public:
    static constexpr std::array<float, 5> frequencies { 250, 500, 1000, 2000, 4000 };
    void prepare(double rate, int channels)
    {
        sr = juce::jmax(8000.0, rate);
        channelCount = juce::jlimit(1, 2, channels);
        for (size_t i = 0; i < detectors.size(); ++i) {
            detectors[i].prepare(sr, channelCount);
            detectors[i].setBandPass(125.0 * std::pow(2.0, static_cast<double>(i)), detectorQ);
        }
        for (auto& f : filters) f.prepare(sr, channelCount);
        interval = juce::jmax(1, static_cast<int>(std::round(sr * 0.005)));
        energyCoefficient = static_cast<float>(std::exp(-1.0 / (sr * 0.2)));
        reset();
    }
    void reset()
    {
        for (auto& d : detectors) d.reset();
        for (auto& f : filters) { f.reset(); f.setIdentity(); }
        powers.fill(0); gains.fill(0); totalPower = 0; countdown = 0;
        for (auto& v : displayedGains) v.store(0, std::memory_order_relaxed);
    }
    void setParameters(float strength, float budgetDb, float responseMs)
    {
        amount = juce::jlimit(0.0f, 1.0f, strength);
        budget = juce::jlimit(1.0f, 6.0f, budgetDb);
        response = juce::jlimit(100.0f, 1000.0f, responseMs);
    }
    float gainDb(size_t band) const noexcept { return displayedGains[band].load(std::memory_order_relaxed); }
    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(channelCount, buffer.getNumChannels());
        if (channels == 0) return;
        for (int n = 0; n < buffer.getNumSamples(); ++n) {
            std::array<float, 2> input {};
            float power = 0;
            for (int c = 0; c < channels; ++c) {
                const auto x = buffer.getSample(c, n);
                input[static_cast<size_t>(c)] = std::isfinite(x) ? x : 0.0f;
                power += input[static_cast<size_t>(c)] * input[static_cast<size_t>(c)] / channels;
            }
            totalPower = energyCoefficient * totalPower + (1.0f - energyCoefficient) * power;
            for (size_t b = 0; b < detectors.size(); ++b) {
                float energy = 0;
                for (int c = 0; c < channels; ++c) {
                    const float y = detectors[b].processSample(c, input[static_cast<size_t>(c)]);
                    energy += y * y / channels;
                }
                powers[b] = energyCoefficient * powers[b] + (1.0f - energyCoefficient) * energy;
            }
            if (--countdown <= 0) { update(); countdown = interval; }
            for (int c = 0; c < channels; ++c) {
                float x = input[static_cast<size_t>(c)];
                for (auto& f : filters) x = f.processSample(c, x);
                buffer.setSample(c, n, x);
            }
        }
    }
private:
    void update()
    {
        std::array<float, 5> targets {};
        float sum = 0;
        // Power gate at -55 dBFS; no automatic boosts into silence or noise.
        if (totalPower > 3.162278e-6f && amount > 0) {
            for (size_t b = 0; b < targets.size(); ++b) {
                if (frequencies[b] > sr * 0.3) continue;
                auto db = [this](size_t i) { return 10.0f * std::log10(juce::jmax(1.0e-16f, powers[i])); };
                // Log-frequency curvature: a broad spectral tilt is not a fault.
                const float excess = db(b + 1) - 0.5f * (db(b) + db(b + 2));
                targets[b] = amount * juce::jlimit(0.0f, budget, (excess - deadbandDb) * slope);
                sum += targets[b];
            }
        }
        const float scale = sum > budget * amount ? budget * amount / sum : 1.0f;
        for (size_t b = 0; b < gains.size(); ++b) {
            const float target = -targets[b] * scale;
            const float ms = target < gains[b] ? response : response * 3.0f;
            const float coefficient = static_cast<float>(std::exp(-interval / (sr * ms * 0.001)));
            gains[b] = coefficient * gains[b] + (1.0f - coefficient) * target;
        }
        // Shared instantaneous budget also covers automation and crossing bands.
        float used = 0; for (float g : gains) used -= g;
        const float limitScale = used > budget ? budget / used : 1.0f;
        for (size_t b = 0; b < gains.size(); ++b) {
            gains[b] *= limitScale;
            if (std::abs(gains[b]) < 1.0e-5f) gains[b] = 0;
            filters[b].setPeaking(frequencies[b], 0.9, gains[b]);
            displayedGains[b].store(gains[b], std::memory_order_relaxed);
        }
    }
    /*  How sharply a band has to stand out before it counts as a resonance.

        The detectors sit an octave apart, so their width decides how much of a
        neighbour's energy leaks into a band's own measurement. At Q=1 a tone one
        octave away still reads only about 5 dB down, which means even a pure
        sine could never show more than roughly 5 dB of excess — and after the
        deadband and slope below, that left less than a decibel of correction
        available no matter how the controls were set. The stage measured as
        working and was inaudible, which is the same thing as not working.

        At Q=2.5 that neighbour falls near 12 dB instead, so a real resonance
        registers as one and the budget the user set becomes reachable. Narrower
        still would start missing the broad, gentle humps that a room puts on a
        voice, which are exactly what this is for.
    */
    static constexpr double detectorQ = 2.5;
    static constexpr float deadbandDb = 2.5f;   // below this it is tilt, not a fault
    static constexpr float slope = 1.0f;

    double sr = 48000;
    int channelCount = 2, interval = 240, countdown = 0;
    float amount = 0, budget = 3, response = 250, totalPower = 0, energyCoefficient = 0;
    std::array<Biquad, 7> detectors;
    std::array<Biquad, 5> filters;
    std::array<float, 7> powers {};
    std::array<float, 5> gains {};
    std::array<std::atomic<float>, 5> displayedGains {};
};

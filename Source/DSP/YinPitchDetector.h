#pragma once
#include <JuceHeader.h>
#include "Biquad.h"
#include <vector>
#include <atomic>
#include <cmath>
#include <algorithm>

/*
    Monophonic F0 tracker based on YIN's cumulative mean normalised
    difference function (CMNDF).

    Analysis runs near 12 kHz after a fourth-order low-pass and decimation.
    Fixed time windows keep the workload bounded at high host sample rates.
    The YIN inner loop uses a preallocated chronological buffer.

    This is an analysis-only component: it never delays or changes audio.
*/
class YinPitchDetector
{
public:
    void prepare(double sampleRate)
    {
        sourceRate = std::isfinite(sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        decimation = juce::jmax(1, static_cast<int>(std::lround(sourceRate / 12000.0)));
        antiAlias1.prepare(sourceRate, 1);
        antiAlias2.prepare(sourceRate, 1);
        const double cutoff = juce::jmin(1800.0, sourceRate / decimation * 0.35);
        antiAlias1.setLowPass(cutoff, 0.541196100146197);
        antiAlias2.setLowPass(cutoff, 1.306562964876377);
        analysisRate = sourceRate / static_cast<double>(decimation);

        // Keep analysis timing and cost comparable at 44.1/48/96/192 kHz.
        frameSize = juce::jmax(128, static_cast<int>(std::ceil(analysisRate * 0.048)));
        hopSize = juce::jmax(1, static_cast<int>(std::lround(analysisRate * 0.008)));

        frame.assign(static_cast<size_t>(frameSize), 0.0f);
        chronological.assign(static_cast<size_t>(frameSize), 0.0f);
        difference.assign(static_cast<size_t>(frameSize / 2), 0.0f);
        cmndf.assign(static_cast<size_t>(frameSize / 2), 1.0f);

        writePos = 0;
        filledSamples = 0;
        samplesSinceAnalysis = 0;
        decimCount = 0;
        decimAccumulator = 0.0f;

        latestHz.store(0.0f);
        latestConfidence.store(0.0f);
        voiced.store(false);
    }

    void reset()
    {
        antiAlias1.reset();
        antiAlias2.reset();
        std::fill(frame.begin(), frame.end(), 0.0f);
        std::fill(difference.begin(), difference.end(), 0.0f);
        std::fill(cmndf.begin(), cmndf.end(), 1.0f);

        writePos = 0;
        filledSamples = 0;
        samplesSinceAnalysis = 0;
        decimCount = 0;
        decimAccumulator = 0.0f;

        latestHz.store(0.0f);
        latestConfidence.store(0.0f);
        voiced.store(false);
    }

    void pushSample(float monoSample) noexcept
    {
        // Analysis only: the audible path is not filtered here.
        const float input = std::isfinite(monoSample) ? monoSample : 0.0f;
        decimAccumulator += antiAlias2.processSample(0, antiAlias1.processSample(0, input));

        if (++decimCount < decimation)
            return;

        const float x = decimAccumulator / static_cast<float>(decimation);
        decimCount = 0;
        decimAccumulator = 0.0f;

        frame[static_cast<size_t>(writePos)] = x;
        writePos = (writePos + 1) % frameSize;
        filledSamples = juce::jmin(frameSize, filledSamples + 1);

        if (++samplesSinceAnalysis >= hopSize)
        {
            samplesSinceAnalysis = 0;

            if (filledSamples >= frameSize)
                analyse();
        }
    }

    double getAnalysisWindowSeconds() const noexcept { return frameSize / analysisRate; }
    double getAnalysisHopSeconds() const noexcept { return hopSize / analysisRate; }
    float getFrequencyHz() const noexcept { return latestHz.load(); }
    float getConfidence() const noexcept { return latestConfidence.load(); }
    bool isVoiced() const noexcept { return voiced.load(); }

private:
    void analyse() noexcept
    {
        // Vocal-focused range. The upper bound also rejects many consonants.
        constexpr float minHz = 60.0f;
        constexpr float maxHz = 1000.0f;

        const int minTau = juce::jmax(
            2, static_cast<int>(std::floor(analysisRate / maxHz)));
        const int maxTau = juce::jmin(
            frameSize / 2 - 2,
            static_cast<int>(std::ceil(analysisRate / minHz)));

        // RMS gate and chronological frame access from circular memory.
        double sumSq = 0.0;
        for (int i = 0; i < frameSize; ++i)
        {
            const float s = sampleAt(i);
            chronological[static_cast<size_t>(i)] = s;
            sumSq += static_cast<double>(s) * static_cast<double>(s);
        }

        const float rms = static_cast<float>(
            std::sqrt(sumSq / static_cast<double>(frameSize)));

        const float rmsDb = juce::Decibels::gainToDecibels(rms, -120.0f);

        if (rmsDb < -56.0f)
        {
            latestHz.store(0.0f);
            latestConfidence.store(0.0f);
            voiced.store(false);
            return;
        }

        std::fill(difference.begin(), difference.end(), 0.0f);

        // Difference function. Restricting the comparison length keeps
        // each tau based on the same number of samples.
        const int compareLength = frameSize - maxTau - 1;

        // CMNDF must accumulate the difference function from tau=1,
        // even though the final search begins at minTau. Skipping the lower
        // lags biases high-note estimates toward octave/subharmonic errors.
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            double sum = 0.0;

            for (int i = 0; i < compareLength; ++i)
            {
                const float d = chronological[static_cast<size_t>(i)] - chronological[static_cast<size_t>(i + tau)];
                sum += static_cast<double>(d) * static_cast<double>(d);
            }

            difference[static_cast<size_t>(tau)] = static_cast<float>(sum);
        }

        // CMNDF.
        float running = 0.0f;
        cmndf[0] = 1.0f;

        for (int tau = 1; tau <= maxTau; ++tau)
        {
            running += difference[static_cast<size_t>(tau)];
            cmndf[static_cast<size_t>(tau)] =
                running > 1.0e-12f
                    ? difference[static_cast<size_t>(tau)]
                        * static_cast<float>(tau) / running
                    : 1.0f;
        }

        // Absolute threshold + local-minimum walk.
        constexpr float threshold = 0.15f;
        int tauEstimate = -1;

        for (int tau = minTau; tau <= maxTau; ++tau)
        {
            if (cmndf[static_cast<size_t>(tau)] < threshold)
            {
                while (tau + 1 <= maxTau
                       && cmndf[static_cast<size_t>(tau + 1)]
                              < cmndf[static_cast<size_t>(tau)])
                    ++tau;

                tauEstimate = tau;
                break;
            }
        }

        // If there was no threshold crossing, still use the best candidate
        // only when its confidence is respectable.
        if (tauEstimate < 0)
        {
            float best = 1.0f;
            int bestTau = -1;

            for (int tau = minTau; tau <= maxTau; ++tau)
            {
                if (cmndf[static_cast<size_t>(tau)] < best)
                {
                    best = cmndf[static_cast<size_t>(tau)];
                    bestTau = tau;
                }
            }

            if (bestTau < 0 || best > 0.34f)
            {
                latestHz.store(0.0f);
            latestConfidence.store(0.0f);
                voiced.store(false);
                return;
            }

            tauEstimate = bestTau;
        }

        // Parabolic interpolation around the CMNDF minimum.
        float refinedTau = static_cast<float>(tauEstimate);

        if (tauEstimate > minTau && tauEstimate < maxTau)
        {
            const float y0 = cmndf[static_cast<size_t>(tauEstimate - 1)];
            const float y1 = cmndf[static_cast<size_t>(tauEstimate)];
            const float y2 = cmndf[static_cast<size_t>(tauEstimate + 1)];

            const float denom = y0 - 2.0f * y1 + y2;

            if (std::abs(denom) > 1.0e-9f)
                refinedTau += 0.5f * (y0 - y2) / denom;
        }

        const float hz = static_cast<float>(analysisRate) /
                         juce::jmax(1.0f, refinedTau);

        const float confidence = juce::jlimit(
            0.0f, 1.0f,
            1.0f - cmndf[static_cast<size_t>(tauEstimate)]);

        // Reject implausible jumps unless the new candidate is very confident.
        const float previous = latestHz.load();
        if (previous > 0.0f)
        {
            const float jumpSemitones =
                12.0f * std::log2(juce::jmax(1.0e-6f, hz / previous));

            if (std::abs(jumpSemitones) > 12.5f && confidence < 0.92f)
            {
                latestConfidence.store(confidence * 0.5f);
                voiced.store(false);
                return;
            }
        }

        latestHz.store(hz);
        latestConfidence.store(confidence);
        voiced.store(confidence >= 0.62f);
    }

    float sampleAt(int chronologicalIndex) const noexcept
    {
        int index = writePos + chronologicalIndex;
        if (index >= frameSize)
            index -= frameSize;

        return frame[static_cast<size_t>(index)];
    }

    int decimation = 4;
    Biquad antiAlias1, antiAlias2;
    std::vector<float> chronological;

    double sourceRate = 48000.0;
    double analysisRate = 12000.0;

    int frameSize = 1024;
    int hopSize = 128;
    int writePos = 0;
    int filledSamples = 0;
    int samplesSinceAnalysis = 0;

    int decimCount = 0;
    float decimAccumulator = 0.0f;

    std::vector<float> frame;
    std::vector<float> difference;
    std::vector<float> cmndf;

    std::atomic<float> latestHz { 0.0f };
    std::atomic<float> latestConfidence { 0.0f };
    std::atomic<bool> voiced { false };
};

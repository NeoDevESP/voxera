#pragma once
#include <JuceHeader.h>
#include "Biquad.h"
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

class AdaptiveSpectralEngine
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int spectrumBins = fftSize / 2 + 1;

    AdaptiveSpectralEngine()
        : fft(fftOrder)
    {
        for (int i = 0; i < fftSize; ++i)
        {
            // Periodic Hann, suitable for overlapped analysis.
            window[(size_t)i] = 0.5f - 0.5f * std::cos(
                juce::MathConstants<float>::twoPi * static_cast<float>(i) / static_cast<float>(fftSize));
        }
    }

    void prepare(double sampleRate, int maxBlockSize, int channels)
    {
        juce::ignoreUnused(maxBlockSize);

        sr = sampleRate;
        controlInterval = juce::jmax(1, static_cast<int>(std::lround(sr * 0.001)));
        controlCountdown = 0;
        bypassBlend.reset(sr, 0.020);
        bypassBlend.setCurrentAndTargetValue(enabled ? 1.0f : 0.0f);
        toneBlend.reset(sr, 0.020);
        toneBlend.setCurrentAndTargetValue(enabled ? 1.0f : 0.0f);
        deEssBlend.reset(sr, 0.020);
        deEssBlend.setCurrentAndTargetValue(enabled ? 1.0f : 0.0f);
        numChannels = juce::jlimit(1, 2, channels);
        ring.fill(0.0f);
        fftData.fill(0.0f);
        spectrumDb.fill(-120.0f);

        writeIndex = 0;
        samplesUntilAnalysis = fftSize;
        framesSeen = 0;

        track1 = {};
        track2 = {};
        sibilance = {};
        latestVoiceEnergy = -120.0f;

        resonance1.prepare(sr, numChannels);
        resonance2.prepare(sr, numChannels);
        deEsserFilter.prepare(sr, numChannels);
        bodyShelf.prepare(sr, numChannels);
        presenceBell.prepare(sr, numChannels);
        airShelf.prepare(sr, numChannels);

        resonance1.reset();
        resonance2.reset();
        deEsserFilter.reset();
        bodyShelf.reset();
        presenceBell.reset();
        airShelf.reset();

        currentBodyDb = 0.0f;
        currentPresenceDb = 0.0f;
        currentAirDb = 0.0f;
        currentCut1Db = currentCut2Db = currentDeEssDb = 0.0f;

        updateStaticToneFilters();
        updateDynamicFilters(0);
    }

    void reset()
    {
        controlCountdown = 0;
        bypassBlend.setCurrentAndTargetValue(enabled ? 1.0f : 0.0f);
        // Both halves, or the tone section keeps whatever fade position it was
        // left in and comes back at the wrong level after a transport stop.
        toneBlend.setCurrentAndTargetValue(enabled ? 1.0f : 0.0f);
        deEssBlend.setCurrentAndTargetValue(enabled ? 1.0f : 0.0f);
        ring.fill(0.0f);
        fftData.fill(0.0f);
        spectrumDb.fill(-120.0f);

        writeIndex = 0;
        samplesUntilAnalysis = fftSize;
        framesSeen = 0;

        track1 = {};
        track2 = {};
        sibilance = {};

        resonance1.reset();
        resonance2.reset();
        deEsserFilter.reset();
        bodyShelf.reset();
        presenceBell.reset();
        airShelf.reset();
    }

    void setEnabled(bool v) noexcept { enabled = v; }
    void setClean(float v01) noexcept { clean = juce::jlimit(0.0f, 1.0f, v01); }
    void setDeEss(float v01) noexcept { deEssAmount = juce::jlimit(0.0f, 1.0f, v01); }

    void setBodyDb(float db) noexcept
    {
        targetBodyDb = juce::jlimit(-6.0f, 6.0f, db);
    }

    void setPresenceDb(float db) noexcept
    {
        targetPresenceDb = juce::jlimit(-6.0f, 6.0f, db);
    }

    void setAirDb(float db) noexcept
    {
        targetAirDb = juce::jlimit(-8.0f, 8.0f, db);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumSamples() == 0)
            return;

        bypassBlend.setTargetValue(enabled ? 1.0f : 0.0f);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                mono += buffer.getReadPointer(ch)[i];
            pushForAnalysisSample(mono / static_cast<float>(juce::jmax(1, buffer.getNumChannels())));
            if (controlCountdown-- <= 0)
            {
                updateControlSmoothing(controlInterval);
                updateStaticToneFilters();
                updateDynamicFilters(controlInterval);
                controlCountdown = controlInterval - 1;
            }
            const float wet = bypassBlend.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto& output = buffer.getWritePointer(ch)[i];
                const float dry = output;
                float x = resonance1.processSample(ch, dry);
                x = resonance2.processSample(ch, x);
                output = dry + wet * (x - dry);
            }
        }
    }

    /*  The tone half, run separately so it can sit after the dynamics.

        These three shelves and the bell are the only part of this stage that
        adds anything; everything above takes away. The distinction decides
        where each belongs, and it is the oldest rule in the chain: what you
        remove goes before the compressor, what you add goes after.

        Removing first means the compressor is not reacting to mud and
        sibilance that were on their way out anyway — it works on the signal
        that is actually staying. Adding afterwards means the compressor cannot
        undo the boost: a presence lift made ahead of it is simply louder in
        the band the detector is watching, so the compressor pulls it straight
        back down and the control ends up fighting itself, which is heard as an
        EQ that stops doing much past a certain point.

        This used to run as one block ahead of the dynamics, so the additive
        half was in exactly that position.
    */
    void processTone(juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumSamples() == 0) return;

        // Its own smoother, advanced once per sample like the other, so the two
        // halves fade together rather than one lagging the other by a block.
        toneBlend.setTargetValue(enabled ? 1.0f : 0.0f);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float wet = toneBlend.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto& output = buffer.getWritePointer(ch)[i];
                const float dry = output;
                float x = bodyShelf.processSample(ch, dry);
                x = presenceBell.processSample(ch, x);
                x = airShelf.processSample(ch, x);
                output = dry + wet * (x - dry);
            }
        }
    }

    /*  The de-esser, run last of all rather than with the other corrections.

        Sibilance is not only what the singer produced. Saturation, an exciter
        and a presence lift all make more of it, and every one of those sits
        downstream of the corrective filters — so a de-esser placed with them
        smooths the esses that arrived and then hands them to three stages that
        put the harshness back. The literature is unambiguous about this: the
        de-esser belongs at the end of the chain precisely so that it catches
        what the rest of the chain added.

        It stays part of this class because the sibilance measurement it follows
        comes from the analysis above, and moving the filter somewhere else
        would mean either duplicating that transform or wiring the score across
        to a stage that has no other reason to know about it.
    */
    void processDeEss(juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumSamples() == 0) return;
        deEssBlend.setTargetValue(enabled ? 1.0f : 0.0f);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float wet = deEssBlend.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto& output = buffer.getWritePointer(ch)[i];
                const float dry = output;
                output = dry + wet * (deEsserFilter.processSample(ch, dry) - dry);
            }
        }
    }

    /*  The magnitudes from the most recent analysis frame, so a later stage can
        read the spectrum instead of estimating one of its own.

        This transform is already being computed and then discarded; anything
        downstream that needs to know the shape of the spectrum was previously
        approximating it with filters. Sharing costs nothing.

        Two things the caller has to know. The frame is of this engine's INPUT,
        taken before its own filters are applied, so a reader placed after it
        sees the spectrum as it was before this stage corrected anything. And
        it is only valid on the audio thread between process calls — there is no
        synchronisation here because both stages run in sequence on that thread.
    */
    static constexpr int analysisBinCount = spectrumBins;
    const float* analysisMagnitudes() const noexcept { return fftData.data(); }
    double analysisBinHz() const noexcept { return sr / static_cast<double>(fftSize); }
    // Increments on each new frame, so a reader can tell fresh data from stale.
    uint32_t analysisFrame() const noexcept { return frameCounter; }
    float analysisScale() const noexcept { return 1.0f / static_cast<float>(fftSize); }

    std::atomic<float> detectedResonance1Hz { 0.0f };
    std::atomic<float> detectedResonance2Hz { 0.0f };
    std::atomic<float> resonance1Score { 0.0f };
    std::atomic<float> resonance2Score { 0.0f };
    std::atomic<float> sibilanceScore { 0.0f };
    std::atomic<float> detectedSibilanceHz { 7000.0f };
    std::atomic<float> voiceEnergyDb { -120.0f };

private:
    struct Track
    {
        float frequency = 0.0f;
        float score = 0.0f;
        float persistence = 0.0f;
    };

    struct SibilanceState
    {
        float frequency = 7000.0f;
        float score = 0.0f;
    };

    struct Candidate
    {
        float frequency = 0.0f;
        float prominenceDb = 0.0f;
        float score = 0.0f;
    };

    void pushForAnalysisSample(float mono)
    {
        ring[static_cast<size_t>(writeIndex)] = mono;
        writeIndex = (writeIndex + 1) & (fftSize - 1);
        if (--samplesUntilAnalysis <= 0)
        {
            analyseFrame();
            samplesUntilAnalysis = hopSize;
        }
    }

    void analyseFrame()
    {
        double timeEnergy = 0.0;

        for (int i = 0; i < fftSize; ++i)
        {
            const int ringIndex = (writeIndex + i) & (fftSize - 1);
            const float s = ring[(size_t)ringIndex];
            timeEnergy += static_cast<double>(s) * static_cast<double>(s);
            fftData[(size_t)i] = s * window[(size_t)i];
        }

        std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform(fftData.data(), true);
        ++frameCounter;

        const float rms = static_cast<float>(std::sqrt(timeEnergy / static_cast<double>(fftSize)));
        latestVoiceEnergy = juce::Decibels::gainToDecibels(rms, -120.0f);
        voiceEnergyDb.store(latestVoiceEnergy);

        // Ignore very quiet/no-signal frames to avoid chasing noise.
        const bool activeVoice = latestVoiceEnergy > -55.0f;

        for (int bin = 0; bin < spectrumBins; ++bin)
        {
            const float magnitude = fftData[(size_t)bin] / static_cast<float>(fftSize);
            spectrumDb[(size_t)bin] = juce::Decibels::gainToDecibels(magnitude, -120.0f);
        }

        std::array<Candidate, 8> candidates {};
        int candidateCount = 0;

        const int minBin = freqToBin(160.0f);
        const int maxBin = freqToBin(8000.0f);

        for (int bin = juce::jmax(minBin, 8); bin < juce::jmin(maxBin, spectrumBins - 8); ++bin)
        {
            const float here = spectrumDb[(size_t)bin];

            if (!(here > spectrumDb[(size_t)(bin - 1)] && here >= spectrumDb[(size_t)(bin + 1)]))
                continue;

            float env = 0.0f;
            int envN = 0;

            // Local spectral envelope excludes the centre area so a narrow peak
            // is measured against its neighbourhood rather than itself.
            for (int k = -7; k <= 7; ++k)
            {
                if (std::abs(k) <= 2)
                    continue;

                env += spectrumDb[(size_t)(bin + k)];
                ++envN;
            }

            env /= static_cast<float>(juce::jmax(1, envN));
            const float prominence = here - env;

            if (prominence < 3.0f)
                continue;

            const float freq = binToFreq(bin);

            // Perceptual weighting avoids overreacting to very low harmonics.
            float weight = 1.0f;
            if (freq < 300.0f) weight *= 0.55f;
            if (freq > 6500.0f) weight *= 0.80f;

            const float score = juce::jlimit(0.0f, 1.0f, (prominence - 3.0f) / 9.0f) * weight;

            if (candidateCount < static_cast<int>(candidates.size()))
            {
                candidates[(size_t)candidateCount++] = { freq, prominence, score };
            }
            else
            {
                int weakest = 0;
                for (int c = 1; c < candidateCount; ++c)
                    if (candidates[(size_t)c].score < candidates[(size_t)weakest].score)
                        weakest = c;

                if (score > candidates[(size_t)weakest].score)
                    candidates[(size_t)weakest] = { freq, prominence, score };
            }
        }

        std::sort(candidates.begin(), candidates.begin() + candidateCount,
                  [](const Candidate& a, const Candidate& b)
                  {
                      return a.score > b.score;
                  });

        Candidate best1 {}, best2 {};

        if (candidateCount > 0)
            best1 = candidates[0];

        for (int i = 1; i < candidateCount; ++i)
        {
            if (best1.frequency <= 0.0f)
                break;

            const float octaveDistance = std::abs(std::log2(candidates[(size_t)i].frequency / best1.frequency));
            if (octaveDistance > 0.12f)
            {
                best2 = candidates[(size_t)i];
                break;
            }
        }

        if (!activeVoice)
        {
            best1.score = 0.0f;
            best2.score = 0.0f;
        }

        updateTrack(track1, best1);
        updateTrack(track2, best2);
        analyseSibilance(activeVoice);

        detectedResonance1Hz.store(track1.frequency);
        detectedResonance2Hz.store(track2.frequency);
        resonance1Score.store(track1.score * track1.persistence);
        resonance2Score.store(track2.score * track2.persistence);
        detectedSibilanceHz.store(sibilance.frequency);
        sibilanceScore.store(sibilance.score);

        ++framesSeen;
    }

    void updateTrack(Track& t, const Candidate& c)
    {
        if (c.frequency <= 0.0f || c.score <= 0.0f)
        {
            t.persistence *= 0.78f;
            t.score *= 0.82f;
            return;
        }

        if (t.frequency <= 0.0f)
        {
            t.frequency = c.frequency;
            t.score = c.score;
            t.persistence = 0.25f;
            return;
        }

        const float octaveDistance = std::abs(std::log2(c.frequency / t.frequency));
        const bool sameRegion = octaveDistance < 0.10f;

        if (sameRegion)
        {
            // Frequency smoothing stops narrow filters from jumping between harmonic bins.
            t.frequency = 0.82f * t.frequency + 0.18f * c.frequency;
            t.score = 0.72f * t.score + 0.28f * c.score;
            t.persistence = juce::jlimit(0.0f, 1.0f, t.persistence + 0.12f);
        }
        else
        {
            t.persistence *= 0.72f;

            // Only retarget quickly when the new candidate is clearly stronger
            // or the old track has mostly disappeared.
            if (c.score > t.score * 1.20f || t.persistence < 0.18f)
            {
                t.frequency = c.frequency;
                t.score = c.score;
                t.persistence = 0.22f;
            }
        }
    }

    void analyseSibilance(bool activeVoice)
    {
        const int totalMin = freqToBin(250.0f);
        const int totalMax = freqToBin(14000.0f);
        const int sibMin = freqToBin(4200.0f);
        const int sibMax = freqToBin(11000.0f);

        double totalPower = 1.0e-12;
        double sibPower = 0.0;
        float strongestDb = -120.0f;
        float strongestHz = 7000.0f;

        for (int bin = totalMin; bin <= totalMax && bin < spectrumBins; ++bin)
        {
            const float db = spectrumDb[(size_t)bin];
            const float mag = juce::Decibels::decibelsToGain(db);
            const double p = static_cast<double>(mag) * static_cast<double>(mag);
            totalPower += p;

            if (bin >= sibMin && bin <= sibMax)
            {
                sibPower += p;

                if (db > strongestDb)
                {
                    strongestDb = db;
                    strongestHz = binToFreq(bin);
                }
            }
        }

        const float ratio = static_cast<float>(sibPower / totalPower);

        // Typical singing can already carry substantial high-frequency energy.
        // We deliberately make the detector conservative.
        float score = juce::jlimit(0.0f, 1.0f, (ratio - 0.16f) / 0.28f);

        // A bright sustained vowel should not automatically count as an "S".
        // Sibilance is weighted up when the strongest HF component dominates.
        const float hfPeakWeight = juce::jlimit(
            0.0f, 1.0f,
            (strongestDb - latestVoiceEnergy - 10.0f) / 18.0f);

        score *= (0.55f + 0.45f * hfPeakWeight);

        if (!activeVoice)
            score = 0.0f;

        sibilance.score = 0.70f * sibilance.score + 0.30f * score;

        if (score > 0.08f)
            sibilance.frequency = 0.82f * sibilance.frequency + 0.18f * strongestHz;
    }

    void updateControlSmoothing(int blockSamples)
    {
        const float dt = static_cast<float>(blockSamples / sr);

        auto smooth = [dt](float current, float target, float tau)
        {
            const float a = std::exp(-dt / juce::jmax(0.001f, tau));
            return a * current + (1.0f - a) * target;
        };

        currentBodyDb = smooth(currentBodyDb, targetBodyDb, 0.035f);
        currentPresenceDb = smooth(currentPresenceDb, targetPresenceDb, 0.035f);
        currentAirDb = smooth(currentAirDb, targetAirDb, 0.045f);

        const float target1 = -5.5f * clean * resonance1Score.load();
        const float target2 = -4.5f * clean * resonance2Score.load();
        const float targetDeEss = -7.0f * deEssAmount * sibilanceScore.load();

        // Cuts attack faster than they release.
        currentCut1Db = smooth(currentCut1Db, target1,
                               target1 < currentCut1Db ? 0.012f : 0.120f);
        currentCut2Db = smooth(currentCut2Db, target2,
                               target2 < currentCut2Db ? 0.014f : 0.135f);
        currentDeEssDb = smooth(currentDeEssDb, targetDeEss,
                                targetDeEss < currentDeEssDb ? 0.006f : 0.090f);
    }

    void updateStaticToneFilters()
    {
        bodyShelf.setLowShelf(180.0, currentBodyDb, 0.85);
        presenceBell.setPeaking(2600.0, 0.80, currentPresenceDb);
        airShelf.setHighShelf(10500.0, currentAirDb, 0.75);
    }

    void updateDynamicFilters(int blockSamples)
    {
        juce::ignoreUnused(blockSamples);

        const float f1 = juce::jlimit(180.0f, 7800.0f,
                                     detectedResonance1Hz.load() > 0.0f
                                         ? detectedResonance1Hz.load()
                                         : 1200.0f);

        const float f2 = juce::jlimit(180.0f, 7800.0f,
                                     detectedResonance2Hz.load() > 0.0f
                                         ? detectedResonance2Hz.load()
                                         : 3200.0f);

        const float fs = juce::jlimit(4200.0f, 11000.0f, detectedSibilanceHz.load());

        // Q rises slightly with detector confidence: strong persistent resonances
        // get narrower, weaker issues get gentler treatment.
        const float q1 = 2.2f + 3.8f * resonance1Score.load();
        const float q2 = 2.0f + 3.2f * resonance2Score.load();
        const float qS = 1.5f + 1.2f * sibilanceScore.load();

        resonance1.setPeaking(f1, q1, currentCut1Db);
        resonance2.setPeaking(f2, q2, currentCut2Db);
        deEsserFilter.setPeaking(fs, qS, currentDeEssDb);
    }

    int freqToBin(float hz) const noexcept
    {
        return juce::jlimit(0, spectrumBins - 1,
                           static_cast<int>(std::round(hz * static_cast<float>(fftSize)
                                                      / static_cast<float>(sr))));
    }

    float binToFreq(int bin) const noexcept
    {
        return static_cast<float>(bin) * static_cast<float>(sr)
             / static_cast<float>(fftSize);
    }

    juce::SmoothedValue<float> bypassBlend, toneBlend, deEssBlend;
    int controlInterval = 48, controlCountdown = 0;
    double sr = 48000.0;
    int numChannels = 2;
    bool enabled = true;

    float clean = 0.55f;
    float deEssAmount = 0.55f;

    float targetBodyDb = 0.0f;
    float targetPresenceDb = 0.0f;
    float targetAirDb = 0.0f;

    float currentBodyDb = 0.0f;
    float currentPresenceDb = 0.0f;
    float currentAirDb = 0.0f;
    float currentCut1Db = 0.0f;
    float currentCut2Db = 0.0f;
    float currentDeEssDb = 0.0f;

    juce::dsp::FFT fft;
    std::array<float, fftSize> window {};
    std::array<float, fftSize> ring {};
    std::array<float, fftSize * 2> fftData {};
    uint32_t frameCounter = 0;
    std::array<float, spectrumBins> spectrumDb {};

    int writeIndex = 0;
    int samplesUntilAnalysis = hopSize;
    int framesSeen = 0;
    float latestVoiceEnergy = -120.0f;

    Track track1 {};
    Track track2 {};
    SibilanceState sibilance {};

    Biquad resonance1;
    Biquad resonance2;
    Biquad deEsserFilter;
    Biquad bodyShelf;
    Biquad presenceBell;
    Biquad airShelf;
};

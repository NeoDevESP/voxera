#pragma once
#include <JuceHeader.h>
#include "Biquad.h"
#include <array>
#include <atomic>
#include <cmath>

namespace voxera
{
/*  Matches a take to a reference take of the same singer.

    Voice conversion where the target is you. The problem it solves is ordinary
    and has no good manual fix: takes recorded on different days, at different
    distances from the microphone, or after a different amount of sleep do not
    sit together, and no amount of level matching makes them. What differs is
    the balance across the spectrum, and that is what this measures and moves.

    Both the reference and the live signal are read from the same point in the
    chain, so the difference between them means something. The correction is
    applied further downstream, which means the measurement never sees the
    correction and the loop does not close — deliberately. A converging loop
    here would chase its own output and take minutes to settle on material that
    changes every phrase.

    The bands are normalised by their own mean before comparison, so what is
    matched is the shape and not the loudness. Level belongs to the auto-gain
    upstream, and a stage that fought it over the same territory would leave
    neither doing its job properly.
*/
class VoiceMatch
{
public:
    static constexpr int numBands = 8;

    // Kept as a plain array so it can be written into the plugin state and read
    // back without knowing anything about this class.
    struct Reference
    {
        std::array<float, numBands> shapeDb {};
        bool ready = false;
    };

    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);

        for (auto& f : filters) f.prepare(sr, numChannels);

        // Several seconds: this is the singer's character, not the phrase.
        liveCoeff = 0.0f; // updated for each fresh spectrum using elapsed samples

        amount.reset(sr, 0.100);
        amount.setCurrentAndTargetValue(0.0f);
        reset();
    }

    void reset()
    {
        for (auto& f : filters) { f.reset(); f.setIdentity(); }
        live.fill(-40.0f);
        applied.fill(0.0f);
        liveSeen = false;
        capturing = false;
        captureFrames = 0;
        captureReadings = 0;
        capture.fill(0.0f);
        amount.setCurrentAndTargetValue(amount.getTargetValue());
    }

    void setAmount(float normalised) { amount.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }

    void useSpectrum(const float* magnitudes, int bins, double binHz, float scale, uint32_t frame, int hopSamples)
    {
        spectrum = magnitudes;
        spectrumBins = bins;
        spectrumBinHz = binHz;
        spectrumScale = scale;
        spectrumFrame = frame;
        spectrumHop = hopSamples;
    }

    void startCapture(float seconds)
    {
        capture.fill(0.0f);
        captureFrames = 0;
        captureReadings = 0;
        // Duration is measured in samples, independent of host block size.
        captureTarget = juce::jmax(1, static_cast<int>(seconds * sr));
        lastFrame = spectrumFrame;
        capturing = true;
    }

    bool isCapturing() const noexcept { return capturing; }
    float captureProgress() const noexcept
    {
        return captureTarget > 0 ? juce::jlimit(0.0f, 1.0f,
            static_cast<float>(captureFrames) / static_cast<float>(captureTarget)) : 0.0f;
    }

    bool hasReference() const noexcept { return reference.ready; }
    const Reference& getReference() const noexcept { return reference; }
    void setReference(const Reference& r) noexcept { reference = r; capturing = false; applied.fill(0.0f); }
    void clearReference() noexcept { setReference({}); }

    // How far the correction is currently reaching, for the editor to show.
    float appliedRangeDb() const noexcept { return rangeDb.load(std::memory_order_relaxed); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        const int count = buffer.getNumSamples();
        if (channels <= 0 || count == 0) return;

        const float wet = amount.skip(count);
        measure();

        if (capturing || !reference.ready || wet < 0.001f) {
            // Nothing to apply: leave the samples exactly as they arrived rather
            // than passing them through filters set to unity, which would still
            // cost the arithmetic and still not be bit-exact.
            for (auto& f : filters) f.setIdentity();
            rangeDb.store(0.0f, std::memory_order_relaxed);
            return;
        }

        float widest = 0.0f;
        for (int b = 0; b < numBands; ++b) {
            const float wanted = reference.shapeDb[static_cast<size_t>(b)]
                               - live[static_cast<size_t>(b)];
            const float target = juce::jlimit(-maxCorrectionDb, maxCorrectionDb, wanted) * wet;

            // Moved gradually even though it is recomputed per block, so that a
            // reference loaded mid-phrase arrives as a fade and not a step.
            auto& current = applied[static_cast<size_t>(b)];
            current += (1.0f - std::exp(-static_cast<float>(count / (sr * 0.2)))) * (target - current);
            filters[static_cast<size_t>(b)].setPeaking(bandHz(b), 1.1, current);
            widest = juce::jmax(widest, std::abs(current));
        }
        rangeDb.store(widest, std::memory_order_relaxed);

        for (int i = 0; i < count; ++i)
            for (int ch = 0; ch < channels; ++ch) {
                auto& sample = buffer.getWritePointer(ch)[i];
                for (auto& f : filters) sample = f.processSample(ch, sample);
            }
    }

private:
    static double bandHz(int index)
    {
        // 125 Hz upwards in octaves, which covers a voice from its fundamental
        // to the top of its air without wasting bands where nothing sings.
        return 125.0 * std::pow(2.0, static_cast<double>(index));
    }

    /*  One reading of the spectrum, reduced to eight band levels and normalised.

        Subtracting the mean is what makes this a match of shape rather than of
        loudness, and it is also what stops a quiet take from being turned up to
        meet a loud reference.
    */
    void measure()
    {
        if (spectrum == nullptr || spectrumBins <= 8 || spectrumBinHz <= 0.0) return;
        if (spectrumFrame == lastFrame) return;
        const auto frames = spectrumFrame > lastFrame ? spectrumFrame - lastFrame : 1u;
        lastFrame = spectrumFrame;
        const int elapsed = static_cast<int>(juce::jmin(frames, 1024u)) * spectrumHop;
        liveCoeff = 1.0f - std::exp(-static_cast<float>(elapsed / (4.0 * sr)));

        std::array<float, numBands> frame {};
        float mean = 0.0f;

        for (int b = 0; b < numBands; ++b) {
            const double centre = bandHz(b);
            const int first = juce::jmax(1, static_cast<int>(centre * 0.707 / spectrumBinHz));
            const int last = juce::jmin(spectrumBins - 1, static_cast<int>(centre * 1.414 / spectrumBinHz));
            if (last <= first) { frame[static_cast<size_t>(b)] = -80.0f; continue; }

            /*  Power summed across the band, then converted once.

                Averaging the decibels of each bin instead would be dominated by
                the empty ones: a band holding a single strong partial and forty
                bins of nothing measures as nearly silent that way, which is the
                opposite of what it contains. Energy is what a band holds, and
                energy adds as power.
            */
            double power = 0.0;
            for (int bin = first; bin <= last; ++bin) {
                const double magnitude = spectrum[bin] * spectrumScale;
                power += magnitude * magnitude;
            }
            frame[static_cast<size_t>(b)] = static_cast<float>(10.0 * std::log10(juce::jmax(1.0e-18, power)));
            mean += frame[static_cast<size_t>(b)];
        }
        mean /= static_cast<float>(numBands);

        // Silence has no shape worth learning from, and averaging it in would
        // drag both the reference and the live estimate towards the noise floor.
        if (mean < silenceDb) return;

        for (auto& v : frame) v -= mean;

        if (capturing) {
            for (int b = 0; b < numBands; ++b) capture[static_cast<size_t>(b)] += frame[static_cast<size_t>(b)];
            ++captureReadings;
            captureFrames += elapsed;
            if (captureFrames >= captureTarget) {
                for (int b = 0; b < numBands; ++b)
                    reference.shapeDb[static_cast<size_t>(b)] =
                        capture[static_cast<size_t>(b)] / static_cast<float>(captureReadings);
                reference.ready = true;
                capturing = false;
            }
            return;
        }

        if (!liveSeen) { live = frame; liveSeen = true; return; }
        for (int b = 0; b < numBands; ++b)
            live[static_cast<size_t>(b)] += liveCoeff * (frame[static_cast<size_t>(b)] - live[static_cast<size_t>(b)]);
    }

    // Wide enough to move a voice, narrow enough that a bad reference cannot
    // turn one into something unrecognisable.
    static constexpr float maxCorrectionDb = 6.0f;
    static constexpr float silenceDb = -70.0f;

    std::array<Biquad, numBands> filters;
    std::array<float, numBands> live {}, applied {}, capture {};
    Reference reference;
    std::atomic<float> rangeDb { 0.0f };

    juce::SmoothedValue<float> amount;
    const float* spectrum = nullptr;
    double sr = 48000.0, spectrumBinHz = 0.0;
    float spectrumScale = 1.0f, liveCoeff = 0.0f;
    int spectrumBins = 0, numChannels = 2;
    uint32_t spectrumFrame = 0, lastFrame = 0;
    int spectrumHop = 512;
    int captureFrames = 0, captureTarget = 0, captureReadings = 0;
    bool capturing = false, liveSeen = false;
};
}

#pragma once
#include <JuceHeader.h>
#include "Biquad.h"
#include <cmath>

namespace voxera
{
/*  Corrective EQ that follows the singer's own register.

    Every vocal chain cuts mud at a fixed frequency and high-passes at a fixed
    one, and both are guesses about a voice nobody measured. 250 Hz is where a
    baritone's boxiness lives; on a soprano it is her fundamental, and cutting it
    hollows her out. An 80 Hz high-pass is right for that baritone and leaves a
    tenor's worth of rumble untouched under a higher voice.

    This places both relative to what the pitch detector is actually hearing:
    the high-pass sits just under the lowest note the singer has been reaching,
    and the mud cut sits near the second harmonic of their median pitch, which is
    where "boxy" lives for that particular voice.

    The whole difficulty is in what it tracks. Following the melody note by note
    would sweep the filters audibly on every phrase — the classic way this idea
    fails. So it tracks the singer's tessitura instead: a very slow estimate,
    with a time constant measured in seconds, of where their voice sits overall.
    Frequencies are held in log space because pitch is perceived that way, and an
    average in hertz would be dragged around by the high notes.

    Unvoiced material is ignored rather than averaged in. Consonants and breaths
    have no fundamental, and letting the detector's zero readings into the
    estimate would drag the filters towards the bottom of the spectrum.
*/
class VocalLock
{
public:
    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);

        highPass.prepare(sr, numChannels);
        mudCut.prepare(sr, numChannels);

        // Per-block updates, so the coefficients are in samples of block time.
        medianCoeff = blockCoefficient(medianSeconds);
        floorCoeff = blockCoefficient(floorSeconds);

        amount.reset(sr, 0.050);
        amount.setCurrentAndTargetValue(0.0f);
        reset();
    }

    void reset()
    {
        highPass.reset();
        mudCut.reset();
        medianLog2 = std::log2(defaultHz);
        floorLog2 = std::log2(defaultHz);
        voicedSeen = false;
        applyCoefficients();
    }

    void setAmount(float normalised) { amount.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }

    /*  Fed once per block from the pitch engine's detector, which keeps running
        even in tracking mode, so this works there too.
    */
    void observePitch(float hz, float confidence) noexcept
    {
        if (hz < lowestHz || hz > highestHz || confidence < confidenceFloor) return;

        const float note = std::log2(hz);
        if (!voicedSeen) { medianLog2 = floorLog2 = note; voicedSeen = true; return; }

        medianLog2 += medianCoeff * (note - medianLog2);

        // The floor follows a low note immediately but drifts back up slowly,
        // so one deep note claims the bottom of the range and a long stretch of
        // high singing is needed before the high-pass is allowed to rise again.
        floorLog2 = note < floorLog2
            ? note
            : floorLog2 + floorCoeff * (note - floorLog2);
    }

    float highPassHz() const noexcept
    {
        return juce::jlimit(minHighPassHz, maxHighPassHz,
                            std::exp2(floorLog2) * highPassRatio);
    }

    float mudHz() const noexcept
    {
        return juce::jlimit(minMudHz, maxMudHz,
                            std::exp2(medianLog2) * mudRatio);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        const int count = buffer.getNumSamples();
        if (channels <= 0 || count == 0) return;

        const float wet = amount.skip(count);
        applyCoefficients();

        if (wet < 0.001f) return;   // exact identity when the control is down

        for (int i = 0; i < count; ++i)
            for (int ch = 0; ch < channels; ++ch) {
                auto& sample = buffer.getWritePointer(ch)[i];
                sample = mudCut.processSample(ch, highPass.processSample(ch, sample));
            }
    }

private:
    float blockCoefficient(double seconds) const
    {
        // Assumes a block rate around 100 Hz; the estimate is slow enough that
        // the exact figure does not matter, only its order.
        return 1.0f - std::exp(-1.0f / static_cast<float>(seconds * 100.0));
    }

    void applyCoefficients()
    {
        const float wet = amount.getCurrentValue();
        highPass.setHighPass(highPassHz(), 0.707);
        mudCut.setPeaking(mudHz(), 1.1, -mudDepthDb * wet);
    }

    static constexpr float lowestHz = 60.0f;      // outside this the detector is
    static constexpr float highestHz = 1200.0f;   // not reporting a sung note
    static constexpr float confidenceFloor = 0.6f;
    static constexpr float defaultHz = 180.0f;

    static constexpr double medianSeconds = 6.0;  // tessitura, not melody
    static constexpr double floorSeconds = 20.0;  // the bottom of the range drifts slower still

    static constexpr float highPassRatio = 0.80f; // just under the lowest note
    static constexpr float mudRatio = 2.2f;       // near the second harmonic
    static constexpr float mudDepthDb = 4.5f;

    static constexpr float minHighPassHz = 45.0f, maxHighPassHz = 220.0f;
    static constexpr float minMudHz = 140.0f, maxMudHz = 700.0f;

    Biquad highPass, mudCut;
    juce::SmoothedValue<float> amount;

    double sr = 48000.0;
    float medianLog2 = 0.0f, floorLog2 = 0.0f;
    float medianCoeff = 0.0f, floorCoeff = 0.0f;
    int numChannels = 2;
    bool voicedSeen = false;
};
}

#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace voxera
{
/*  Tempo-locked rhythmic gate.

    Position comes from the host's transport when it offers one, rather than
    from a phase this class advances itself. That distinction is the whole
    difference between a chop that sits on the grid and one that drifts: a
    free-running counter starts wherever the plugin happened to be initialised
    and slides against the beat for the length of the song. When the host gives
    nothing — an offline render, a host without a transport — it falls back to
    counting, which at least keeps the rhythm even if the origin is arbitrary.

    Edges are ramped over a couple of milliseconds. A gate that steps straight
    from open to closed puts a discontinuity in the waveform, which is heard as
    a click on every single step.
*/
class Chop
{
public:
    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);
        edgeCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sr * edgeSeconds));
        amount.reset(sr, 0.030);
        amount.setCurrentAndTargetValue(0.0f);
        reset();
    }

    void reset()
    {
        gain = 1.0f;
        freeRunningStep = 0.0;
        lastGiven = -1.0;
        amount.setCurrentAndTargetValue(amount.getTargetValue());
    }

    void setAmount(float normalised) { amount.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }
    void setTempo(double bpm) noexcept { tempo = juce::jlimit(20.0, 300.0, bpm); }
    void setDivision(int index) noexcept { division = juce::jlimit(0, numDivisions - 1, index); }
    void setPattern(int index) noexcept { pattern = juce::jlimit(0, numPatterns - 1, index); }

    /*  ppq below zero means the host offered no transport position.

        A repeated identical value is ignored rather than applied. The processor
        splits a host buffer into chunks and reads the playhead once per chunk,
        which returns the same position for all of them; assigning it each time
        would rewind the phase this class advanced during the previous chunk and
        replay the same window. It only shows up when the host's buffer is
        larger than the prepared block size, which is exactly the case a test on
        one block size never reaches.
    */
    void setPosition(double ppqPosition) noexcept
    {
        if (ppqPosition < 0.0) { ppq = -1.0; lastGiven = -1.0; return; }
        if (ppqPosition == lastGiven) return;
        lastGiven = ppqPosition;
        ppq = ppqPosition;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        const int count = buffer.getNumSamples();
        if (channels <= 0 || count == 0) return;

        // Quarter notes per step, so a step is this many beats long.
        const double beatsPerStep = divisionBeats[static_cast<size_t>(division)];
        const double stepsPerSample = tempo / (60.0 * sr * beatsPerStep);

        double step = ppq >= 0.0 ? ppq / beatsPerStep : freeRunningStep;

        for (int i = 0; i < count; ++i)
        {
            const float wet = amount.getNextValue();

            const auto index = static_cast<int>(std::floor(step)) & (patternLength - 1);
            const bool open = (patternMask[static_cast<size_t>(pattern)] >> index) & 1;

            // Closed steps fall to whatever depth the control asks for, so the
            // effect can be a subtle pulse as well as a hard gate.
            const float target = open ? 1.0f : (1.0f - wet);
            gain += edgeCoeff * (target - gain);

            for (int ch = 0; ch < channels; ++ch)
                buffer.getWritePointer(ch)[i] *= gain;

            step += stepsPerSample;
        }

        freeRunningStep = std::fmod(step, static_cast<double>(patternLength));
        if (ppq >= 0.0) ppq += static_cast<double>(count) * tempo / (60.0 * sr);
    }

    static constexpr int numDivisions = 4;
    static constexpr int numPatterns = 4;

private:
    static constexpr int patternLength = 16;
    static constexpr double edgeSeconds = 0.002;

    // 1/8, 1/16, 1/4, 1/8 triplet — in quarter notes per step.
    static constexpr std::array<double, numDivisions> divisionBeats { 0.5, 0.25, 1.0, 1.0 / 3.0 };

    /*  Sixteen steps, read from the low bit up. A set bit is an open step.
        Written as binary so the rhythm is legible in the source: whoever edits
        these should be able to see the pattern rather than decode a hex number.
    */
    static constexpr std::array<uint16_t, numPatterns> patternMask {
        0b1010101010101010,   // Alternate: every other step
        0b0101010101010101,   // Offbeat: the complement, lands off the grid
        0b1100110011001100,   // Stutter: pairs
        0b1110101011101010    // Broken: a longer phrase that does not repeat by 4
    };

    juce::SmoothedValue<float> amount;
    double sr = 48000.0, tempo = 120.0, ppq = -1.0, lastGiven = -1.0, freeRunningStep = 0.0;
    float gain = 1.0f, edgeCoeff = 1.0f;
    int numChannels = 2, division = 1, pattern = 0;
};

/*  Bit and sample-rate reduction.

    Both halves alias on purpose. Quantising to fewer bits folds the error back
    across the spectrum as harsh non-harmonic noise, and holding each sample for
    several periods mirrors everything above the reduced Nyquist back down. That
    grit is the effect being asked for, so unlike every other nonlinear stage
    here it must not be oversampled away.
*/
class Crush
{
public:
    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);
        amount.reset(sr, 0.030);
        mix.reset(sr, 0.030);
        amount.setCurrentAndTargetValue(0.0f);
        mix.setCurrentAndTargetValue(0.0f);
        reset();
    }

    void reset()
    {
        held.fill(0.0f);
        counter = 0.0f;
        amount.setCurrentAndTargetValue(amount.getTargetValue());
        mix.setCurrentAndTargetValue(mix.getTargetValue());
    }

    void setAmount(float normalised) { amount.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }
    void setMix(float normalised) { mix.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        if (channels <= 0 || buffer.getNumSamples() == 0) return;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float drive = amount.getNextValue();
            const float wet = mix.getNextValue();

            // Sixteen bits down to four, and full rate down to a sixteenth.
            const float levels = std::pow(2.0f, 16.0f - 12.0f * drive) - 1.0f;
            const float hold = 1.0f + 15.0f * drive;

            counter += 1.0f;
            const bool sampleNow = counter >= hold;
            if (sampleNow) counter -= hold;

            for (int ch = 0; ch < channels; ++ch)
            {
                auto& sample = buffer.getWritePointer(ch)[i];
                auto& previous = held[static_cast<size_t>(ch)];

                if (sampleNow)
                    previous = std::round(juce::jlimit(-1.0f, 1.0f, sample) * levels) / levels;

                sample += wet * (previous - sample);
            }
        }
    }

private:
    juce::SmoothedValue<float> amount, mix;
    std::array<float, 2> held {};
    double sr = 48000.0;
    float counter = 0.0f;
    int numChannels = 2;
};
}

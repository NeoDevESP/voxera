#pragma once
#include <JuceHeader.h>
#include <cmath>

namespace voxera
{
/*  Parallel ("New York") compression.

    Compressing a vocal this hard in series would flatten its diction: the
    consonant transients that carry intelligibility are exactly what a 10:1 ratio
    with a 2 ms attack removes. In parallel it does the opposite. The dry path
    keeps every transient intact, and the crushed copy underneath only fills the
    gaps between them, so the take reads as dense and close without losing its
    articulation.

    The settings are fixed rather than exposed. What makes this stage useful is
    that it is always far more aggressive than the serial compressor upstream;
    handing those controls over would mostly offer ways to make it a second,
    redundant version of that one.
*/
class Punch
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        capacity = juce::jmax(1, static_cast<int>(spec.maximumBlockSize));
        numChannels = juce::jlimit(1, 2, static_cast<int>(spec.numChannels));

        wet.setSize(numChannels, capacity);
        wet.clear();

        juce::dsp::ProcessSpec local { spec.sampleRate,
                                       static_cast<juce::uint32>(capacity),
                                       static_cast<juce::uint32>(numChannels) };
        compressor.prepare(local);
        compressor.setThreshold(threshold);
        compressor.setRatio(ratio);
        compressor.setAttack(attackMs);
        compressor.setRelease(releaseMs);
        compressor.reset();

        amount.reset(spec.sampleRate, 0.030);
        amount.setCurrentAndTargetValue(0.0f);
    }

    void reset()
    {
        compressor.reset();
        wet.clear();
        amount.setCurrentAndTargetValue(amount.getTargetValue());
    }

    void setAmount(float normalised) { amount.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        if (channels <= 0 || buffer.getNumSamples() == 0) return;

        // Hosts may exceed their advertised maximum block size.
        for (int offset = 0; offset < buffer.getNumSamples(); offset += capacity)
        {
            const int count = juce::jmin(capacity, buffer.getNumSamples() - offset);

            for (int ch = 0; ch < channels; ++ch)
                wet.copyFrom(ch, 0, buffer, ch, offset, count);

            juce::dsp::AudioBlock<float> block(wet);
            auto active = block.getSubsetChannelBlock(0, static_cast<size_t>(channels))
                               .getSubBlock(0, static_cast<size_t>(count));
            juce::dsp::ProcessContextReplacing<float> context(active);
            compressor.process(context);

            for (int i = 0; i < count; ++i)
            {
                const float blend = amount.getNextValue() * makeup;
                for (int ch = 0; ch < channels; ++ch)
                    buffer.getWritePointer(ch)[offset + i] += blend * wet.getReadPointer(ch)[i];
            }
        }
    }

private:
    static constexpr float threshold = -32.0f;
    static constexpr float ratio = 10.0f;
    static constexpr float attackMs = 2.0f;
    static constexpr float releaseMs = 140.0f;
    // Recovers roughly what the ratio takes off a vocal sitting near -18 dBFS,
    // so the blend control starts doing something useful straight away.
    static constexpr float makeup = 2.8f;

    juce::dsp::Compressor<float> compressor;
    juce::AudioBuffer<float> wet;
    juce::SmoothedValue<float> amount;
    int capacity = 1, numChannels = 2;
};
}

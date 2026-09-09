/*  Runs a real recording through the whole plugin and reports what happened.

    Every measurement in this project so far has been taken on a signal built to
    be measurable — sine tones, harmonic series, impulses. Those answer precise
    questions precisely, and they cannot answer the only question that finally
    matters, which is what the chain does to a voice. A held vowel is not a
    sine: it has vibrato, a noise floor, consonants, breaths, and a level that
    moves. Stages tuned entirely against synthetic material can be individually
    correct and collectively wrong on the first take anyone sings.

    So this loads a wav, drives the processor exactly as a host would, writes
    the result, and prints what each stage actually did on the way through. It
    is a measuring instrument, not part of the product.

        VOXERA_Render in.wav --preset Rage --out out.wav
        VOXERA_Render in.wav --set neuralMix=100 --set compMix=60
*/
#include "../Source/PluginProcessor.h"
#include <iostream>
#include <iomanip>

namespace
{
struct Running
{
    void add(float value) { total += value; peak = juce::jmax(peak, value); ++count; }
    float mean() const { return count > 0 ? total / static_cast<float>(count) : 0.0f; }
    float total = 0.0f, peak = 0.0f;
    int count = 0;
};

/*  Energy in three bands, by direct summation rather than a transform.

    The point is to describe a tilt, not to resolve a spectrum, and three
    one-pole splits do that at a hundredth of the code an FFT would need here.
*/
struct Tilt
{
    void prepare(double sampleRate)
    {
        lowCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 250.0f / static_cast<float>(sampleRate));
        highCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 4000.0f / static_cast<float>(sampleRate));
    }

    void add(float x)
    {
        lowState += lowCoeff * (x - lowState);
        highState += highCoeff * (x - highState);
        const float low = lowState;
        const float high = x - highState;
        const float mid = x - low - high;
        lowEnergy += low * low;
        midEnergy += mid * mid;
        highEnergy += high * high;
    }

    void report(const char* label) const
    {
        const double total = juce::jmax(1.0e-12, lowEnergy + midEnergy + highEnergy);
        std::cout << "  " << label << " balance   low " << std::setw(5)
                  << static_cast<int>(100.0 * lowEnergy / total) << "%   mid " << std::setw(5)
                  << static_cast<int>(100.0 * midEnergy / total) << "%   high " << std::setw(5)
                  << static_cast<int>(100.0 * highEnergy / total) << "%\n";
    }

    float lowCoeff = 0.0f, highCoeff = 0.0f, lowState = 0.0f, highState = 0.0f;
    double lowEnergy = 0.0, midEnergy = 0.0, highEnergy = 0.0;
};

float decibels(float gain) { return juce::Decibels::gainToDecibels(gain, -120.0f); }
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI lifetime;

    if (argc < 2) {
        std::cout << "usage: VOXERA_Render <input.wav> [--preset Name] [--set id=value]... [--out out.wav]\n";
        return 1;
    }

    const juce::File input(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
    if (!input.existsAsFile()) { std::cerr << "no such file: " << input.getFullPathName() << "\n"; return 1; }

    juce::File output;
    juce::String preset;
    bool autoMix = false;
    juce::String insert;
    bool listInsert = false;
    juce::StringPairArray insertSets;
    juce::StringPairArray overrides;

    for (int i = 2; i < argc; ++i) {
        const juce::String argument(argv[i]);
        if (argument == "--out" && i + 1 < argc)
            output = juce::File::getCurrentWorkingDirectory().getChildFile(argv[++i]);
        else if (argument == "--preset" && i + 1 < argc) preset = argv[++i];
        else if (argument == "--auto-mix") autoMix = true;
        else if (argument == "--insert" && i + 1 < argc) insert = argv[++i];
        else if (argument == "--insert-list") listInsert = true;
        else if (argument == "--insert-set" && i + 1 < argc) {
            const juce::String pair(argv[++i]);
            insertSets.set(pair.upToFirstOccurrenceOf("=", false, false),
                           pair.fromFirstOccurrenceOf("=", false, false));
        }
        else if (argument == "--set" && i + 1 < argc) {
            const juce::String pair(argv[++i]);
            overrides.set(pair.upToFirstOccurrenceOf("=", false, false),
                          pair.fromFirstOccurrenceOf("=", false, false));
        }
    }
    if (output == input) { std::cerr << "Output must differ from the original recording\n"; return 1; }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(input));
    if (reader == nullptr) { std::cerr << "not a readable audio file\n"; return 1; }

    const double rate = reader->sampleRate;
    const int channels = juce::jlimit(1, 2, static_cast<int>(reader->numChannels));
    const int length = static_cast<int>(reader->lengthInSamples);

    juce::AudioBuffer<float> dry(channels, length);
    reader->read(&dry, 0, length, 0, true, channels > 1);

    VoxeraAudioProcessor processor;
    if (preset.isNotEmpty()) {
        for (int i = 0; i < VoxeraAudioProcessor::numFactoryPresets; ++i)
            if (processor.getProgramName(i).equalsIgnoreCase(preset)) processor.applyFactoryPreset(i);
    }
    for (const auto& id : overrides.getAllKeys()) {
        if (auto* parameter = processor.apvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(overrides[id].getFloatValue()));
        else
            std::cerr << "unknown parameter: " << id << "\n";
    }

    constexpr int block = 128;
    processor.prepareToPlay(rate, block);

    /*  A plugin of somebody else's, in the insert slot.

        Loaded here rather than assumed to work: hosting is the one feature in
        this chain whose success depends on code nobody here wrote, so the only
        honest way to find out whether a given plugin loads is to load it.
    */
    if (insert.isNotEmpty()) {
        const juce::File file(insert);
        const auto loaded = processor.loadInsertPlugin(file);
        std::cout << "  insert: " << file.getFileName() << " -> " << loaded.message << std::endl;
        if (!loaded.ok) return 3;

        /*  Its controls, listed by the names it gives them.

            Printed rather than assumed. Nothing in the format tells a host
            which knob on a compressor is the one that decides how hard it
            works, so the only way to find out what this particular plugin
            calls things is to ask it and read the answer.
        */
        if (listInsert) {
            const auto names = processor.insertParameterNames();
            std::cout << "  " << names.size() << " controls:" << std::endl;
            for (int n = 0; n < names.size(); ++n)
                std::cout << "    [" << n << "] " << names[n]
                          << "  =  " << processor.insertParameterText(n) << std::endl;
        }

        // Set by name rather than by index, so a plugin update that reorders
        // its list does not silently move a different knob.
        for (const auto& key : insertSets.getAllKeys()) {
            const int index = processor.findInsertParameter({ key });
            if (index < 0) { std::cerr << "  no control matching: " << key << std::endl; continue; }
            processor.setInsertParameter(index, insertSets[key].getFloatValue());
            std::cout << "  set " << processor.insertParameterNames()[index]
                      << " -> " << processor.insertParameterText(index) << std::endl;
        }
    }
    if (autoMix) {
        processor.requestAutoMix();
        juce::MidiBuffer captureMidi;
        for (int at = 0; at < length; at += block) {
            const int count = juce::jmin(block, length - at);
            juce::AudioBuffer<float> capture(channels, count);
            for (int ch = 0; ch < channels; ++ch) capture.copyFrom(ch, 0, dry, ch, at, count);
            processor.processBlock(capture, captureMidi);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        std::cout << processor.autoMixReport() << "\n";
        processor.releaseResources(); processor.prepareToPlay(rate, block);
    }
    const int latency = processor.getLatencySamples();

    /*  The tail is rendered as well as the take.

        The chain reports a delay of its own, so stopping at the last input
        sample would cut exactly that much off the end of the result and quietly
        shorten every rendered file. Feeding silence for the length of the
        reported latency is what lets the output line up with the input.
    */
    juce::AudioBuffer<float> wet(channels, length + latency);
    wet.clear();
    for (int i = 0; i < length; ++i)
        for (int ch = 0; ch < channels; ++ch) wet.setSample(ch, i, dry.getSample(ch, i));

    Running limiter, gate, optical, compressor, lift, confidence;
    Running correction;
    Tilt inTilt, outTilt;
    inTilt.prepare(rate);
    outTilt.prepare(rate);

    for (int i = 0; i < length; ++i)
        for (int ch = 0; ch < channels; ++ch) inTilt.add(dry.getSample(ch, i));

    juce::MidiBuffer midi;
    for (int at = 0; at < wet.getNumSamples(); at += block) {
        const int piece = juce::jmin(block, wet.getNumSamples() - at);
        juce::AudioBuffer<float> slice(wet.getArrayOfWritePointers(), channels, at, piece);
        processor.processBlock(slice, midi);

        limiter.add(processor.limiterReductionDb());
        gate.add(processor.gateReductionDb());
        optical.add(processor.opticalReductionDb());
        compressor.add(processor.compressorReductionDb());
        lift.add(processor.densityLiftDb());
        confidence.add(processor.pitchConfidence());
        correction.add(std::abs(processor.shiftSemitones()));
    }

    for (int i = latency; i < wet.getNumSamples(); ++i)
        for (int ch = 0; ch < channels; ++ch) outTilt.add(wet.getSample(ch, i));

    const auto measure = [](const juce::AudioBuffer<float>& buffer, int from, int to) {
        double sum = 0.0; float peak = 0.0f; int count = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = from; i < to; ++i) {
                const float value = buffer.getSample(ch, i);
                sum += static_cast<double>(value) * value;
                peak = juce::jmax(peak, std::abs(value));
                ++count;
            }
        return std::make_pair(peak, static_cast<float>(std::sqrt(sum / juce::jmax(1, count))));
    };

    const auto [dryPeak, dryRms] = measure(dry, 0, length);
    const auto [wetPeak, wetRms] = measure(wet, latency, wet.getNumSamples());

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n" << input.getFileName() << "  " << rate << " Hz, " << channels
              << " ch, " << (length / rate) << " s"
              << (preset.isNotEmpty() ? ("  preset " + preset) : juce::String()) << "\n";
    std::cout << "  reported latency  " << latency << " samples ("
              << (1000.0 * latency / rate) << " ms)\n\n";

    std::cout << "  peak      " << decibels(dryPeak) << " dB  ->  " << decibels(wetPeak) << " dB\n";
    std::cout << "  rms       " << decibels(dryRms) << " dB  ->  " << decibels(wetRms) << " dB\n";
    // Crest is what a compressor actually changes; level alone hides it.
    std::cout << "  crest     " << (decibels(dryPeak) - decibels(dryRms)) << " dB  ->  "
              << (decibels(wetPeak) - decibels(wetRms)) << " dB\n\n";

    inTilt.report("in ");
    outTilt.report("out");

    std::cout << "\n  worst reduction, and average while running:\n";
    std::cout << "    gate        " << gate.peak << " dB   (mean " << gate.mean() << ")\n";
    std::cout << "    compressor  " << compressor.peak << " dB   (mean " << compressor.mean() << ")\n";
    std::cout << "    optical     " << optical.peak << " dB   (mean " << optical.mean() << ")\n";
    std::cout << "    limiter     " << limiter.peak << " dB   (mean " << limiter.mean() << ")\n";
    std::cout << "    upward lift " << lift.peak << " dB   (mean " << lift.mean() << ")\n";
    std::cout << "\n  tuning: voiced " << (100.0f * confidence.mean()) << "% of the take, "
              << "moved " << correction.mean() << " semitones on average, "
              << correction.peak << " at most\n";

    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < wet.getNumSamples(); ++i)
            if (!std::isfinite(wet.getSample(ch, i))) { std::cerr << "\nNON-FINITE OUTPUT at " << i << "\n"; return 2; }

    if (output != juce::File()) {
        output.deleteFile();
        if (auto stream = std::unique_ptr<juce::FileOutputStream>(output.createOutputStream())) {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wav.createWriterFor(stream.get(), rate, static_cast<unsigned>(channels), 24, {}, 0));
            if (writer != nullptr) {
                stream.release();
                // Written from the latency offset, so the file lines up with the
                // original and the two can be compared without nudging one.
                juce::AudioBuffer<float> aligned(channels, wet.getNumSamples() - latency);
                for (int ch = 0; ch < channels; ++ch)
                    for (int i = 0; i < aligned.getNumSamples(); ++i)
                        aligned.setSample(ch, i, wet.getSample(ch, i + latency));
                writer->writeFromAudioSampleBuffer(aligned, 0, aligned.getNumSamples());
                writer.reset();
                std::cout << "\n  written to " << output.getFullPathName() << "\n";
            }
        }
    }

    std::cout << std::endl;
    return 0;
}

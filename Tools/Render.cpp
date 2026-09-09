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

    void add(float x, bool measure = true)
    {
        lowState += lowCoeff * (x - lowState);
        highState += highCoeff * (x - highState);
        const float low = lowState;
        const float high = x - highState;
        const float mid = x - low - high;
        if (!measure) return;
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
    bool audit = false;
    juce::File compareWith;
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
        else if (argument == "--audit") audit = true;
        else if (argument == "--compare" && i + 1 < argc)
            compareWith = juce::File::getCurrentWorkingDirectory().getChildFile(argv[++i]);
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

    /*  Two finished files, side by side, with no processing in between.

        "It sounds better" is not something anyone can act on. What can be acted
        on is which band holds more energy, how much dynamic range each one
        kept, and how loud they actually are — and those are three numbers that
        turn an impression into somewhere to look.

        Deliberately no attempt to align or time-stretch. Two renders of the
        same take start at the same sample or they do not, and pretending
        otherwise would produce confident numbers about the wrong thing.
    */
    /*  Does every control actually do something?

        The exciter looked broken because it was: moving it end to end changed
        the signal by 0.16 dB, which is not a subtle effect but no effect. It
        had passed its own unit test the whole time, on a synthetic signal
        built to make it pass.

        So rather than trust that once, this asks the question of every
        parameter at once. Each one is moved to an extreme, the take is
        rendered again, and the result is compared against the untouched
        render. A control that changes nothing measurable is either broken,
        unreachable, or does nothing on this material — and all three are worth
        knowing before somebody reaches for it.

        Reported and never asserted on. Plenty of controls legitimately do
        nothing on a given take: a de-esser has no work on a take with no
        sibilance, and a key setting does nothing while tuning is off. The
        output is a list to read, not a verdict.
    */
    if (audit) {
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(input));
        if (reader == nullptr) { std::cerr << "not a readable audio file" << std::endl; return 1; }

        const double rate = reader->sampleRate;
        const int channels = juce::jlimit(1, 2, static_cast<int>(reader->numChannels));
        /*  The loudest few seconds, not the first few.

            A few seconds is plenty and keeps a sweep of seventy parameters to
            something a person will wait for — but taking them from the start of
            the file is what this tool got wrong on its first run. Recordings
            begin with silence, and over silence nothing changes anything, so it
            reported a working limiter and a working modulation as doing
            nothing at all. A tool that answers "does this control do something"
            has to be pointed at a moment where there is something to do it to.
        */
        const int whole = static_cast<int>(reader->lengthInSamples);
        const int length = juce::jmin(whole, static_cast<int>(rate * 6.0));
        juce::AudioBuffer<float> everything(channels, whole);
        reader->read(&everything, 0, whole, 0, true, channels > 1);

        int loudestAt = 0;
        double loudest = -1.0;
        for (int at = 0; at + length <= whole; at += juce::jmax(1, length / 4)) {
            double sum = 0.0;
            for (int ch = 0; ch < channels; ++ch)
                for (int i = at; i < at + length; i += 16) {
                    const double v = everything.getSample(ch, i);
                    sum += v * v;
                }
            if (sum > loudest) { loudest = sum; loudestAt = at; }
        }

        juce::AudioBuffer<float> source(channels, length);
        for (int ch = 0; ch < channels; ++ch)
            source.copyFrom(ch, 0, everything, ch, loudestAt, length);

        constexpr int block = 128;
        juce::MidiBuffer midi;

        const auto renderWith = [&](const juce::String& id, float value) {
            VoxeraAudioProcessor p;
            if (id.isNotEmpty())
                if (auto* parameter = p.apvts.getParameter(id))
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            p.prepareToPlay(rate, block);
            juce::AudioBuffer<float> work(channels, length);
            work.makeCopyOf(source);
            for (int at = 0; at < length; at += block) {
                const int piece = juce::jmin(block, length - at);
                juce::AudioBuffer<float> slice(work.getArrayOfWritePointers(), channels, at, piece);
                p.processBlock(slice, midi);
            }
            return work;
        };

        const auto reference = renderWith({}, 0.0f);
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::endl << "Sweeping every control on " << input.getFileName()
                  << " (" << (length / rate) << " s from " << (loudestAt / rate)
                  << " s, the loudest stretch)" << std::endl << std::endl;

        VoxeraAudioProcessor probe;
        juce::StringArray silent;

        for (auto* raw : probe.getParameters()) {
            auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(raw);
            if (parameter == nullptr) continue;
            const auto id = parameter->paramID;
            // Modes and actions rather than sound controls, and the one that
            // starts an eight-second listen if you touch it.
            if (id == "bypass" || id == "analyzeVoice" || id == "autoVoice"
                || id == "levelMatch" || id == "lowLatency") continue;

            const auto range = parameter->getNormalisableRange();
            const float here = parameter->convertFrom0to1(parameter->getValue());
            // Whichever end is further from where it sits now, so a control
            // already at one extreme is still moved.
            const float target = std::abs(range.end - here) > std::abs(here - range.start)
                               ? range.end : range.start;

            const auto moved = renderWith(id, target);
            float widest = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < length; ++i)
                    widest = juce::jmax(widest, std::abs(moved.getSample(ch, i)
                                                       - reference.getSample(ch, i)));

            /*  Why a control was expected to do nothing, where that is known.

                Most silent controls here are silent for a good reason: they
                depend on something that is switched off, or they act only while
                Auto Mix runs. Printing the list without saying so leaves
                whoever reads it to work out fifteen explanations, and the one
                entry that matters gets lost among the fourteen that do not.
            */
            const auto expected = [&]() -> juce::String {
                if (id == "pitchKey" || id == "pitchScale")
                    return "chromatic scale ignores the key";
                if (id.startsWith("chop") && id != "chopAmount")  return "chop amount is zero";
                if (id == "crushMix")                              return "crush is zero";
                if (id.startsWith("mod") && id != "modType")       return "mod type is Off";
                if (id == "modType")                               return "mod mix is zero";
                if (id == "neuralMix")                             return "no capture loaded";
                if (id == "voiceMatch")                            return "no reference learned";
                if (id.startsWith("autoMix"))                      return "only acts while Auto Mix runs";
                if (id == "limiterOn" || id == "limiterCeiling")
                    return "nothing reaches the ceiling on this take";
                return {};
            }();

            const float db = juce::Decibels::gainToDecibels(widest, -120.0f);
            std::cout << "  " << id.paddedRight(' ', 18)
                      << juce::String(here, 1).paddedLeft(' ', 8) << " -> "
                      << juce::String(target, 1).paddedLeft(' ', 8)
                      << "   difference " << juce::String(db, 1).paddedLeft(' ', 7) << " dB"
                      << (db < -80.0f ? (expected.isEmpty() ? "   NOTHING"
                                                             : "   nothing, expected: " + expected)
                                       : juce::String()) << std::endl;
            // Only the unexplained ones are worth carrying to the summary.
            if (db < -80.0f && expected.isEmpty()) silent.add(id);
        }

        std::cout << std::endl << silent.size() << " controls changed nothing without a reason";
        if (!silent.isEmpty()) std::cout << ": " << silent.joinIntoString(", ");
        std::cout << std::endl << std::endl;
        return 0;
    }

    if (compareWith != juce::File()) {
        std::unique_ptr<juce::AudioFormatReader> a(formats.createReaderFor(input));
        std::unique_ptr<juce::AudioFormatReader> b(formats.createReaderFor(compareWith));
        if (a == nullptr || b == nullptr) { std::cerr << "could not read both files" << std::endl; return 1; }

        if (a->sampleRate != b->sampleRate) {
            std::cerr << "Comparison requires matching sample rates; resample both files first.\n"; return 1;
        }
        const int shortest = static_cast<int>(juce::jmin(a->lengthInSamples, b->lengthInSamples));
        const double rateA = a->sampleRate;

        juce::AudioBuffer<float> audioA(int(a->numChannels), shortest), audioB(int(b->numChannels), shortest);
        a->read(&audioA, 0, shortest, 0, true, audioA.getNumChannels()>1);
        b->read(&audioB, 0, shortest, 0, true, audioB.getNumChannels()>1);
        const int window = juce::jmax(1, int(rateA * 0.020));
        std::vector<bool> active(size_t((shortest+window-1)/window), false);
        int activeSamples=0;
        for (int at=0;at<shortest;at+=window) {
            const int n=juce::jmin(window,shortest-at);
            float energy=0;
            for (const auto* buffer : {&audioA,&audioB})
                for(int ch=0;ch<buffer->getNumChannels();++ch)
                    energy=juce::jmax(energy,buffer->getRMSLevel(ch,at,n));
            const bool voiced=energy>juce::Decibels::decibelsToGain(-55.0f);
            active[size_t(at/window)]=voiced;
            if(voiced) activeSamples+=n;
        }
        if(activeSamples==0) { std::cerr << "No active audio above -55 dBFS\n"; return 1; }
        const auto study = [&](const juce::AudioBuffer<float>& buffer) {
            Tilt tilt; tilt.prepare(rateA);
            double sum=0; float peak=0; int count=0;
            for(int ch=0;ch<buffer.getNumChannels();++ch) {
                tilt.lowState=tilt.highState=0;
                for(int i=0;i<shortest;++i) {
                    const float v=buffer.getSample(ch,i); const bool measure=active[size_t(i/window)];
                    tilt.add(v,measure);
                    if(measure) { sum+=double(v)*v; peak=juce::jmax(peak,std::abs(v)); ++count; }
                }
            }
            return std::make_tuple(peak,float(std::sqrt(sum/juce::jmax(1,count))),tilt);
        };
        const auto [peakA,rmsA,tiltA]=study(audioA);
        const auto [peakB,rmsB,tiltB]=study(audioB);

        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::endl << "A  " << input.getFileName() << std::endl
                  << "B  " << compareWith.getFileName() << std::endl
                  << "   comparing " << (shortest / rateA) << " s" << std::endl << std::endl;
        std::cout << "  peak    A " << decibels(peakA) << "   B " << decibels(peakB)
                  << "   difference " << (decibels(peakB) - decibels(peakA)) << " dB" << std::endl;
        std::cout << "  rms     A " << decibels(rmsA) << "   B " << decibels(rmsB)
                  << "   difference " << (decibels(rmsB) - decibels(rmsA)) << " dB" << std::endl;
        // Crest is the one that says which is squashed harder, and level alone
        // hides it completely.
        std::cout << "  crest   A " << (decibels(peakA) - decibels(rmsA))
                  << "   B " << (decibels(peakB) - decibels(rmsB))
                  << "   difference " << ((decibels(peakB) - decibels(rmsB)) - (decibels(peakA) - decibels(rmsA)))
                  << " dB" << std::endl << std::endl;
        std::cout << "  shared active audio: " << activeSamples/rateA << " s; RMS trim for B to match A: "
                  << decibels(rmsA)-decibels(rmsB) << " dB (not LUFS)\n";
        tiltA.report("A");
        tiltB.report("B");
        std::cout << std::endl;
        return 0;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(input));
    if (reader == nullptr) { std::cerr << "not a readable audio file\n"; return 1; }

    const double rate = reader->sampleRate;
    const int channels = juce::jlimit(1, 2, static_cast<int>(reader->numChannels));
    const int length = static_cast<int>(reader->lengthInSamples);

    juce::AudioBuffer<float> dry(channels, length);
    reader->read(&dry, 0, length, 0, true, channels > 1);

    VoxeraAudioProcessor processor;
    processor.setNonRealtime(true);
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

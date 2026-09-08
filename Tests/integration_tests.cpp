#include "../Source/PluginProcessor.h"
#include "Checks.h"
#include <chrono>
#include <iostream>

void set(VoxeraAudioProcessor& p, const char* id, float value) {
    auto* parameter = p.apvts.getParameter(id); CHECK(parameter);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

/*  Cost of the whole chain with every stage doing real work.

    The figure that matters to someone loading this on eight vocal tracks is not
    milliseconds but the realtime factor: how many seconds of audio one second of
    CPU can process. The assertion is deliberately loose, because a CI runner is
    not the machine anyone mixes on and a tight bound would fail for reasons that
    have nothing to do with this code. What it does catch is an order-of-magnitude
    regression, which is the kind that matters.
*/
double measureCost(double sr, int blockSize, bool tracking)
{
    juce::MidiBuffer midi;
    VoxeraAudioProcessor p;

    // Everything on, and nothing left at rest.
    for (const auto* id : { "pitchOn", "spectralOn", "spatialOn", "gateOn", "limiterOn" })
        set(p, id, 1.0f);
    for (const auto* id : { "punch", "exciter", "optical", "density", "clipAmount",
                            "vocalLock", "smartEQAmount", "satWarmth", "autoVoice" })
        set(p, id, 80.0f);
    set(p, "satMix", 60.0f);
    set(p, "space", 50.0f);
    set(p, "character", 1.0f);
    set(p, "lowLatency", tracking ? 1.0f : 0.0f);

    p.prepareToPlay(sr, blockSize);

    const int blocks = static_cast<int>(sr * 10.0 / blockSize);   // ten seconds
    juce::AudioBuffer<float> b(2, blockSize);

    // A sung note rather than silence: gates, detectors and compressors all cost
    // differently depending on whether they think anything is happening.
    int phase = 0;
    const auto fill = [&] {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                b.setSample(ch, i, 0.25f * std::sin(juce::MathConstants<float>::twoPi
                                                    * 220.0f * static_cast<float>(phase + i) / static_cast<float>(sr)));
        phase += blockSize;
    };

    fill(); p.processBlock(b, midi);   // one block outside the timing, to settle

    const auto start = std::chrono::steady_clock::now();
    for (int n = 0; n < blocks; ++n) { fill(); p.processBlock(b, midi); }
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    const double audioSeconds = static_cast<double>(blocks * blockSize) / sr;
    const double realtimeFactor = audioSeconds / elapsed;

    std::cout << "COST " << sr << " Hz, block " << blockSize
              << (tracking ? ", shifter bypassed: " : ", full chain:      ")
              << realtimeFactor << "x realtime ("
              << (100.0 / realtimeFactor) << "% of one core, about "
              << static_cast<int>(realtimeFactor) << " instances)\n";

    // Loose on purpose: a CI runner is not a mixing machine, and a tight bound
    // would fail for reasons unrelated to this code. What it catches is an
    // order-of-magnitude regression.
    CHECK(realtimeFactor > 1.5);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i) CHECK(std::isfinite(b.getSample(ch, i)));
    return realtimeFactor;
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::MidiBuffer midi;
    for (double sr : {44100.0, 48000.0, 96000.0}) for (int channels : {1, 2}) {
        VoxeraAudioProcessor p;
        auto layout = p.getBusesLayout();
        layout.inputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        layout.outputBuses.set(0, layout.inputBuses[0]); CHECK(p.setBusesLayout(layout));
        set(p, "smartEQAmount", 75);
        p.prepareToPlay(sr, 128);
        for (int size : {0, 1, 64, 257, 2048, 17, 4096}) {
            juce::AudioBuffer<float> b(channels, size);
            for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < size; ++i)
                b.setSample(ch, i, 0.1f * std::sin(static_cast<float>(2.0 * 3.141592653589793 * 220.0 * i / sr)));
            p.processBlock(b, midi);
            for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < size; ++i)
                CHECK(std::isfinite(b.getSample(ch, i)));
        }
        p.releaseResources(); set(p, "bypass", 1); p.prepareToPlay(sr, 128);
        const int n = p.getLatencySamples() + 2048;
        juce::AudioBuffer<float> impulse(channels, n); impulse.clear();
        for (int ch = 0; ch < channels; ++ch) impulse.setSample(ch, 0, 0.25f);
        p.processBlockBypassed(impulse, midi);
        for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < n; ++i)
            CHECK(std::abs(impulse.getSample(ch, i) - (i == p.getLatencySamples() ? 0.25f : 0.0f)) < 1.0e-6f);
        p.releaseResources(); set(p, "bypass", 0); set(p, "globalMix", 0); p.prepareToPlay(sr, 128);
        impulse.clear(); for (int ch = 0; ch < channels; ++ch) impulse.setSample(ch, 0, 0.25f);
        p.processBlock(impulse, midi);
        for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < n; ++i)
            CHECK(std::abs(impulse.getSample(ch, i) - (i == p.getLatencySamples() ? 0.25f : 0.0f)) < 1.0e-6f);
        const int mixLatency = p.getLatencySamples();

        /*  Tracking mode has to earn its name: the reported latency must
            actually collapse, the host must be told, and the dry path must stay
            aligned to the new figure — an impulse arriving anywhere other than
            the reported sample would mean the compensation delay and the chain
            disagree, which is heard as comb filtering rather than as delay.
        */
        p.releaseResources(); set(p, "bypass", 1); set(p, "lowLatency", 1);
        p.prepareToPlay(sr, 128);
        const int trackingLatency = p.getLatencySamples();
        CHECK(trackingLatency < mixLatency / 8);
        CHECK(trackingLatency < static_cast<int>(sr * 0.006));   // under 6 ms
        CHECK(p.reportedLatencySamples() == trackingLatency);

        juce::AudioBuffer<float> tracked(channels, trackingLatency + 2048); tracked.clear();
        for (int ch = 0; ch < channels; ++ch) tracked.setSample(ch, 0, 0.25f);
        p.processBlockBypassed(tracked, midi);
        for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < tracked.getNumSamples(); ++i)
            CHECK(std::abs(tracked.getSample(ch, i) - (i == trackingLatency ? 0.25f : 0.0f)) < 1.0e-6f);

        // Switching back mid-stream must restore the full figure without a
        // reallocation path or a stale delay offset.
        set(p, "lowLatency", 0);
        juce::AudioBuffer<float> settle(channels, 512); settle.clear();
        p.processBlock(settle, midi);
        CHECK(p.reportedLatencySamples() == mixLatency);
        set(p, "bypass", 0);

        std::cout << "PASS chain " << sr << " Hz " << channels << " ch; dry/bypass delay "
                  << mixLatency << ", tracking " << trackingLatency << " ("
                  << (1000.0 * trackingLatency / sr) << " ms)\n";
    }
    // Measured both ways at each rate, because the difference between them is
    // the pitch shifter's share of the bill and nothing else changes.
    /*  Neural stage: a synthetic capture with known weights.

        There is no .nam file to hand and none can be bundled, so the format is
        exercised by writing one. The weights are chosen so the answer can be
        worked out independently: with every gate weight zero and only the
        biases set, the gates take fixed values regardless of the input, which
        makes one LSTM step short enough to compute by hand and compare against.

        This is the part of the loader worth testing. Getting the split points
        of that flat weight array wrong produces a model that loads, runs, and
        sounds like nothing in particular — a failure no crash would reveal.
    */
    {
        const auto scratch = juce::File(argc > 1 ? argv[1] : "voxera-neural-test");
        scratch.createDirectory();

        const int hidden = 2;
        // Order: W (1 x 4H), U (H x 4H), gate biases (4H), head (H), head bias.
        std::vector<double> weights;
        for (int i = 0; i < 4 * hidden; ++i) weights.push_back(0.0);              // W
        for (int i = 0; i < hidden * 4 * hidden; ++i) weights.push_back(0.0);     // U
        // Gates i, f, g, o. Large positive g and o, so tanh and sigmoid saturate
        // towards one and the cell fills on the first step.
        const double gateBias[4] { 4.0, -4.0, 4.0, 4.0 };
        for (int g = 0; g < 4; ++g)
            for (int h = 0; h < hidden; ++h) weights.push_back(gateBias[g]);
        for (int h = 0; h < hidden; ++h) weights.push_back(0.5);                  // head
        weights.push_back(0.25);                                                  // head bias

        juce::String json = "{\"architecture\":\"LSTM\",\"config\":{\"num_layers\":1,"
                            "\"input_size\":1,\"hidden_size\":" + juce::String(hidden)
                          + "},\"sample_rate\":48000,\"weights\":[";
        for (size_t i = 0; i < weights.size(); ++i)
            json += (i ? "," : "") + juce::String(weights[i], 6);
        json += "]}";

        const auto file = scratch.getChildFile("synthetic.nam");
        file.deleteFile();
        CHECK(file.replaceWithText(json));

        VoxeraAudioProcessor n;
        const auto loaded = n.loadNeuralModel(file);
        std::cout << "NEURAL load: " << loaded.message << "\n";
        CHECK(loaded.ok);
        CHECK(n.hasNeuralModel());

        set(n, "neuralMix", 100.0f);
        for (const auto* id : { "pitchOn", "spectralOn", "spatialOn", "gateOn", "limiterOn" })
            set(n, id, 0.0f);
        set(n, "autoGain", 0.0f);
        set(n, "satMix", 0.0f);
        set(n, "smartEQAmount", 0.0f);
        n.prepareToPlay(48000.0, 256);

        juce::AudioBuffer<float> b(2, 256);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) b.setSample(ch, i, 0.1f);
        for (int block = 0; block < 40; ++block) n.processBlock(b, midi);

        /*  Every gate is saturated, so after many steps each hidden unit sits at
            tanh(1) and the head gives H * 0.5 * tanh(1) + 0.25.
        */
        const double expected = hidden * 0.5 * std::tanh(1.0) + 0.25;
        const double got = b.getSample(0, 255);
        std::cout << "NEURAL output: " << got << " expected about " << expected << "\n";
        CHECK(std::abs(got - expected) < 0.05);

        /*  The same capture in a session at a different rate.

            A capture is resampled so the network keeps running at the rate it
            learned at. What that has to not do is drift: asking for a rounded
            number of model-rate samples every block loses a fraction of a
            sample each time, and over minutes that becomes audible. Running a
            long stretch and checking the tail is still steady is what catches
            it — a drifting resampler shows up as the level wandering, not as an
            obvious fault in the first few blocks.
        */
        {
            VoxeraAudioProcessor r;
            CHECK(r.loadNeuralModel(file).ok);
            set(r, "neuralMix", 100.0f);
            for (const auto* id : { "pitchOn", "spectralOn", "spatialOn", "gateOn", "limiterOn" })
                set(r, id, 0.0f);
            set(r, "autoGain", 0.0f); set(r, "satMix", 0.0f); set(r, "smartEQAmount", 0.0f);
            r.prepareToPlay(44100.0, 256);   // model is 48 kHz, session is not

            juce::AudioBuffer<float> rb(2, 256);
            float earliest = 0.0f, latest = 0.0f;
            for (int block = 0; block < 900; ++block) {   // about five seconds
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 256; ++i) rb.setSample(ch, i, 0.1f);
                r.processBlock(rb, midi);
                for (int i = 0; i < 256; ++i) CHECK(std::isfinite(rb.getSample(0, i)));
                if (block == 100) earliest = rb.getSample(0, 200);
                if (block == 899) latest = rb.getSample(0, 200);
            }
            std::cout << "NEURAL resampled 44.1k: " << earliest << " early, " << latest << " late\n";
            // Steady input, steady output: any wander here is the resampler
            // losing its place against the clock.
            CHECK(std::abs(latest - earliest) < 0.02f);
        }

        // A WaveNet capture has to be refused clearly rather than half-loaded.
        const auto wave = scratch.getChildFile("wavenet.nam");
        wave.deleteFile();
        CHECK(wave.replaceWithText("{\"architecture\":\"WaveNet\",\"config\":{},\"weights\":[0]}"));
        const auto refused = n.loadNeuralModel(wave);
        CHECK(!refused.ok);
        CHECK(refused.message.containsIgnoreCase("LSTM"));
        std::cout << "NEURAL refusal: " << refused.message << "\n";
        n.unloadNeuralModel();
        CHECK(!n.hasNeuralModel());
    }

    /*  Even harmonics out of the whole chain, with a factory preset loaded.

        This is the measurement that answers whether the plugin sounds valve-like
        rather than merely contains a stage that could. Everything upstream is
        symmetric — tanh, the compressors, the filters — so a second harmonic in
        the output can only have come from the biased shaper. If a preset leaves
        that control at zero the chain is incapable of producing one, which is
        exactly the state this plugin shipped in until now.
    */
    {
        const auto secondHarmonic = [&](int preset) {
            VoxeraAudioProcessor p;
            p.applyFactoryPreset(preset);
            for (const auto* id : { "pitchOn", "spatialOn", "gateOn", "spectralOn" })
                set(p, id, 0.0f);
            set(p, "smartEQAmount", 0.0f);
            set(p, "exciter", 0.0f);      // its own even harmonics live above 7 kHz
            set(p, "chopAmount", 0.0f);
            set(p, "crush", 0.0f);
            p.prepareToPlay(48000.0, 512);

            const int total = 48000;
            juce::AudioBuffer<float> acc(1, total);
            int written = 0;
            while (written < total) {
                juce::AudioBuffer<float> b(2, 512);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i) {
                        const auto t = static_cast<float>(written + i) / 48000.0f;
                        b.setSample(ch, i, 0.35f * std::sin(juce::MathConstants<float>::twoPi * 400.0f * t));
                    }
                p.processBlock(b, midi);
                const int copy = juce::jmin(512, total - written);
                acc.copyFrom(0, written, b, 0, 0, copy);
                written += copy;
            }

            // Goertzel over the settled second half, so the smoothed controls
            // have arrived and the compressors are no longer moving.
            const auto tone = [&acc, total](double hz) {
                const int start = total / 2, n = total - start;
                const double w = juce::MathConstants<double>::twoPi * hz / 48000.0;
                const double coeff = 2.0 * std::cos(w);
                double s1 = 0.0, s2 = 0.0, windowSum = 0.0;
                for (int i = 0; i < n; ++i) {
                    const double hann = 0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * i / (n - 1));
                    windowSum += hann;
                    const double s0 = hann * acc.getSample(0, start + i) + coeff * s1 - s2;
                    s2 = s1; s1 = s0;
                }
                const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w);
                return 2.0 * std::sqrt(re * re + im * im) / juce::jmax(1.0, windowSum);
            };
            return std::make_pair(tone(800.0), tone(400.0));   // second, fundamental
        };

        static const char* names[] { "Clean", "Warm", "Modern", "Dream", "Radio" };
        for (int preset = 0; preset < VoxeraAudioProcessor::numFactoryPresets; ++preset) {
            const auto [second, fundamental] = secondHarmonic(preset);
            const double ratio = second / juce::jmax(1.0e-9, fundamental);
            std::cout << "ANALOGUE " << names[preset] << ": 2nd harmonic "
                      << (100.0 * ratio) << "% of the fundamental\n";
            CHECK(std::isfinite(second) && fundamental > 1.0e-4);
            // A tenth of a percent is far below audibility; the point of the
            // check is that it is not zero, which is what a missing control
            // would give.
            CHECK(ratio > 0.001);
        }
    }

    /*  Smart EQ inside the whole chain, not on its own.

        Its unit test passes, so if it seems to do nothing in use the cause is
        upstream: Vocal Lock and the spectral engine both remove low-mid
        emphasis, and what they take out is no longer there for this stage to
        find. Measuring it alone and then again with those running is what
        separates "broken" from "already handled".
    */
    {
        const auto resonantCut = [&](bool upstreamActive) {
            VoxeraAudioProcessor p;
            set(p, "smartEQAmount", 100.0f);
            set(p, "smartEQRange", 6.0f);
            set(p, "smartEQResponse", 100.0f);
            set(p, "pitchOn", 0.0f);
            set(p, "spectralOn", upstreamActive ? 1.0f : 0.0f);
            set(p, "vocalLock", upstreamActive ? 80.0f : 0.0f);
            set(p, "gateOn", 0.0f);
            p.prepareToPlay(48000.0, 512);

            // Two seconds of a voice-like tone with a heavy 1 kHz resonance on
            // top, which is exactly what this stage exists to find.
            for (int block = 0; block < 188; ++block) {
                juce::AudioBuffer<float> b(2, 512);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i) {
                        const auto t = static_cast<float>(block * 512 + i) / 48000.0f;
                        b.setSample(ch, i, 0.12f * std::sin(juce::MathConstants<float>::twoPi * 220.0f * t)
                                         + 0.35f * std::sin(juce::MathConstants<float>::twoPi * 1000.0f * t));
                    }
                p.processBlock(b, midi);
            }
            // Report every band, not just the one under test: the budget is
            // shared, so a smaller figure on 1 kHz can mean the detector found
            // something else worth cutting rather than that it went deaf.
            std::cout << "   bands:";
            float total = 0.0f;
            for (size_t band = 0; band < 5; ++band) {
                std::cout << " " << juce::String(p.smartEQGain(band), 2).toStdString();
                total -= p.smartEQGain(band);
            }
            std::cout << "   total " << total << " dB\n";
            return p.smartEQGain(2);   // the 1 kHz band
        };

        std::cout << "SMART EQ, alone:\n";
        const float alone = resonantCut(false);
        std::cout << "SMART EQ, with Vocal Lock and spectral engine:\n";
        const float withUpstream = resonantCut(true);

        /*  A guard against the stage going quietly useless again. It once cut
            0.74 dB here with the budget set to six, which measured as working
            and was inaudible. Anything under two decibels on a resonance this
            blatant means the detector has stopped separating bands.
        */
        CHECK(alone < -2.0f);
        CHECK(withUpstream < -2.0f);
    }

    for (const auto rate : { 48000.0, 96000.0 }) {
        const double full = measureCost(rate, 128, false);
        const double bypassed = measureCost(rate, 128, true);
        std::cout << "     shifter accounts for "
                  << static_cast<int>(100.0 * (1.0 / full - 1.0 / bypassed) / (1.0 / full))
                  << "% of the chain's cost at " << rate << " Hz\n";
    }

    VoxeraAudioProcessor p;
    set(p, "delayFeedback", 100); CHECK(p.getTailLengthSeconds() > 85.0);
    set(p, "delayFeedback", 0); CHECK(p.getTailLengthSeconds() >= 7.8);
    set(p, "pitchKey", 9); p.applyFactoryPreset(3);
    CHECK(p.apvts.getRawParameterValue("pitchKey")->load() == 9.0f);
    set(p, "smartEQAmount", 65); set(p, "smartEQRange", 2.5f); set(p, "smartEQResponse", 600);
    juce::MemoryBlock state; p.getStateInformation(state);
    auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    auto* profile = xml->getChildByName("VoiceProfile"); CHECK(profile);
    profile->setAttribute("ready", true); profile->setAttribute("rms", -18.0); profile->setAttribute("crest", 9.0);
    juce::AudioProcessor::copyXmlToBinary(*xml, state);
    VoxeraAudioProcessor restored; restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    restored.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> silence(2, 128); silence.clear(); restored.processBlock(silence, midi);
    CHECK(restored.profileReady.load());
    CHECK(restored.apvts.getRawParameterValue("smartEQAmount")->load() == 65);
    CHECK(restored.apvts.getRawParameterValue("smartEQRange")->load() == 2.5f);
    CHECK(restored.apvts.getRawParameterValue("smartEQResponse")->load() == 600);
    // Older sessions must reset new parameters even when loading into a used instance.
    auto legacy = juce::ValueTree::fromXml(*xml);
    for (const auto* id : {"smartEQAmount", "smartEQRange", "smartEQResponse"})
        legacy.removeChild(legacy.getChildWithProperty("id", id), nullptr);
    auto legacyXml = legacy.createXml(); juce::MemoryBlock legacyData;
    juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyData);
    restored.setStateInformation(legacyData.getData(), static_cast<int>(legacyData.getSize()));
    CHECK(restored.apvts.getRawParameterValue("smartEQAmount")->load() == 0);
    CHECK(restored.apvts.getRawParameterValue("smartEQRange")->load() == 3);
    CHECK(restored.apvts.getRawParameterValue("smartEQResponse")->load() == 250);
    CHECK(restored.apvts.getRawParameterValue("pitchKey")->load() == 9.0f);
    restored.getStateInformation(state);
    auto after = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    CHECK(after->getChildByName("VoiceProfile")->getDoubleAttribute("rms") == -18.0);
    std::cout << "PASS state roundtrip: parameters and voice profile\n";
    juce::AudioBuffer<float> previewAudio(2, 24000);
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 24000; ++i)
        previewAudio.setSample(ch, i, 0.1f * (std::sin(i * 0.0288f) + 0.3f * std::sin(i * 0.0576f)));
    restored.processBlock(previewAudio, midi);
    const auto folder = juce::File(argc > 1 ? argv[1] : "/tmp/voxera-previews"); folder.createDirectory();
    std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());
    bool checkedAttachment = false, checkedEQAttachment = false;
    for (auto* child : editor->getChildren()) if (auto* slider = dynamic_cast<juce::Slider*>(child)) {
        if (slider->getName() == "AMOUNT %") {
            slider->setValue(55, juce::sendNotificationSync);
            CHECK(restored.apvts.getRawParameterValue("smartEQAmount")->load() == 55);
            slider->setValue(0, juce::sendNotificationSync); checkedEQAttachment = true;
        }
        if (slider->getName() == "TUNE") {
            const auto before = slider->getValue();
            slider->setValue(42.0, juce::sendNotificationSync);
            CHECK(std::abs(restored.apvts.getRawParameterValue("tuneAmount")->load() - 42.0f) < 0.01f);
            slider->setValue(before, juce::sendNotificationSync); checkedAttachment = true;
        }
    }
    CHECK(checkedAttachment && checkedEQAttachment);
    for (const auto& name : {"VOCALS", "FX", "PRESETS", "MORE", "SMART EQ", "CHOP"}) {
        for (auto* child : editor->getChildren()) if (auto* button = dynamic_cast<juce::TextButton*>(child))
            if (button->getButtonText() == name) button->onClick();
        if (juce::String(name) == "SMART EQ") {
            set(restored, "smartEQAmount", 100);
            juce::AudioBuffer<float> testTone(2, 96000);
            for (int i=0;i<96000;++i) for(int ch=0;ch<2;++ch)
                testTone.setSample(ch,i,0.2f*std::sin(static_cast<float>(i*juce::MathConstants<double>::twoPi*1000.0/48000.0)));
            restored.processBlock(testTone, midi);
        }
        const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        const auto file = folder.getChildFile(juce::String(name).toLowerCase().replace(" ", "_") + ".png");
        file.deleteFile();
        auto stream = file.createOutputStream(); CHECK(stream);
        juce::PNGImageFormat png; CHECK(png.writeImageToStream(image, *stream));
    }
    editor->setSize(960, 672);
    const auto small = editor->createComponentSnapshot(editor->getLocalBounds());
    folder.getChildFile("small.png").deleteFile();
    auto smallStream = folder.getChildFile("small.png").createOutputStream();
    juce::PNGImageFormat png; CHECK(png.writeImageToStream(small, *smallStream));
    std::cout << "PASS editor creation, tabs, snapshots, resize\n";
}

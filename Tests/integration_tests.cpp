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

        const int hidden = 3;
        const int gates = 4 * hidden;
        const int stride = 1 + hidden;   // one input column, then the hidden ones

        /*  Written in the layout NAM actually uses, with weights chosen so that
            layout is observable.

            The fixture this replaces set every gate weight to zero. That made
            the test unable to fail for two independent reasons: it laid the
            numbers out the way the loader read them rather than the way NAM
            writes them, and even had it not, all-zero matrices are identical
            whether you read them interleaved or in separate blocks, so the
            distinction under test left no trace in the output. The gates were
            also driven hard into saturation, which throws away whatever
            differences did survive.

            So: distinct non-zero weights throughout, small enough that no gate
            saturates, and initial states set far from zero so that a loader
            which forgets to step over them reads them as head weights and
            gives an obviously different answer.
        */
        std::vector<double> combined(static_cast<size_t>(gates * stride));
        for (int r = 0; r < gates; ++r)
            for (int c = 0; c < stride; ++c)
                combined[static_cast<size_t>(r * stride + c)] = 0.05 * (r + 1) + 0.02 * (c + 1);

        std::vector<double> gateBias(static_cast<size_t>(gates));
        for (int r = 0; r < gates; ++r) gateBias[static_cast<size_t>(r)] = -0.3 + 0.05 * r;

        std::vector<double> head(static_cast<size_t>(hidden));
        for (int h = 0; h < hidden; ++h) head[static_cast<size_t>(h)] = 0.06 + 0.02 * h;
        const double headBias = 0.01;

        std::vector<double> weights;
        weights.insert(weights.end(), combined.begin(), combined.end());
        weights.insert(weights.end(), gateBias.begin(), gateBias.end());
        for (int h = 0; h < hidden; ++h) weights.push_back(5.0 + h);   // initial hidden
        for (int h = 0; h < hidden; ++h) weights.push_back(8.0 + h);   // initial cell
        weights.insert(weights.end(), head.begin(), head.end());
        weights.push_back(headBias);

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

        /*  Everything but the network turned off, so the number that comes out
            is the network's and nothing else's.

            The previous version silenced five stages and left the rest at their
            defaults — the compressor, the de-esser, the doubler, the delay, the
            width — then compared against a tolerance wide enough to swallow
            what they did. That is two mistakes propping each other up: the
            chain was not isolated, and the tolerance was loose enough that
            nobody had to notice.
        */
        set(n, "neuralMix", 100.0f);
        for (const auto* id : { "pitchOn", "spectralOn", "spatialOn", "gateOn", "limiterOn" })
            set(n, id, 0.0f);
        for (const auto* id : { "autoGain", "satMix", "smartEQAmount", "clean", "deEss",
                                "width", "doubler", "delayMix", "punch", "exciter",
                                "optical", "glue", "vocalLock", "voiceMatch",
                                "density", "clipAmount", "satWarmth", "autoVoice",
                                "modMix", "chopAmount", "crush", "space", "duck" })
            set(n, id, 0.0f);
        // Ratio one is not enough on its own: the compressor's character stages
        // add drive whatever the ratio says, which on a steady input shows up as
        // a shifted level rather than as anything recognisable as compression.
        set(n, "compRatio", 1.0f);
        set(n, "compType", 0.0f);      // Clean: no drive, no bend, no even harmonics
        set(n, "toneMacro", 0.0f);
        n.prepareToPlay(48000.0, 256);

        juce::AudioBuffer<float> b(2, 256);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) b.setSample(ch, i, 0.1f);
        for (int block = 0; block < 40; ++block) n.processBlock(b, midi);

        /*  The expected value, worked out here rather than read off the loader.

            This is the whole point of the rewrite. An LSTM step is short enough
            to write out in full, so the test can compute what the network in
            that file settles to under a constant input without consulting the
            code it is testing. A fixture that agrees with the loader by
            construction proves only that the loader agrees with itself, which
            is exactly what the previous version of this test established while
            the loader read the format wrong.

            Gates in NAM's order: input, forget, cell candidate, output.
        */
        const auto sigmoid = [](double v) { return 1.0 / (1.0 + std::exp(-v)); };
        std::vector<double> h(static_cast<size_t>(hidden), 0.0), c(static_cast<size_t>(hidden), 0.0);
        const double x = 0.1;   // the DC fed through the plugin below

        /*  Exactly as many steps as the plugin took, not "enough to converge".

            The recurrent state advances once per sample from the moment the
            model loads, so after forty blocks of 256 it has taken 10240 steps
            and no more. Running the reference to its fixed point instead
            compares two different moments in the same trajectory and calls the
            gap a bug — which is what happened here: the first mismatch this
            test reported was my own impatience, not the loader.
        */
        for (int step = 0; step < 40 * 256; ++step) {
            std::vector<double> z(static_cast<size_t>(gates));
            for (int r = 0; r < gates; ++r) {
                double sum = combined[static_cast<size_t>(r * stride)] * x
                           + gateBias[static_cast<size_t>(r)];
                for (int k = 0; k < hidden; ++k)
                    sum += combined[static_cast<size_t>(r * stride + 1 + k)] * h[static_cast<size_t>(k)];
                z[static_cast<size_t>(r)] = sum;
            }
            for (int k = 0; k < hidden; ++k) {
                const double gi = sigmoid(z[static_cast<size_t>(k)]);
                const double gf = sigmoid(z[static_cast<size_t>(hidden + k)]);
                const double gg = std::tanh(z[static_cast<size_t>(2 * hidden + k)]);
                const double go = sigmoid(z[static_cast<size_t>(3 * hidden + k)]);
                c[static_cast<size_t>(k)] = gf * c[static_cast<size_t>(k)] + gi * gg;
                h[static_cast<size_t>(k)] = go * std::tanh(c[static_cast<size_t>(k)]);
            }
        }

        double expected = headBias;
        for (int k = 0; k < hidden; ++k) expected += head[static_cast<size_t>(k)] * h[static_cast<size_t>(k)];

        /*  Before believing the number, check the path that carried it.

            Comparing the plugin's output against a hand-computed network only
            means something if everything around the network passes the signal
            through untouched. Asserting that separately is what turns a
            mismatch from a mystery into a location: if this passes and the next
            one fails, the loader is wrong; if this fails, the test is.
        */
        VoxeraAudioProcessor plain;
        CHECK(plain.loadNeuralModel(file).ok);
        set(plain, "neuralMix", 0.0f);
        for (const auto* id : { "pitchOn", "spectralOn", "spatialOn", "gateOn", "limiterOn" })
            set(plain, id, 0.0f);
        for (const auto* id : { "autoGain", "satMix", "smartEQAmount", "clean", "deEss",
                                "width", "doubler", "delayMix", "punch", "exciter",
                                "optical", "glue", "vocalLock", "voiceMatch",
                                "density", "clipAmount", "satWarmth", "autoVoice",
                                "modMix", "chopAmount", "crush", "space", "duck" })
            set(plain, id, 0.0f);
        set(plain, "compRatio", 1.0f);
        set(plain, "compType", 0.0f);
        set(plain, "toneMacro", 0.0f);
        plain.prepareToPlay(48000.0, 256);

        juce::AudioBuffer<float> flat(2, 256);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) flat.setSample(ch, i, 0.1f);
        for (int block = 0; block < 40; ++block) {
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) flat.setSample(ch, i, 0.1f);
            plain.processBlock(flat, midi);
        }
        const double throughput = flat.getSample(0, 255);
        std::cout << "NEURAL chain transparency: 0.1 in, " << throughput << " out\n";
        CHECK(std::abs(throughput - 0.1) < 0.001);

        /*  The loader checked where the loader lives, not through the chain.

            Driving this from the plugin's input compares the network against a
            number that has already been through every stage ahead of it, and
            when the two disagree there is no way to tell which of twenty
            stages moved it. Measuring the whole chain's transparency does not
            rescue that: a stage before the network and a stage after it can
            each be off and still sum to a transparent-looking whole.

            So the network is driven directly, with a known sample going in and
            the same sample's worth of arithmetic to compare against.
        */
        voxera::NeuralStage direct;
        direct.prepare(48000.0, 256, 1);
        CHECK(direct.load(file).ok);
        direct.commitLoad();          // load only stages it; this is what arms it
        CHECK(direct.hasModel());
        direct.setMix(1.0f);

        juce::AudioBuffer<float> probe(1, 256);
        for (int block = 0; block < 40; ++block) {
            for (int i = 0; i < 256; ++i) probe.setSample(0, i, static_cast<float>(x));
            direct.process(probe);
        }
        const double got = probe.getSample(0, 255);

        std::cout << "NEURAL output: " << got << " expected " << expected << "\n";
        CHECK(std::abs(got - expected) < 0.001);

        /*  And through the plugin, as a smoke check only.

            Deliberately loose. The stage sits behind everything upstream of it,
            so the exact figure here belongs to the chain rather than to the
            loader, and pinning it tightly would only produce a test that fails
            whenever an unrelated stage changes. What it is worth asserting is
            that the model is reached at all and does something: silent
            non-application is the failure mode this catches.
        */
        const double throughChain = b.getSample(0, 255);
        std::cout << "NEURAL through the chain: " << throughChain << "\n";
        CHECK(std::isfinite(throughChain));
        CHECK(std::abs(throughChain - 0.1) > 0.01);

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

        /*  Asked of the processor rather than listed again here.

            This was a five-entry array indexed by the preset count, which read
            off its end the moment a sixth preset existed — a fault that would
            not have shown up as a wrong name but as whatever happened to follow
            it in memory.
        */
        VoxeraAudioProcessor namer;
        for (int preset = 0; preset < VoxeraAudioProcessor::numFactoryPresets; ++preset) {
            const auto [second, fundamental] = secondHarmonic(preset);
            const double ratio = second / juce::jmax(1.0e-9, fundamental);
            std::cout << "ANALOGUE " << namer.getProgramName(preset) << ": 2nd harmonic "
                      << (100.0 * ratio) << "% of the fundamental\n";
            CHECK(std::isfinite(second) && fundamental > 1.0e-4);
            // A tenth of a percent is far below audibility; the point of the
            // check is that it is not zero, which is what a missing control
            // would give.
            CHECK(ratio > 0.001);
        }
    }

    /*  Every preset reaches every control it claims to, and no two are the same.

        The failure this exists for has happened repeatedly here: a control gets
        added, the preset table is not widened, and the stage then never runs
        from a preset however good it sounds when found by hand. Nothing reports
        it — the preset still loads, the plugin still works, and the control
        simply sits at its default forever.

        So: read the parameters back after each preset, require that every
        column actually varies somewhere across the set, and require that the
        presets are distinguishable from each other. A column that never moves
        is either dead or pointless, and both are worth failing over.
    */
    {
        static constexpr const char* watched[] = {
            "tuneAmount", "retune", "humanize", "toneMacro", "airDb",
            "space", "satDrive", "satMix", "punch", "exciter",
            "optical", "density", "clipAmount", "smartEQAmount", "vocalLock",
            "satWarmth", "compType", "pitchMode", "formant", "doubler",
            "width", "delayMix", "reverbBody", "presenceDb", "deEss",
            "compMix", "compSidechain"
        };
        constexpr int watchedCount = static_cast<int>(std::size(watched));

        VoxeraAudioProcessor p;
        std::vector<std::vector<float>> seen;

        for (int preset = 0; preset < VoxeraAudioProcessor::numFactoryPresets; ++preset) {
            p.applyFactoryPreset(preset);
            std::vector<float> row;
            for (const auto* id : watched) {
                auto* parameter = p.apvts.getParameter(id);
                CHECK(parameter);
                row.push_back(parameter->convertFrom0to1(parameter->getValue()));
            }
            seen.push_back(row);
        }

        // No column may sit still across the whole set.
        for (int column = 0; column < watchedCount; ++column) {
            float lowest = seen[0][static_cast<size_t>(column)];
            float highest = lowest;
            for (const auto& row : seen) {
                lowest = juce::jmin(lowest, row[static_cast<size_t>(column)]);
                highest = juce::jmax(highest, row[static_cast<size_t>(column)]);
            }
            if (highest - lowest < 1.0e-4f)
                std::cout << "PRESET column never varies: " << watched[column] << "\n";
            CHECK(highest - lowest > 1.0e-4f);
        }

        // And no two presets may land on the same settings.
        for (size_t a = 0; a + 1 < seen.size(); ++a)
            for (size_t b = a + 1; b < seen.size(); ++b)
                CHECK(seen[a] != seen[b]);

        // The style presets are built around the tuner gripping hard; if that
        // stopped reaching them they would be indistinguishable from the rest.
        VoxeraAudioProcessor namer2;
        int hardTuned = 0;
        for (size_t i = 0; i < seen.size(); ++i)
            if (seen[i][17] > 1.5f) ++hardTuned;   // pitchMode: 2 is Hard
        std::cout << "PRESETS: " << VoxeraAudioProcessor::numFactoryPresets
                  << " total, " << hardTuned << " hard-tuned, all distinct\n";
        CHECK(hardTuned >= 3);
    }

    /*  Voice Match, end to end.

        A reference is learned from one take, then a second take of the same
        voice recorded darker — the way a step back from the microphone or a
        different day actually changes it — is measured before and after. What
        has to happen is that the second take's balance moves towards the first.

        Measuring the distance between the two spectra in decibels is the only
        honest test here: checking that some filter moved would pass even if it
        moved the wrong way.
    */
    {
        const auto runTake = [&](VoxeraAudioProcessor& p, float tilt, int seconds,
                                 juce::AudioBuffer<float>* collect) {
            const int blocks = seconds * 48000 / 512;
            int written = 0;
            for (int n = 0; n < blocks; ++n) {
                juce::AudioBuffer<float> b(2, 512);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i) {
                        const auto t = static_cast<float>(n * 512 + i) / 48000.0f;
                        // A voice-like stack: fundamental, some mids, some air.
                        // `tilt` darkens it by pulling the top down.
                        const float v = 0.30f * std::sin(juce::MathConstants<float>::twoPi * 200.0f * t)
                                      + 0.18f * std::sin(juce::MathConstants<float>::twoPi * 900.0f * t)
                                      + 0.14f * tilt * std::sin(juce::MathConstants<float>::twoPi * 3200.0f * t)
                                      + 0.10f * tilt * std::sin(juce::MathConstants<float>::twoPi * 6400.0f * t);
                        b.setSample(ch, i, v);
                    }
                p.processBlock(b, midi);
                if (collect != nullptr && written + 512 <= collect->getNumSamples()) {
                    collect->copyFrom(0, written, b, 0, 0, 512);
                    written += 512;
                }
            }
        };

        const auto quiet = [](VoxeraAudioProcessor& p) {
            for (const auto* id : { "pitchOn", "spectralOn", "spatialOn", "gateOn", "limiterOn" })
                set(p, id, 0.0f);
            for (const auto* id : { "smartEQAmount", "vocalLock", "punch", "exciter", "optical",
                                    "density", "clipAmount", "satMix", "satWarmth", "glue" })
                set(p, id, 0.0f);
            set(p, "autoGain", 0.0f);
            set(p, "compType", 0.0f);
            set(p, "compThreshold", 0.0f);
        };

        // Learn from a bright take.
        VoxeraAudioProcessor learner;
        quiet(learner);
        learner.prepareToPlay(48000.0, 512);
        learner.requestVoiceReference();
        runTake(learner, 1.0f, 8, nullptr);
        CHECK(learner.hasVoiceReference());

        // Carry the reference into a fresh instance, as reopening a session does.
        juce::MemoryBlock stored;
        learner.getStateInformation(stored);

        const auto topToBottom = [](const juce::AudioBuffer<float>& b) {
            // Ratio of energy above 2 kHz to energy below it, in decibels.
            Biquad low, high;
            low.prepare(48000.0, 1); low.setLowPass(2000.0);
            high.prepare(48000.0, 1); high.setHighPass(2000.0);
            double lowSum = 0.0, highSum = 0.0;
            for (int i = 0; i < b.getNumSamples(); ++i) {
                const float x = b.getSample(0, i);
                const float l = low.processSample(0, x), h = high.processSample(0, x);
                lowSum += static_cast<double>(l) * l;
                highSum += static_cast<double>(h) * h;
            }
            return 10.0 * std::log10(juce::jmax(1.0e-12, highSum) / juce::jmax(1.0e-12, lowSum));
        };

        juce::AudioBuffer<float> without(1, 48000 * 4), with(1, 48000 * 4);

        VoxeraAudioProcessor off;
        off.setStateInformation(stored.getData(), static_cast<int>(stored.getSize()));
        quiet(off);
        set(off, "voiceMatch", 0.0f);
        off.prepareToPlay(48000.0, 512);
        runTake(off, 0.35f, 6, &without);      // the darker take, uncorrected

        VoxeraAudioProcessor on;
        on.setStateInformation(stored.getData(), static_cast<int>(stored.getSize()));
        CHECK(on.hasVoiceReference());          // the reference survived the session
        quiet(on);
        set(on, "voiceMatch", 100.0f);
        on.prepareToPlay(48000.0, 512);
        runTake(on, 0.35f, 6, &with);          // the same take, matched

        const double target = topToBottom(without) ;
        const double corrected = topToBottom(with);
        std::cout << "VOICE MATCH: dark take " << target << " dB top/bottom, matched "
                  << corrected << " dB (reference was brighter)\n";

        for (int i = 0; i < with.getNumSamples(); ++i) CHECK(std::isfinite(with.getSample(0, i)));
        // The reference is brighter, so matching has to lift the ratio.
        CHECK(corrected > target + 0.5);
        CHECK(on.voiceMatchRangeDb() > 0.5f);
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

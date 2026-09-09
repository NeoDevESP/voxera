#include "../Source/PluginProcessor.h"
#include "Checks.h"
#include <thread>

static void set(VoxeraAudioProcessor& p, const char* id, float v) {
    auto* a = p.apvts.getParameter(id); CHECK(a); a->setValueNotifyingHost(a->convertTo0to1(v));
}
static float get(VoxeraAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue(id)->load(); }
static juce::MemoryBlock state(VoxeraAudioProcessor& p) { juce::MemoryBlock b; p.getStateInformation(b); return b; }
static void restore(VoxeraAudioProcessor& p, const juce::MemoryBlock& b) { p.setStateInformation(b.getData(), static_cast<int>(b.getSize())); }

static void captureTiming() {
    for (double rate : {44100.0, 48000.0, 96000.0}) for (int block : {64, 128, 512, 1024}) {
        voxera::VoiceMatch match; match.prepare(rate, 2);
        std::array<float, 1025> spectrum; spectrum.fill(0.02f);
        match.useSpectrum(spectrum.data(), 1025, rate / 2048.0, 1.0f, 0, 512);
        match.startCapture(6.0f);
        juce::AudioBuffer<float> b(2, block); b.clear();
        int samples = 0;
        while (match.isCapturing() && samples < rate * 7) {
            samples += block;
            match.useSpectrum(spectrum.data(), 1025, rate / 2048.0, 1.0f, static_cast<uint32_t>(samples / 512), 512);
            match.process(b);
        }
        CHECK(match.hasReference());
        CHECK(std::abs(samples / rate - 6.0) <= (512 + block) / rate);
    }
    std::cout << "PASS reference duration: 12 rate/buffer combinations\n";
}

static void stateAndPresets() {
    VoxeraAudioProcessor p, clean;
    auto empty = state(clean);
    auto xml = juce::AudioProcessor::getXmlFromBinary(empty.getData(), static_cast<int>(empty.getSize()));
    auto* ref = xml->createNewChildElement("VoiceReference");
    for (int i = 0; i < 8; ++i) ref->setAttribute("b" + juce::String(i), i - 4);
    juce::MemoryBlock withReference; juce::AudioProcessor::copyXmlToBinary(*xml, withReference);
    restore(p, withReference); CHECK(p.hasVoiceReference());
    p.clearVoiceReference();
    auto cleared = state(p); restore(clean, cleared); CHECK(!clean.hasVoiceReference());
    restore(p, withReference); restore(p, empty); CHECK(!p.hasVoiceReference());
    auto resaved = state(p); restore(clean, resaved); CHECK(!clean.hasVoiceReference());

    // A real, minimal RTNeural model verifies absence clears a running model too.
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("voxera-review", "", false);
    CHECK(folder.createDirectory());
    const auto model = folder.getChildFile("linear.json");
    CHECK(model.replaceWithText(R"({"in_shape":[null,1],"layers":[{"type":"dense","shape":[null,1],"weights":[[[1.0]],[0.0]],"activation":""}]})"));
    const auto loaded = p.loadNeuralModel(model);
    CHECK(loaded.ok); CHECK(p.hasNeuralModel());
    const auto withModel = state(p);
    restore(p, empty); CHECK(!p.hasNeuralModel());
    restore(p, withModel); CHECK(p.hasNeuralModel());
    p.unloadNeuralModel(); auto noModel = state(p); restore(clean, noModel); CHECK(!clean.hasNeuralModel());
    model.deleteFile(); restore(p, withModel); CHECK(!p.hasNeuralModel());
    CHECK(p.autoMixReport().contains("unavailable")); folder.deleteFile();

    set(p, "pitchKey", 7); set(p, "inputDb", -3);
    for (int preset = 0; preset < VoxeraAudioProcessor::numFactoryPresets; ++preset) {
        set(p, "compThreshold", -40); set(p, "modMix", 99); set(p, "neuralMix", 100);
        p.applyFactoryPreset(preset); const auto expected = state(p);
        set(p, "compThreshold", -8); set(p, "modMix", 35); set(p, "neuralMix", 42);
        p.applyFactoryPreset(preset); const auto actual = state(p);
        CHECK(actual == expected); CHECK(get(p, "pitchKey") == 7); CHECK(get(p, "inputDb") == -3);
    }
    std::cout << "PASS existing-instance state replacement, missing models, deterministic presets\n";
}

static void compression() {
    for (int type = 0; type < voxera::ColourCompressor::numTypes; ++type) {
        voxera::ColourCompressor same, opposite;
        same.prepare(48000, 2); opposite.prepare(48000, 2);
        for (auto* c : { &same, &opposite }) { c->setType(type); c->setThresholdDb(-24); c->setRatio(6); c->setColour(0); }
        juce::AudioBuffer<float> a(2, 128), b(2, 128);
        for (int n = 0; n < 800; ++n) {
            for (int i = 0; i < 128; ++i) {
                const float v = 0.5f * std::sin(static_cast<float>(2 * juce::MathConstants<double>::pi * 300 * (n * 128 + i) / 48000));
                a.setSample(0, i, v); a.setSample(1, i, v); b.setSample(0, i, v); b.setSample(1, i, -v);
            }
            same.process(a); opposite.process(b);
        }
        CHECK(same.getReductionDb() > 1.0f);
        CHECK(std::abs(same.getReductionDb() - opposite.getReductionDb()) < 0.001f);
        for (int i = 0; i < 128; ++i) CHECK(std::abs(a.getSample(0, i) - b.getSample(0, i)) < 1.0e-5f);
        b.clear(); for (int n = 0; n < 3000; ++n) { b.clear(); opposite.process(b); }
        CHECK(b.getMagnitude(0, 128) < 1.0e-5f);
    }
    // Compare processing partitions, not merely the control values.
    for (double sr : {44100.0, 96000.0}) {
        voxera::ColourCompressor a, b; a.prepare(sr, 2); b.prepare(sr, 2);
        a.setType(3); b.setType(3); a.setColour(1); b.setColour(1);
        juce::AudioBuffer<float> whole(2, 4096), split(2, 4096);
        for (int i = 0; i < 4096; ++i) for (int ch = 0; ch < 2; ++ch) whole.setSample(ch, i, 0.5f * std::sin(i * 0.09f));
        split.makeCopyOf(whole); a.process(whole);
        for (int offset = 0; offset < 4096; offset += 64) {
            float* ptrs[] { split.getWritePointer(0) + offset, split.getWritePointer(1) + offset };
            juce::AudioBuffer<float> block(ptrs, 2, 64); b.process(block);
        }
        for (int i = 0; i < 4096; ++i) CHECK(std::abs(whole.getSample(0, i) - split.getSample(0, i)) < 1.0e-6f);
    }
    std::cout << "PASS all compressor flavours: stereo polarity, silence and block invariance\n";
}

static void qualityAndMix() {
    VoiceProfileEngine profile; profile.prepare(48000); profile.startCapture();
    for (int i = 0; i < 8 * 48000; ++i) profile.pushSample(i < 48000 ? 0.1f * std::sin(i * 0.1f) : 0.0f);
    CHECK(!profile.getProfile().ready); CHECK(profile.getProfile().usefulSeconds < 1.2f);
    profile.startCapture(); for (int i = 0; i < 8 * 48000; ++i) profile.pushSample(i % 2 ? 1.0f : -1.0f);
    CHECK(!profile.getProfile().ready);

    VoxeraAudioProcessor p; set(p, "lowLatency", 1); set(p, "autoMixLockPitch", 1);
    set(p, "autoMixLockColour", 1); set(p, "tuneAmount", 17); set(p, "compColour", 23);
    set(p, "clean", 12); set(p, "autoMixIntensity", 50);
    p.prepareToPlay(48000, 512); p.requestAutoMix();
    juce::MidiBuffer midi; juce::AudioBuffer<float> b(2, 512);
    // A latency notification during capture must not apply an unfinished Auto Mix.
    set(p, "lowLatency", 0);
    for (int n = 0; n < 755; ++n) {
        for (int i = 0; i < 512; ++i) for (int c = 0; c < 2; ++c)
            b.setSample(c, i, 0.2f * std::sin(static_cast<float>((n * 512 + i) * 2 * juce::MathConstants<double>::pi * 220 / 48000)));
        p.processBlock(b, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(1);
        if (n == 2) CHECK(!p.canUndoAutoMix());
    }
    CHECK(p.canUndoAutoMix()); CHECK(get(p, "tuneAmount") == 17); CHECK(get(p, "compColour") == 23);
    const float after = get(p, "clean"); CHECK(after > 12 && after < 60);
    p.compareAutoMix(); CHECK(get(p, "clean") == 12); CHECK(p.isComparingBefore());
    p.compareAutoMix(); CHECK(get(p, "clean") == after);
    p.undoAutoMix(); CHECK(get(p, "clean") == 12); CHECK(!p.canUndoAutoMix());

    voxera::LevelMatch match; match.prepare(48000);
    juce::AudioBuffer<float> dry(2, 512), wet(2, 512);
    for (int n = 0; n < 1000; ++n) {
        for (int i = 0; i < 512; ++i) for (int c = 0; c < 2; ++c) {
            const float x = 0.1f * std::sin((n * 512 + i) * 0.1f); dry.setSample(c, i, x); wet.setSample(c, i, 2*x);
        }
        match.process(wet, dry, true);
    }
    CHECK(std::abs(wet.getRMSLevel(0, 0, 512) / dry.getRMSLevel(0, 0, 512) - 1) < 0.01f);
    std::cout << "PASS signal qualification, Auto Mix locks/intensity/A-B/undo and RMS matching\n";
}

static void postColourDeEss() {
    // No upstream analysis: this represents sibilance created by later stages.
    for (double sr : {44100.0, 48000.0, 96000.0}) {
        AdaptiveSpectralEngine a, b;
        a.prepare(sr, 128, 2); b.prepare(sr, 128, 2);
        a.setDeEss(1); b.setDeEss(1);
        juce::AudioBuffer<float> x(2,128), y(2,128);
        for (int n=0; n<500; ++n) {
            for (int i=0;i<128;++i) {
                const float v=0.2f*std::sin(float((n*128+i)*2*juce::MathConstants<double>::pi*8000/sr));
                x.setSample(0,i,v); x.setSample(1,i,v);
                y.setSample(0,i,v); y.setSample(1,i,-v);
            }
            a.processDeEss(x); b.processDeEss(y);
        }
        CHECK(a.deEssReduction.load()>4.0f);
        CHECK(std::abs(a.deEssReduction.load()-b.deEssReduction.load())<0.001f);
        CHECK(x.getRMSLevel(0,0,128)<0.11f);
        for (int n=0;n<500;++n) {
            for(int i=0;i<128;++i) for(int ch=0;ch<2;++ch)
                x.setSample(ch,i,0.2f*std::sin(float((n*128+i)*2*juce::MathConstants<double>::pi*220/sr)));
            a.processDeEss(x);
        }
        CHECK(a.deEssReduction.load()<0.1f);
    }
    voxera::Diagnosis clean; clean.ready=true;
    CHECK(std::abs(voxera::decide(clean).bodyDb)<0.001f);
    clean.mud=1; CHECK(voxera::decide(clean).bodyDb < -3.0f);
    std::cout << "PASS post-colour de-ess detection, stereo polarity, voice-body preservation\n";
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    captureTiming(); stateAndPresets(); compression(); qualityAndMix(); postColourDeEss();
}

#include "../Source/PluginProcessor.h"
#include "Checks.h"
#include <iostream>

void set(VoxeraAudioProcessor& p, const char* id, float value) {
    auto* parameter = p.apvts.getParameter(id); CHECK(parameter);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
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
    for (const auto& name : {"VOCALS", "FX", "PRESETS", "MORE", "SMART EQ"}) {
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

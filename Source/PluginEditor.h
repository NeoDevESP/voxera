#pragma once
#include "PluginProcessor.h"

class VoxeraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VoxeraLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
};

class VoxeraAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxeraAudioProcessorEditor(VoxeraAudioProcessor&);
    ~VoxeraAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct Control {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        int page = -1;
    };
    struct Choice {
        juce::ComboBox combo;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };
    Control& addControl(const char* id, const char* title, int page, bool large = false);
    void selectPage(int);
    void timerCallback() override;
    void filePreset(bool save);
    void drawMeter(juce::Graphics&, juce::Rectangle<float>, float, const juce::String&);
    void drawMascot(juce::Graphics&, juce::Rectangle<float>);
    VoxeraAudioProcessor& processor;
    VoxeraLookAndFeel theme;
    juce::TooltipWindow tooltips { this, 600 };
    std::vector<std::unique_ptr<Control>> controls;
    std::array<Choice, 3> choices;
    std::array<juce::TextButton, 5> tabs;
    std::array<juce::TextButton, 5> presets;
    juce::TextButton analyze { "ANALYZE VOICE" }, savePreset { "SAVE PRESET" }, loadPreset { "LOAD PRESET" };
    juce::TextButton autoMix { "AUTO MIX (8s)" };
    juce::TextButton bypass { "BYPASS" };
    juce::TextButton lowLatency { "LOW LATENCY" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lowLatencyAttachment;
    juce::Viewport advancedViewport;
    std::unique_ptr<juce::GenericAudioProcessorEditor> advancedEditor;
    std::unique_ptr<juce::FileChooser> chooser;
    int activePage = 0;
    float inPeak = -120.0f, outPeak = -120.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoxeraAudioProcessorEditor)
};

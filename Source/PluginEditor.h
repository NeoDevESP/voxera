#pragma once
#include "PluginProcessor.h"

/*  Development-only component inspector.

    Guarded rather than merely unused: the point of the CMake option is that the
    module is not compiled into a release binary at all, so this header must not
    assume it exists.
*/
#if VOXERA_WITH_INSPECTOR
 #include <melatonin_inspector/melatonin_inspector.h>
#endif

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
    // Compact horizontal bar for one stage's gain change, so it is visible at a
    // glance which of the ten stages is actually doing something.
    void drawWorking(juce::Graphics&, juce::Rectangle<float>, const juce::String&, float, float);
    void drawMascot(juce::Graphics&, juce::Rectangle<float>);
    VoxeraAudioProcessor& processor;
    VoxeraLookAndFeel theme;
    juce::TooltipWindow tooltips { this, 600 };
    std::vector<std::unique_ptr<Control>> controls;
    // Six: the three pitch selectors on VOCALS, and three more on CHOP.
    std::array<Choice, 6> choices;
    std::array<juce::TextButton, 6> tabs;
    std::array<juce::TextButton, 5> presets;
    juce::TextButton analyze { "ANALYZE VOICE" }, savePreset { "SAVE PRESET" }, loadPreset { "LOAD PRESET" };
    juce::TextButton autoMix { "AUTO MIX (8s)" };
    juce::TextButton loadModel { "LOAD NEURAL MODEL" };
    void chooseNeuralModel();
    juce::TextButton bypass { "BYPASS" };
    juce::TextButton lowLatency { "LOW LATENCY" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lowLatencyAttachment;
    juce::Viewport advancedViewport;
    std::unique_ptr<juce::GenericAudioProcessorEditor> advancedEditor;
    std::unique_ptr<juce::FileChooser> chooser;
    int activePage = 0;
    float inPeak = -120.0f, outPeak = -120.0f;
#if VOXERA_WITH_INSPECTOR
    // Opened with the I key. Holds a reference to this editor, so it has to be
    // declared last and destroyed first.
    std::unique_ptr<melatonin::Inspector> inspector;
    bool keyPressed(const juce::KeyPress&) override;
#endif
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoxeraAudioProcessorEditor)
};

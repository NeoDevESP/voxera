#include "PluginEditor.h"

namespace {
const juce::Colour pink(0xffff39bd), ink(0xff120b12), silver(0xffc9c8c6);
juce::Font font(float size, bool bold = false) {
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}
}

VoxeraLookAndFeel::VoxeraLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, ink);
    setColour(juce::Slider::thumbColourId, pink);
    setColour(juce::ScrollBar::thumbColourId, pink.withAlpha(0.65f));
    setColour(juce::Slider::textBoxTextColourId, pink);
    setColour(juce::Slider::textBoxBackgroundColourId, ink);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff261321));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffffc9ed));
    setColour(juce::ComboBox::outlineColourId, pink.withAlpha(0.4f));
    setColour(juce::PopupMenu::backgroundColourId, ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff822b68));
    setColour(juce::TextButton::textColourOffId, pink);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}
void VoxeraLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
    const juce::Colour&, bool over, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced(1.5f);
    const bool on = button.getToggleState();
    g.setColour(on || down ? juce::Colour(0xff842364) : juce::Colour(0xff21121e));
    g.fillRoundedRectangle(r, 7.0f);
    g.setColour(pink.withAlpha(on || over ? 1.0f : 0.28f));
    g.drawRoundedRectangle(r, 7.0f, on ? 1.6f : 1.0f);
}
void VoxeraLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
    float value, float start, float end, juce::Slider& slider)
{
    auto r = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)).reduced(9.0f);
    const float diameter = juce::jmin(r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre(diameter, diameter);
    const auto c = r.getCentre();
    const float radius = diameter * 0.5f;
    for (int i = 0; i <= 20; ++i) {
        const float angle = start + (end - start) * static_cast<float>(i) / 20.0f;
        const auto a = c.getPointOnCircumference(radius + 3.0f, angle);
        const auto b = c.getPointOnCircumference(radius + 7.0f, angle);
        g.setColour(slider.getProperties().getWithDefault("large", false) ? ink : pink.withAlpha(0.45f));
        g.drawLine({a, b}, 1.0f);
    }
    juce::Path shadowShape; shadowShape.addEllipse(r);
    juce::DropShadow(juce::Colours::black.withAlpha(0.55f), 8, {0, 4}).drawForPath(g, shadowShape);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffecebe8), r.getTopLeft(), juce::Colour(0xff222123), r.getBottomRight(), false));
    g.fillEllipse(r);
    g.setColour(juce::Colour(0xff131015)); g.drawEllipse(r, 2.0f);
    for (int i = 0; i < 90; ++i) {
        const float a = static_cast<float>(i) * juce::MathConstants<float>::twoPi / 90.0f;
        g.setColour(juce::Colours::white.withAlpha(i % 2 == 0 ? 0.5f : 0.15f));
        g.drawLine({c.getPointOnCircumference(radius - 2.0f, a), c.getPointOnCircumference(radius - 5.0f, a)}, 0.7f);
    }
    auto face = r.reduced(6.0f);
    juce::ColourGradient metal(juce::Colour(0xff454447), face.getTopLeft(), juce::Colour(0xffdeddda), face.getBottomRight(), false);
    metal.addColour(0.43, juce::Colour(0xfffaf9f6)); metal.addColour(0.52, juce::Colour(0xff8c8a89));
    g.setGradientFill(metal); g.fillEllipse(face);
    for (int i = 0; i < 72; ++i) {
        const float a = static_cast<float>(i) * juce::MathConstants<float>::twoPi / 72.0f;
        juce::Path wedge; wedge.startNewSubPath(c);
        wedge.lineTo(c.getPointOnCircumference(radius - 7.0f, a));
        wedge.lineTo(c.getPointOnCircumference(radius - 7.0f, a + 0.089f)); wedge.closeSubPath();
        g.setColour((std::sin(a * 2.0f) > 0.0f ? juce::Colours::white : juce::Colours::black)
            .withAlpha(std::abs(std::sin(a * 2.0f)) * 0.22f));
        g.fillPath(wedge);
    }
    g.setColour(juce::Colours::white.withAlpha(0.6f)); g.drawEllipse(face, 1.0f);
    const float angle = start + value * (end - start);
    const auto a = c.getPointOnCircumference(radius * 0.42f, angle);
    const auto b = c.getPointOnCircumference(radius * 0.76f, angle);
    g.setColour(ink); g.drawLine({a, b}, 4.0f);
    g.setColour(pink); g.fillEllipse(c.x - 2.0f, r.getY() - 17.0f, 4.0f, 4.0f);
}

VoxeraAudioProcessorEditor::Control& VoxeraAudioProcessorEditor::addControl(const char* id, const char* title, int page, bool large)
{
    auto control = std::make_unique<Control>();
    control->page = page;
    auto& slider = control->slider;
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, large ? 90 : 70, 22);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.getProperties().set("large", large);
    slider.setName(title); slider.setTooltip(juce::String(title) + " — drag to adjust; double-click to reset");
    slider.setScrollWheelEnabled(false);
    slider.setColour(juce::Slider::textBoxTextColourId, pink);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, ink);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    const auto* parameter = processor.apvts.getParameter(id);
    slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    control->label.setText(title, juce::dontSendNotification);
    control->label.setFont(font(large ? 17.0f : 11.0f, true));
    control->label.setJustificationType(juce::Justification::centred);
    control->label.setColour(juce::Label::textColourId, large ? ink : pink);
    addAndMakeVisible(slider); addAndMakeVisible(control->label);
    control->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, id, slider);
    auto& result = *control;
    controls.push_back(std::move(control)); return result;
}

VoxeraAudioProcessorEditor::VoxeraAudioProcessorEditor(VoxeraAudioProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    setLookAndFeel(&theme);
    addControl("tuneAmount", "TUNE", -1, true);
    addControl("toneMacro", "TONE", -1, true);
    addControl("airDb", "AIR", -1, true);
    addControl("space", "SPACE", -1, true);
    addControl("globalMix", "MIX", -1, true);
    addControl("retune", "RETUNE", 0);
    addControl("humanize", "HUMANIZE", 0);
    addControl("autoVoice", "AUTO VOICE", 0);
    addControl("width", "WIDTH", 1);
    addControl("doubler", "DOUBLE", 1);
    addControl("delayMix", "DELAY", 1);
    addControl("delayFeedback", "FEEDBACK", 1);
    addControl("satDrive", "DRIVE", 1);
    addControl("satMix", "SAT MIX", 1);
    addControl("smartEQAmount", "AMOUNT %", 4);
    addControl("smartEQRange", "BUDGET dB", 4);
    addControl("smartEQResponse", "RESPONSE ms", 4);
    addControl("chopAmount", "CHOP", 5);
    addControl("crush", "CRUSH", 5);
    addControl("crushMix", "CRUSH MIX", 5);
    addControl("modRate", "MOD RATE", 5);
    addControl("modDepth", "MOD DEPTH", 5);
    addControl("modMix", "MOD MIX", 5);
    addControl("glue", "GLUE", 5);
    addControl("compColour", "COMP SAUCE", 2);
    compressorChoice.combo.addItemList(dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter("compType"))->choices, 1);
    compressorChoice.combo.setName("Compressor character");
    compressorChoice.label.setText("COMPRESSOR", juce::dontSendNotification);
    compressorChoice.label.setColour(juce::Label::textColourId, pink);
    addAndMakeVisible(compressorChoice.combo); addAndMakeVisible(compressorChoice.label);
    compressorChoice.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "compType", compressorChoice.combo);
    const char* tabNames[] = {"VOCALS", "FX", "PRESETS", "MORE", "SMART EQ", "CHOP"};
    for (int i = 0; i < 6; ++i) {
        auto& tab = tabs[static_cast<size_t>(i)];
        tab.setButtonText(tabNames[i]); tab.onClick = [this, i] { selectPage(i); };
        addAndMakeVisible(tab);
    }
    const char* choiceIDs[] = {"pitchKey", "pitchScale", "pitchMode",
                               "chopDivision", "chopPattern", "modType"};
    const char* choiceNames[] = {"KEY", "SCALE", "MODE", "RATE", "PATTERN", "MOD"};
    for (int i = 0; i < 6; ++i) {
        auto& c = choices[static_cast<size_t>(i)];
        auto* param = dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter(choiceIDs[i]));
        c.combo.addItemList(param->choices, 1); c.combo.setName(choiceNames[i]);
        c.label.setText(choiceNames[i], juce::dontSendNotification); c.label.setFont(font(11.0f, true));
        c.label.setColour(juce::Label::textColourId, pink);
        addAndMakeVisible(c.combo); addAndMakeVisible(c.label);
        c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, choiceIDs[i], c.combo);
    }
    /*  Names taken from the processor rather than repeated here.

        They were written out twice, and a second list is a second thing to
        forget: adding a preset to the table and not to this array gives a
        button labelled with somebody else's preset, which is worse than a
        missing one because it looks right.
    */
    for (int i = 0; i < VoxeraAudioProcessor::numFactoryPresets; ++i) {
        auto& b = presets[static_cast<size_t>(i)];
        b.setButtonText(processor.getProgramName(i).toUpperCase());
        b.onClick = [this, i] { processor.applyFactoryPreset(i); };
        addAndMakeVisible(b);
    }
    analyze.onClick = [this] { processor.requestVoiceCapture(); };
    analyze.setTooltip("Sing a representative phrase for 8 seconds while audio is running.");
    autoMix.onClick = [this] { processor.requestAutoMix(); };
    compareMix.onClick = [this] { processor.compareAutoMix(); };
    undoMix.onClick = [this] { processor.undoAutoMix(); };
    mixOptions.onClick = [this] { showMixOptions(); };
    matchLevel.setClickingTogglesState(true);
    matchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "levelMatch", matchLevel);
    compareMix.setTooltip("Switch between settings before and after the last Auto Mix. Enable MATCH LEVEL for RMS comparison; allow a few seconds to settle.");
    undoMix.setTooltip("Restore every setting changed by the last Auto Mix in one click.");
    mixOptions.setTooltip("Choose intensity and keep your tuning, EQ, dynamics or colour unchanged.");
    matchLevel.setTooltip("Match processed RMS to the delayed input, with up to 36 dB attenuation or 9 dB boost. The limiter remains downstream. This is a comparison aid, not a LUFS measurement.");
    autoMix.setTooltip("Listens for 8 seconds, then sets EQ, de-ess, dynamics, punch, "
                       "exciter and tuning from what it heard. Every control stays editable "
                       "afterwards, and the move can be undone.");
    savePreset.onClick = [this] { filePreset(true); };
    loadPreset.onClick = [this] { filePreset(false); };
    learnVoice.onClick = [this] {
        if (processor.hasVoiceReference()) processor.clearVoiceReference();
        else processor.requestVoiceReference();
    };
    learnVoice.setTooltip("Learns the tonal balance of the take playing now, over six seconds. "
                          "Other takes can then be matched to it with the Voice Match control, "
                          "so a session recorded on a different day or at a different distance "
                          "from the microphone still sits with the rest. Click again to forget it.");
    loadModel.onClick = [this] { chooseNeuralModel(); };
    insertButton.onClick = [this] { chooseInsertPlugin(); };
    insertButton.setTooltip("Puts one of your own plugins inside this chain, after the dynamics "
                            "and before the harmonic stages. Auto Mix will set its main control "
                            "if it can recognise one.");
    loadModel.setTooltip("Loads a Neural Amp Modeler capture (.nam) or an RTNeural model (.json) "
                         "and runs it where a preamp would sit. Captures of microphone preamps and "
                         "console channels are what suit a voice. NAM LSTM and WaveNet up to 12 channels "
                         "are supported. Nothing is bundled, so the "
                         "capture is yours to provide or to make.");
    bypass.setClickingTogglesState(true);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "bypass", bypass);
    lowLatency.setClickingTogglesState(true);
    lowLatency.setTooltip("For singing through the plugin. Takes the pitch shifter out of the "
                          "path, which is where nearly all the delay comes from: about 59 ms "
                          "drops to under 5. Everything else keeps working; only the tuning "
                          "stops. Turn it off again to mix.");
    lowLatencyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "lowLatency", lowLatency);
    motion.setClickingTogglesState(true);
    motion.setToggleState(true, juce::dontSendNotification);
    motion.setTooltip("Turn decorative animation on or off. Audio meters remain live.");
    motion.onClick = [this] {
        motion.setButtonText(motion.getToggleState() ? "MOTION ON" : "MOTION OFF");
        animationTime = voiceMotion = 0.0f;
        repaint();
    };
    addAndMakeVisible(motion);
    for (auto* b : {&compareMix, &undoMix, &mixOptions, &matchLevel, &analyze, &autoMix, &learnVoice, &savePreset, &loadPreset, &loadModel, &insertButton,
                    &bypass, &lowLatency})
        addAndMakeVisible(b);
    advancedEditor = std::make_unique<juce::GenericAudioProcessorEditor>(processor);
    advancedViewport.setViewedComponent(advancedEditor.get(), false);
    addAndMakeVisible(advancedViewport);
    setResizable(true, true); setResizeLimits(960, 672, 1600, 1120);
    getConstrainer()->setFixedAspectRatio(1200.0 / 840.0);
    setSize(1200, 840); selectPage(0); timerCallback(); startTimerHz(30);
}
VoxeraAudioProcessorEditor::~VoxeraAudioProcessorEditor()
{
    stopTimer(); chooser.reset();
    /*  The hosted plugin's window goes before anything else.

        It shows an editor belonging to the plugin in the insert slot, and that
        plugin outlives this editor: closing the DAW's plugin window must not
        leave a floating window pointing into something whose owner is on its
        way out.
    */
    closeInsertWindow();
    advancedViewport.setViewedComponent(nullptr, false);
    setLookAndFeel(nullptr);
}
void VoxeraAudioProcessorEditor::selectPage(int page)
{
    activePage = page;
    for (auto& c : controls) {
        const bool visible = c->page < 0 || c->page == page;
        c->slider.setVisible(visible); c->label.setVisible(visible);
    }
    // The first three selectors belong to VOCALS, the last three to CHOP.
    for (size_t i = 0; i < choices.size(); ++i) {
        const bool visible = (i < 3) ? (page == 0) : (page == 5);
        choices[i].combo.setVisible(visible); choices[i].label.setVisible(visible);
    }
    for (auto& b : presets) b.setVisible(page == 2);
    analyze.setVisible(page == 0); autoMix.setVisible(page == 0);
    compareMix.setVisible(page == 0); undoMix.setVisible(page == 0);
    mixOptions.setVisible(page == 0); matchLevel.setVisible(page == 0);
    learnVoice.setVisible(page == 0);
    savePreset.setVisible(page == 2); loadPreset.setVisible(page == 2);
    loadModel.setVisible(page == 2);
    insertButton.setVisible(page == 2);
    compressorChoice.combo.setVisible(page == 2); compressorChoice.label.setVisible(page == 2);
    advancedViewport.setVisible(page == 3);
    for (int i = 0; i < 6; ++i) tabs[static_cast<size_t>(i)].setToggleState(i == page, juce::dontSendNotification);
    resized(); repaint();
}
void VoxeraAudioProcessorEditor::resized()
{
    const float scale = static_cast<float>(getWidth()) / 1200.0f;
    auto place = [scale](juce::Component& c, int x, int y, int w, int h) {
        c.setBounds((juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)) * scale).toNearestInt());
    };
    // Six tabs now, so they are narrower and start further left. The row must
    // end before the OUT meter, which owns the panel from x=1000.
    for (int i = 0; i < 6; ++i) place(tabs[static_cast<size_t>(i)], 285 + i * 106, 87, 98, 30);

    /*  CHOP page. The three selectors sit on the left where VOCALS puts its
        pitch selectors, and the seven knobs fill the rest of the panel in two
        rows, clear of the y=124 meter strip above and the y=548 divider below.
    */
    for (int i = 0; i < 3; ++i) {
        auto& c = choices[static_cast<size_t>(3 + i)];
        place(c.label, 98 + i * 160, 350, 150, 20);
        place(c.combo, 98 + i * 160, 382, 148, 32);
    }
    for (int i = 0; i < 4; ++i) {
        auto& c = *controls[static_cast<size_t>(17 + i)];
        place(c.label, 600 + i * 130, 350, 112, 22);
        place(c.slider, 600 + i * 130, 384, 112, 100);
    }
    for (int i = 0; i < 3; ++i) {
        auto& c = *controls[static_cast<size_t>(21 + i)];
        // The dark panel ends near y=520; a taller row puts the value boxes out
        // over the bezel.
        place(c.label, 98 + i * 160, 414, 150, 20);
        place(c.slider, 98 + i * 160, 436, 150, 82);
    }
    for (int i = 0; i < 5; ++i) {
        auto& c = *controls[static_cast<size_t>(i)];
        place(c.label, 195 + i * 176, 560, 146, 26);
        place(c.slider, 195 + i * 176, 605, 146, 155);
    }
    for (int i = 0; i < 3; ++i) {
        auto& c = choices[static_cast<size_t>(i)];
        place(c.label, 98 + i * 160, 357, 150, 20);
        place(c.combo, 98 + i * 160, 382, 148, 32);
        auto& knob = *controls[static_cast<size_t>(5 + i)];
        place(knob.label, 620 + i * 130, 350, 112, 22);
        place(knob.slider, 620 + i * 130, 384, 112, 104);
    }
    for (int i = 0; i < 3; ++i) {
        auto& c = *controls[static_cast<size_t>(14 + i)];
        place(c.label, 195 + i * 240, 350, 150, 22);
        place(c.slider, 195 + i * 240, 384, 150, 104);
    }
    // Three buttons where there used to be two, and the dark panel's inner edge
    // is at about y=520: shorter and tighter so the last one stays inside it.
    place(analyze, 98, 430, 190, 26);
    place(autoMix, 98, 459, 190, 26);
    place(learnVoice, 98, 488, 190, 26);
    place(compareMix, 310, 430, 130, 26); place(undoMix, 448, 430, 130, 26);
    place(mixOptions, 310, 462, 130, 26); place(matchLevel, 448, 462, 130, 26);
    for (int i = 0; i < 6; ++i) {
        auto& c = *controls[static_cast<size_t>(8 + i)];
        place(c.label, 115 + i * 157, 350, 120, 22);
        place(c.slider, 115 + i * 157, 388, 120, 103);
    }
    // Ten across the same span the five used to have. The cell is narrower than
    // the pitch so neighbouring buttons never share an edge, which is the same
    // reason the meters above are laid out that way.
    for (int i = 0; i < VoxeraAudioProcessor::numFactoryPresets; ++i)
        place(presets[static_cast<size_t>(i)], 110 + i * 98, 374, 92, 40);
    place(savePreset, 405, 440, 178, 34); place(loadPreset, 603, 440, 178, 34);
    place(loadModel, 405, 484, 184, 32);
    place(insertButton, 597, 484, 184, 32);
    place(compressorChoice.label, 110, 438, 220, 20);
    place(compressorChoice.combo, 110, 466, 230, 32);
    place(controls[24]->label, 845, 424, 160, 22);
    place(controls[24]->slider, 845, 448, 160, 68);
    place(bypass, 63, 644, 104, 54);
    place(lowLatency, 63, 706, 104, 34);
    place(motion, 1000, 22, 130, 30);
    place(advancedViewport, 92, 337, 1012, 172);
    advancedEditor->setSize(advancedViewport.getWidth(), advancedViewport.getHeight());
}
void VoxeraAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float elapsed = lastAnimationTime > 0.0
        ? static_cast<float>(juce::jlimit(0.0, 0.1, now - lastAnimationTime)) : 0.0f;
    lastAnimationTime = now;
    if (isShowing() && motion.getToggleState() && !bypass.getToggleState()) {
        animationTime = std::fmod(animationTime + elapsed, 60.0f);
        const float target = juce::jlimit(0.0f, 1.0f,
            (processor.inputMeters.peakDb.load() + 48.0f) / 42.0f);
        const float response = target > voiceMotion ? 0.08f : 0.24f;
        voiceMotion += (target - voiceMotion) * (1.0f - std::exp(-elapsed / response));
    } else {
        animationTime = voiceMotion = 0.0f;
    }
    inPeak = juce::jmax(processor.inputMeters.peakDb.load(), inPeak - 1.3f);
    outPeak = juce::jmax(processor.meters.peakDb.load(), outPeak - 1.3f);
    const bool running = processor.capturing.load();
    analyze.setEnabled(!running);
    autoMix.setEnabled(!running && !processor.isAutoMixPending());
    compareMix.setEnabled(processor.canUndoAutoMix() && !running);
    undoMix.setEnabled(processor.canUndoAutoMix() && !running);
    compareMix.setButtonText(processor.isComparingBefore() ? "A/B: BEFORE" : "A/B: AFTER");
    loadModel.setButtonText(processor.hasNeuralModel() ? processor.neuralModelName().toUpperCase() : "LOAD NEURAL MODEL");

    // The button says what it will do next, so there is no separate indicator
    // to read for whether a reference exists.
    if (processor.isLearningReference())
        learnVoice.setButtonText("LEARNING "
            + juce::String(static_cast<int>(processor.referenceProgress() * 100.0f)) + "%");
    else
        learnVoice.setButtonText(processor.hasVoiceReference() ? "FORGET THIS VOICE"
                                                              : "LEARN THIS VOICE");
    // The full reasoning does not fit on the panel, so it lives here. Compared
    // before assigning: this runs thirty times a second and the text changes
    // only when an analysis finishes.
    if (const auto& report = processor.autoMixReport();
        report.isNotEmpty() && report != autoMix.getTooltip())
        autoMix.setTooltip(report);
    analyze.setButtonText(running ? "ANALYZING " + juce::String(static_cast<int>(processor.captureProgress.load() * 100.0f)) + "%" : "ANALYZE VOICE");
    repaint();
}
#if VOXERA_WITH_INSPECTOR
bool VoxeraAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (!key.isKeyCode('I') && !key.isKeyCode('i')) return false;
    if (inspector == nullptr) inspector = std::make_unique<melatonin::Inspector>(*this);
    inspector->setVisible(true);
    inspector->toggle(true);
    return true;
}
#endif

void VoxeraAudioProcessorEditor::applyNeuralModel(const juce::File& file)
{
    const auto result = processor.loadNeuralModel(file);
    if (!result.ok) {
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "VOXERA", result.message);
        return;
    }
    loadModel.setButtonText(processor.neuralModelName().toUpperCase());
    // Reported on success too: running at a different rate from the session is
    // worth knowing about even though the stage now handles it.
    if (result.message.contains("captured at"))
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon, "VOXERA", result.message);
}

/*  A menu of what is in the models folder, rather than a file chooser.

    Captures are collected once and used for years, so hunting through the
    filesystem on every session is the wrong shape for the task. Browsing is
    still there for a file kept somewhere else, and the folder itself can be
    opened straight from here, which is how anything gets into it.
*/
/*  Everything installed, listed by name, without a scan that looks like a hang.

    Reading the folders is instant; instantiating a plugin to learn its real
    name is not, and doing that for several hundred of them before showing a
    menu would leave a person staring at nothing. So the menu shows file names,
    and the real name arrives on the button once the chosen one has loaded.
*/
void VoxeraAudioProcessorEditor::chooseInsertPlugin()
{
    juce::Array<juce::File> found;
    for (const auto& folder : VoxeraAudioProcessor::installedPluginFolders())
        folder.findChildFiles(found, juce::File::findFilesAndDirectories, false, "*.vst3");
    found.sort();

    juce::PopupMenu menu;
    if (found.isEmpty()) {
        menu.addSectionHeader("No VST3 plugins found in the usual folders");
    } else {
        juce::PopupMenu list;
        for (int i = 0; i < found.size(); ++i)
            list.addItem(i + 1, found[i].getFileNameWithoutExtension(), true,
                         found[i] == processor.insertPluginFile());
        menu.addSubMenu("Installed (" + juce::String(found.size()) + ")", list);
    }
    menu.addItem(-1, "Browse for a plugin...");
    if (processor.hasInsertPlugin()) {
        menu.addSeparator();
        menu.addItem(-2, "Open its window");
        menu.addItem(-3, "Remove it from the chain");
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(insertButton),
        [safe = juce::Component::SafePointer<VoxeraAudioProcessorEditor>(this), found](int choice) {
            if (safe == nullptr || choice == 0) return;

            if (choice == -2) { safe->showInsertWindow(); return; }
            if (choice == -3) {
                // The window first, then the plugin: it holds a pointer into it.
                safe->closeInsertWindow();
                safe->processor.unloadInsertPlugin();
                safe->insertButton.setButtonText("INSERT PLUGIN");
                return;
            }

            const auto load = [safe](const juce::File& file) {
                if (safe == nullptr) return;
                safe->closeInsertWindow();
                const auto result = safe->processor.loadInsertPlugin(file);
                safe->insertButton.setButtonText(result.ok
                    ? safe->processor.insertPluginName().toUpperCase() : juce::String("INSERT PLUGIN"));
                if (!result.ok)
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                        "That plugin did not load", result.message);
                else
                    safe->showInsertWindow();
            };

            if (choice == -1) {
                safe->chooser = std::make_unique<juce::FileChooser>("Choose a VST3 plugin",
                    VoxeraAudioProcessor::installedPluginFolders().isEmpty()
                        ? juce::File() : VoxeraAudioProcessor::installedPluginFolders().getFirst(),
                    "*.vst3");
                safe->chooser->launchAsync(juce::FileBrowserComponent::openMode
                                         | juce::FileBrowserComponent::canSelectFiles
                                         | juce::FileBrowserComponent::canSelectDirectories,
                    [load](const juce::FileChooser& fc) {
                        if (fc.getResult() != juce::File()) load(fc.getResult());
                    });
                return;
            }

            if (juce::isPositiveAndBelow(choice - 1, found.size())) load(found[choice - 1]);
        });
}

/*  The hosted plugin's own window.

    Its editor is not ours to resize or restyle, so the window simply takes the
    size the plugin asks for. Closing it deletes the editor but leaves the
    plugin loaded and running, which is what a person expects from closing a
    window rather than removing a device.
*/
void VoxeraAudioProcessorEditor::showInsertWindow()
{
    if (!processor.hasInsertPlugin()) return;
    if (insertWindow != nullptr) { insertWindow->toFront(true); return; }
    if (!processor.insertHasEditor()) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            processor.insertPluginName(),
            "That plugin has no window of its own. Its controls are still there and Auto Mix "
            "can still reach them.");
        return;
    }

    auto* hosted = processor.createInsertEditor();
    if (hosted == nullptr) return;

    class HostedWindow : public juce::DocumentWindow
    {
    public:
        HostedWindow(const juce::String& name, VoxeraAudioProcessorEditor& owner)
            : juce::DocumentWindow(name, juce::Colour(0xff120b12), closeButton), editor(owner) {}
        // Routed through the owner so the pointer it keeps is cleared too, and
        // a second click does not try to show a window that is already gone.
        void closeButtonPressed() override { editor.closeInsertWindow(); }
    private:
        VoxeraAudioProcessorEditor& editor;
    };

    auto window = std::make_unique<HostedWindow>(processor.insertPluginName(), *this);
    window->setUsingNativeTitleBar(true);
    /*  Owned, so closing the window destroys the editor.

        This was the crash, and FL Studio's own log named it: an access
        violation inside the hosted plugin, reached through USER32 rather than
        through any audio path, on three different plugins.

        The window used to take the editor without owning it, and closing it
        detached the editor instead of deleting it. What was left was an editor
        still alive, still registered with its plugin, still holding a native
        window — with nothing left to parent it. The next paint or timer message
        Windows delivered went into an object whose world had been dismantled.

        Owning it means the editor is destroyed properly, which is also what
        tells the plugin that its editor is gone: that notification comes from
        the editor's own destructor and never happens if nobody destroys it.
    */
    window->setContentOwned(hosted, true);
    window->setResizable(hosted->isResizable(), false);
    window->centreWithSize(hosted->getWidth(), hosted->getHeight());
    window->setVisible(true);
    insertWindow = std::move(window);
}

void VoxeraAudioProcessorEditor::closeInsertWindow()
{
    if (insertWindow == nullptr) return;
    // Simply released: the window owns the editor, so this destroys it, and the
    // editor's destructor is what tells the plugin it no longer has one.
    // Clearing the content first was the fault — it detached without deleting.
    insertWindow.reset();
}

void VoxeraAudioProcessorEditor::chooseNeuralModel()
{
    juce::PopupMenu menu;
    const auto models = VoxeraAudioProcessor::availableNeuralModels();

    if (models.isEmpty()) {
        menu.addSectionHeader("No captures in your models folder");
        menu.addItem(-3, "Open the models folder");
    } else {
        menu.addSectionHeader("Models folder");
        for (int i = 0; i < models.size(); ++i)
            menu.addItem(i + 1, models[i].getFileNameWithoutExtension(), true,
                         models[i] == processor.neuralModelFile());
        menu.addSeparator();
        menu.addItem(-3, "Open the models folder");
    }

    menu.addItem(-1, "Browse for a file...");
    if (processor.hasNeuralModel()) {
        menu.addSeparator();
        menu.addItem(-2, "Remove the current model");
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(loadModel),
        [safe = juce::Component::SafePointer<VoxeraAudioProcessorEditor>(this), models](int choice) {
            if (safe == nullptr) return;
            auto& processor = safe->processor;
            auto& loadModel = safe->loadModel;
            auto& chooser = safe->chooser;
            if (choice == 0) return;

            if (choice == -2) {
                processor.unloadNeuralModel();
                loadModel.setButtonText("LOAD NEURAL MODEL");
                return;
            }
            if (choice == -3) {
                VoxeraAudioProcessor::neuralModelFolder().revealToUser();
                return;
            }
            if (choice == -1) {
                chooser = std::make_unique<juce::FileChooser>(
                    "Load a neural capture",
                    VoxeraAudioProcessor::neuralModelFolder(), "*.nam;*.json");
                chooser->launchAsync(juce::FileBrowserComponent::openMode
                                     | juce::FileBrowserComponent::canSelectFiles,
                    [safe](const juce::FileChooser& fc) {
                        if (safe != nullptr && fc.getResult() != juce::File{}) safe->applyNeuralModel(fc.getResult());
                    });
                return;
            }
            if (juce::isPositiveAndBelow(choice - 1, models.size()))
                safe->applyNeuralModel(models[choice - 1]);
        });
}

void VoxeraAudioProcessorEditor::showMixOptions()
{
    juce::PopupMenu menu;
    const int amount = static_cast<int>(processor.apvts.getRawParameterValue("autoMixIntensity")->load());
    menu.addSectionHeader("Auto Mix intensity");
    for (int value : {25, 50, 75, 100}) menu.addItem(value, juce::String(value) + "%", true, amount == value);
    menu.addSeparator();
    const char* ids[] { "autoMixLockPitch", "autoMixLockEQ", "autoMixLockDynamics", "autoMixLockColour" };
    const char* names[] { "Keep my tuning", "Keep my EQ", "Keep my dynamics", "Keep my colour" };
    for (int i = 0; i < 4; ++i) menu.addItem(201 + i, names[i], true, processor.apvts.getRawParameterValue(ids[i])->load() > 0.5f);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(mixOptions),
        [safe = juce::Component::SafePointer<VoxeraAudioProcessorEditor>(this)](int choice) {
            if (safe == nullptr || choice == 0) return;
            const char* lockIds[] { "autoMixLockPitch", "autoMixLockEQ", "autoMixLockDynamics", "autoMixLockColour" };
            const char* id = choice <= 100 ? "autoMixIntensity" : lockIds[juce::jlimit(0, 3, choice - 201)];
            auto* p = safe->processor.apvts.getParameter(id);
            const float value = choice <= 100 ? static_cast<float>(choice) : (p->getValue() > 0.5f ? 0.0f : 1.0f);
            p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(value)); p->endChangeGesture();
        });
}

void VoxeraAudioProcessorEditor::filePreset(bool save)
{
    chooser = std::make_unique<juce::FileChooser>(save ? "Save VOXERA preset" : "Load VOXERA preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.voxera");
    const int flags = save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting
                           : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<VoxeraAudioProcessorEditor> safe(this);
    chooser->launchAsync(flags, [safe, save](const juce::FileChooser& fc) {
        if (safe == nullptr) return;
        auto file = fc.getResult(); if (file == juce::File()) return;
        bool ok = false;
        if (save) {
            file = file.withFileExtension("voxera");
            juce::MemoryBlock data; safe->processor.getStateInformation(data);
            ok = file.replaceWithData(data.getData(), data.getSize());
        } else {
            juce::MemoryBlock data;
            if (file.getSize() <= 1024 * 1024 && file.loadFileAsData(data)) {
                auto xml = juce::AudioProcessor::getXmlFromBinary(data.getData(), static_cast<int>(data.getSize()));
                if (xml != nullptr && xml->hasTagName("PARAMETERS")) {
                    safe->processor.setStateInformation(data.getData(), static_cast<int>(data.getSize())); ok = true;
                }
            }
        }
        if (!ok) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "VOXERA", save ? "The preset could not be saved." : "This is not a readable VOXERA preset.");
    });
}
void VoxeraAudioProcessorEditor::drawMeter(juce::Graphics& g, juce::Rectangle<float> r, float db, const juce::String& label)
{
    g.setFont(font(11.0f, true)); g.setColour(pink); g.drawText(label, r.removeFromTop(19).toNearestInt(), juce::Justification::left);
    for (int i = 0; i < 14; ++i) {
        g.setColour(db >= -60.0f + static_cast<float>(i) * 60.0f / 13.0f ? (db > 0.0f ? juce::Colours::red : pink) : juce::Colour(0xff42203a));
        g.fillRoundedRectangle(r.getX() + static_cast<float>(i) * 7.0f, r.getY(), 4.5f, 12.0f, 1.0f);
    }
    g.setColour(pink); g.drawText(juce::String(db, 1) + " dBFS", r.withTrimmedTop(18).toNearestInt(), juce::Justification::left);
}
/*  One stage's activity on a single line: name, bar, figure.

    Laid out horizontally rather than stacked because the only band of the panel
    that is free on every page — between the tab row and the logo — is barely
    thirty pixels tall, and a stacked meter does not fit there without landing on
    top of the tabs.
*/
void VoxeraAudioProcessorEditor::drawWorking(juce::Graphics& g, juce::Rectangle<float> r,
                                             const juce::String& label, float db, float fullScaleDb)
{
    auto name = r.removeFromLeft(44.0f);
    auto value = r.removeFromRight(38.0f);
    auto bar = r.reduced(4.0f, 0.0f);

    g.setFont(font(8.5f, true));
    g.setColour(pink.withAlpha(0.6f));
    g.drawText(label, name.toNearestInt(), juce::Justification::centredLeft);

    const float filled = juce::jlimit(0.0f, 1.0f, std::abs(db) / fullScaleDb);
    const float y = bar.getCentreY() - 2.5f;
    g.setColour(pink.withAlpha(0.13f));
    g.fillRoundedRectangle(bar.getX(), y, bar.getWidth(), 5.0f, 2.5f);
    if (filled > 0.005f) {
        g.setColour(pink.withAlpha(0.85f));
        g.fillRoundedRectangle(bar.getX(), y, bar.getWidth() * filled, 5.0f, 2.5f);
    }

    g.setFont(font(8.5f));
    g.setColour(juce::Colour(0xfff6c0e4));
    g.drawText(juce::String(db, 1), value.toNearestInt(), juce::Justification::centredRight);
}
void VoxeraAudioProcessorEditor::drawMascot(juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::Graphics::ScopedSaveState state(g);
    g.addTransform(juce::AffineTransform::scale(r.getWidth() / 100.0f, r.getHeight() / 100.0f).translated(r.getX(), r.getY()));
    g.addTransform(juce::AffineTransform::translation(0.0f, -voiceMotion * 4.0f));
    juce::Path horns; horns.startNewSubPath(19, 43); horns.quadraticTo(6, 30, 16, 10); horns.quadraticTo(19, 26, 34, 28);
    horns.lineTo(69, 28); horns.quadraticTo(87, 23, 89, 9); horns.quadraticTo(99, 31, 82, 43); horns.closeSubPath();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffa8df), 20, 15, juce::Colour(0xffae066b), 80, 85, false));
    g.fillPath(horns); g.fillEllipse(25, 55, 51, 40); g.fillEllipse(12, 27, 77, 56);
    const float blinkPhase = std::fmod(animationTime, 5.0f);
    const float eyeOpen = blinkPhase > 4.76f
        ? juce::jmax(0.08f, std::abs(blinkPhase - 4.88f) / 0.12f) : 1.0f;
    g.setColour(ink);
    g.fillEllipse(25.0f, 57.5f - 11.5f * eyeOpen, 18.0f, 23.0f * eyeOpen);
    g.fillEllipse(59.0f, 57.5f - 11.5f * eyeOpen, 18.0f, 23.0f * eyeOpen);
    if (eyeOpen > 0.6f) {
        g.setColour(juce::Colours::white);
        g.fillEllipse(29, 48, 5, 6); g.fillEllipse(63, 48, 5, 6);
    }
    juce::Path mouth; mouth.startNewSubPath(43, 71); mouth.quadraticTo(49, 78, 54, 71); mouth.quadraticTo(59, 77, 64, 70);
    g.setColour(ink); g.strokePath(mouth, juce::PathStrokeType(1.8f));
    g.setColour(pink); g.fillEllipse(25, 82, 21, 13); g.fillEllipse(57, 82, 21, 13);
}
void VoxeraAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / 1200.0f));
    g.fillAll(juce::Colour(0xffaaa9a6));
    juce::ColourGradient chassis(juce::Colour(0xfff3f1ed), 0, 0, juce::Colour(0xff8e8c8a), 1200, 840, false);
    chassis.addColour(0.42, silver); chassis.addColour(0.48, juce::Colour(0xffe0dedb));
    g.setGradientFill(chassis); g.fillRoundedRectangle(12, 12, 1176, 816, 30);
    g.setColour(juce::Colours::white.withAlpha(0.65f)); g.drawRoundedRectangle(16, 16, 1168, 808, 27, 2);
    for (int y = 20; y < 820; y += 3) {
        g.setColour(juce::Colours::black.withAlpha(y % 9 == 0 ? 0.026f : 0.012f));
        g.drawHorizontalLine(y, 25, 1175);
    }
    g.setColour(ink); g.setFont(font(10));
    g.drawText("V O C A L   P R O C E S S O R", 70, 28, 420, 22, juce::Justification::left);
    g.drawText("S I N G   L O U D E R    +    S O U N D   P R E T T I E R", 490, 28, 480, 22, juce::Justification::right);
    g.setColour(juce::Colour(0xff494548)); g.fillRoundedRectangle(43, 61, 1114, 478, 25);
    g.setColour(juce::Colour(0xffe7e4df)); g.drawRoundedRectangle(47, 65, 1106, 470, 23, 2);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff230e20), 600, 180, juce::Colour(0xff08070b), 600, 520, false));
    g.fillRoundedRectangle(63, 79, 1074, 438, 18);
    drawMeter(g, {95, 103, 130, 62}, inPeak, "IN");
    drawMeter(g, {1000, 103, 130, 62}, outPeak, "OUT");

    /*  Ten stages now act on the signal, most deciding for themselves how hard
        to work. Without this row the only way to tell which one is responsible
        for a sound is to bypass them one at a time.

        The strip sits between the tab row, which ends at y=117, and the logo,
        which starts at y=153; and between the IN and OUT meters, which own the
        panel out to x=225 and from x=1000. Everything here has to stay inside
        those bounds or it lands on top of something.
    */
    {
        // Cells are narrower than their spacing, so each figure has clear air
        // before the next label rather than running straight into it.
        const float x0 = 242.0f, w = 140.0f, pitch = 152.0f, y = 124.0f, h = 18.0f;
        drawWorking(g, {x0,                 y, w, h}, "GATE",    -processor.gateReductionDb(), 40.0f);
        drawWorking(g, {x0 + pitch,         y, w, h}, "OPT",     -processor.opticalReductionDb(), 12.0f);
        drawWorking(g, {x0 + 2.0f * pitch,  y, w, h}, "DENS",    processor.densityLiftDb(), 12.0f);
        drawWorking(g, {x0 + 3.0f * pitch,  y, w, h}, "LIMIT",   -processor.limiterReductionDb(), 6.0f);
        drawWorking(g, {x0 + 4.0f * pitch,  y, w, h}, "LOCK Hz", processor.lockMudHz(), 700.0f);
    }
    if (activePage != 4) {
    const juce::String logo("VOXERA");
    g.setFont(juce::Font(juce::FontOptions(110.0f, juce::Font::bold | juce::Font::italic)).withHorizontalScale(1.45f));
    for (int i = 7; i > 0; --i) {
        g.setColour(pink.withAlpha(0.02f)); g.drawText(logo, 243 - i, 153 - i, 714 + i * 2, 130 + i * 2, juce::Justification::centred);
    }
    g.setColour(pink); g.drawText(logo, 243, 153, 714, 130, juce::Justification::centred);
    juce::Path orbit; orbit.startNewSubPath(348, 257);
    orbit.cubicTo(186, 310, 802, 283, 887, 170);
    g.setColour(pink.withAlpha(0.15f)); g.strokePath(orbit, juce::PathStrokeType(7.0f));
    g.setColour(pink.withAlpha(0.8f)); g.strokePath(orbit, juce::PathStrokeType(1.4f));
    if (motion.getToggleState() && !bypass.getToggleState()) {
        // Follow the existing cubic orbit; no extra timer or path allocation.
        const float t = std::fmod(animationTime / 6.0f, 1.0f);
        const float u = 1.0f - t;
        const juce::Point<float> spark {
            u*u*u*348.0f + 3.0f*u*u*t*186.0f + 3.0f*u*t*t*802.0f + t*t*t*887.0f,
            u*u*u*257.0f + 3.0f*u*u*t*310.0f + 3.0f*u*t*t*283.0f + t*t*t*170.0f };
        const float alpha = std::sin(t * juce::MathConstants<float>::pi);
        g.setColour(pink.withAlpha(alpha * 0.18f));
        g.fillEllipse(spark.x - 7.0f, spark.y - 7.0f, 14.0f, 14.0f);
        g.setColour(juce::Colours::white.withAlpha(alpha * 0.9f));
        g.fillEllipse(spark.x - 2.0f, spark.y - 2.0f, 4.0f, 4.0f);
    }
    juce::Path star; star.startNewSubPath(911, 144); star.quadraticTo(914, 163, 928, 167);
    star.quadraticTo(914, 169, 911, 185); star.quadraticTo(908, 170, 894, 167);
    star.quadraticTo(908, 164, 911, 144); star.closeSubPath();
    g.setColour(pink); g.fillPath(star);
    juce::Path wave; const int head = processor.scopeHead.load(std::memory_order_relaxed);
    for (int i = 0; i < 256; ++i) {
        const float sample = processor.scope[static_cast<size_t>((head + i) % 256)].load(std::memory_order_relaxed);
        const float x = 95.0f + static_cast<float>(i) * 1010.0f / 255.0f;
        const float y = 299.0f - juce::jlimit(-1.0f, 1.0f, sample * 2.0f) * 27.0f;
        if (i == 0) wave.startNewSubPath(x, y); else wave.lineTo(x, y);
    }
    g.setColour(pink.withAlpha(0.12f)); g.strokePath(wave, juce::PathStrokeType(5));
    g.setColour(pink.withAlpha(0.7f)); g.strokePath(wave, juce::PathStrokeType(1));
    g.setFont(font(11)); g.setColour(juce::Colour(0xfff6c0e4));
    const float hz = processor.detectedHz();
    const juce::String readout = processor.pitchConfidence() >= 0.62f && hz > 0.0f
        ? juce::String(hz, 1) + " Hz  >  " + juce::String(processor.targetHz(), 1) + " Hz     SHIFT " + juce::String(processor.shiftSemitones(), 2) + " st"
        : "H U M A N   V O I C E   F U R T H E R";
    g.drawText(readout, 300, 316, 600, 20, juce::Justification::centred);
    } else {
        g.setColour(pink); g.setFont(font(20, true));
        g.drawText("SMART EQ / ADAPTIVE CUTS", 280, 143, 640, 28, juce::Justification::centred);
        const char* bands[] = {"250 Hz", "500 Hz", "1 kHz", "2 kHz", "4 kHz"};
        for (size_t b = 0; b < 5; ++b) {
            const float x = 190.0f + static_cast<float>(b) * 175.0f;
            const float gain = processor.smartEQGain(b);
            g.setColour(pink.withAlpha(0.13f)); g.fillRoundedRectangle(x, 189, 120, 86, 5);
            g.setColour(pink.withAlpha(0.8f));
            g.fillRoundedRectangle(x, 189, 120, juce::jmax(1.0f, -gain * 86.0f / 6.0f), 5);
            g.setFont(font(13, true)); g.setColour(juce::Colour(0xfff6c0e4));
            g.drawText(juce::String(gain, 2) + " dB", static_cast<int>(x), 278, 120, 20, juce::Justification::centred);
            g.setFont(font(11)); g.drawText(bands[b], static_cast<int>(x), 301, 120, 20, juce::Justification::centred);
        }
        g.setFont(font(11)); g.setColour(pink);
        g.drawText("Raise Amount to enable  /  Linked stereo  /  Shared cut budget", 200, 323, 800, 20, juce::Justification::centred);
    }
    if (activePage == 0) {
        g.setColour(pink.withAlpha(0.8f)); g.setFont(font(11));
        if (processor.autoMixReport().isEmpty())
            g.drawText(processor.profileReady.load() ? "PROFILE READY" : "Sing for 8s, then raise Auto Voice", 310, 494, 290, 20, juce::Justification::left);

        /*  Auto Mix moves twenty controls at once. Saying what it concluded, in
            the same words a mixer would use, is what lets the singer disagree
            with it rather than guess which knob to undo.

            Only the lines that fit are drawn here. The band below the knobs and
            above the divider is all the free space this page has, so the full
            reasoning lives in the button's tooltip instead of being crammed in.
        */
        const auto& report = processor.autoMixReport();
        if (report.isNotEmpty()) {
            g.setColour(pink.withAlpha(0.10f));
            g.fillRoundedRectangle(310, 494, 780, 50, 5);
            g.setFont(font(9.0f));
            g.setColour(juce::Colour(0xfff6c0e4));
            juce::StringArray lines;
            lines.addLines(report);
            lines.removeEmptyStrings();
            for (int i = 0; i < juce::jmin(4, lines.size()); ++i)
                g.drawText(lines[i].trim(), 320, 497 + i * 11, 760, 11, juce::Justification::left);
        }
    }
    g.setColour(ink.withAlpha(0.45f)); g.drawHorizontalLine(548, 24, 1176);
    g.setFont(font(10)); g.setColour(ink);
    const char* captions[] = {"CORRECTION  0 - 100%", "DARK  /  BRIGHT", "AIR GAIN  dB", "REVERB AMOUNT", "DRY  /  PROCESSED"};
    for (int i = 0; i < 5; ++i) g.drawText(captions[i], 187 + i * 176, 772, 162, 18, juce::Justification::centred);
    drawMascot(g, {1070, 527, 100, 100});
    g.drawText("GOOD", 65, 719, 100, 15, juce::Justification::centred);
    g.drawText("VOCALS", 65, 735, 100, 15, juce::Justification::centred);
    g.drawText("ONLY", 65, 751, 100, 15, juce::Justification::centred);
    g.setColour(ink.withAlpha(0.55f)); g.drawHorizontalLine(801, 45, 1155);
    g.setFont(font(10)); g.setColour(ink);
    g.drawText("VOXERA AUDIO LABS", 60, 803, 250, 23, juce::Justification::left);
    g.drawText("A MORE EXPRESSIVE YOU", 420, 803, 360, 23, juce::Justification::centred);
    g.drawText("v" JucePlugin_VersionString "  |  CHOP", 880, 803, 265, 23, juce::Justification::right);
}

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
    control->label.setFont(font(large ? 17.0f : 13.0f, true));
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
    addControl("clean", "CLEAN", 0);
    addControl("deEss", "DE-ESS", 0);
    addControl("voiceMatch", "VOICE MATCH", 0);
    addControl("compThreshold", "THRESHOLD", 2);
    addControl("compRatio", "RATIO", 2);
    addControl("compAttack", "ATTACK", 2);
    addControl("compRelease", "RELEASE", 2);
    addControl("optical", "OPTO LEVEL", 2);
    addControl("density", "DENSITY", 2);

    /*  Three controls that the chain has and the interface did not.

        Every parameter is reachable through the generic ADVANCED list, so
        nothing here was unreachable — but a list of seventy names is where a
        control goes to be never found. The exciter is the clearest case: it is
        the stage that makes a voice sound open rather than merely bright, and
        somebody looking for it on the page called "colour" concluded it did
        not work.

        Added at the end deliberately. The grid below places controls by index,
        so inserting anywhere else would silently move every knob after it.
    */
    addControl("exciter", "EXCITER", 1);
    addControl("satWarmth", "WARMTH", 1);
    addControl("punch", "PUNCH", 2);
    const char* tabNames[] = {"VOCALS", "FX", "DYNAMICS", "ADVANCED", "SMART EQ", "CHOP"};
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
    for (int i = 0; i < VoxeraAudioProcessor::numFactoryPresets; ++i)
        presetPicker.addItem(processor.getProgramName(i), i + 1);
    listenEss.onStateChange = [this] { processor.listenDeEss.store(listenEss.isDown()); };
    listenEss.setTooltip("Hold to hear only what the de-esser removes. Release to return to your vocal.");
    addAndMakeVisible(listenEss);
    presetPicker.setTextWhenNothingSelected("Choose a starting sound");
    presetPicker.setName("Starting sound");
    presetPicker.onChange = [this] { processor.applyFactoryPreset(presetPicker.getSelectedId() - 1); };
    addAndMakeVisible(presetPicker);
    mixReport.setMultiLine(true); mixReport.setReadOnly(true);
    mixReport.setScrollbarsShown(true); mixReport.setCaretVisible(false);
    mixReport.setFont(font(14.0f));
    mixReport.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff211a24));
    mixReport.setColour(juce::TextEditor::textColourId, juce::Colour(0xffeee6ef));
    mixReport.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    mixReport.setName("Auto Mix result"); addAndMakeVisible(mixReport);
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
                            "after you choose the control and its direction in this menu.");
    loadModel.setTooltip("Loads a Neural Amp Modeler capture (.nam) or an RTNeural model (.json) "
                         "and runs it where a preamp would sit. Captures of microphone preamps and "
                         "console channels are what suit a voice. NAM LSTM and WaveNet up to 12 channels "
                         "are supported. Nothing is bundled, so the "
                         "capture is yours to provide or to make.");
    bypass.setClickingTogglesState(true);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "bypass", bypass);
    lowLatency.setClickingTogglesState(true);
    lowLatency.setTooltip("For singing through the plugin. Takes the pitch shifter out of the "
                          "path. The latency display shows the current total, including any inserted plugin. Only the tuning "
                          "stops. Turn it off again to mix.");
    lowLatencyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "lowLatency", lowLatency);
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
    processor.listenDeEss.store(false);
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
    processor.listenDeEss.store(false);
    listenEss.setVisible(page == 0);
    for (auto& c : controls) {
        const bool visible = c->page < 0 || c->page == page;
        c->slider.setVisible(visible); c->label.setVisible(visible);
    }
    for (size_t i = 0; i < choices.size(); ++i) {
        const bool visible = i < 3 ? page == 0 : page == 5;
        choices[i].combo.setVisible(visible); choices[i].label.setVisible(visible);
    }
    for (auto& b : presets) b.setVisible(false);
    analyze.setVisible(page == 4); learnVoice.setVisible(page == 4);
    loadModel.setVisible(page == 2); insertButton.setVisible(page == 2);
    compressorChoice.combo.setVisible(page == 2); compressorChoice.label.setVisible(page == 2);
    advancedViewport.setVisible(page == 3);
    for (int i = 0; i < 6; ++i) tabs[static_cast<size_t>(i)].setToggleState(i == page, juce::dontSendNotification);
    resized(); repaint();
}
void VoxeraAudioProcessorEditor::resized()
{
    const float scale = static_cast<float>(getWidth()) / 1200.0f;
    auto place = [scale](juce::Component& c, int x, int y, int w, int h) {
        c.setBounds((juce::Rectangle<float>(float(x), float(y), float(w), float(h)) * scale).toNearestInt());
    };
    place(listenEss, 938, 145, 204, 30);
    place(presetPicker, 308, 27, 260, 36);
    place(lowLatency, 594, 27, 140, 36); place(bypass, 746, 27, 100, 36);
    place(savePreset, 818, 796, 158, 28); place(loadPreset, 990, 796, 158, 28);
    for (int i = 0; i < 6; ++i) place(tabs[size_t(i)], 300 + i*146, 94, 138, 38);
    place(autoMix, 44, 173, 220, 50);
    place(mixOptions, 44, 234, 220, 34);
    place(compareMix, 44, 280, 106, 34); place(undoMix, 158, 280, 106, 34);
    place(matchLevel, 44, 326, 220, 34);
    place(mixReport, 44, 378, 220, 170);
    for (int i = 0; i < 5; ++i) {
        auto& c = *controls[size_t(i)];
        place(c.label, 55+i*230, 607, 170, 24);
        place(c.slider, 55+i*230, 637, 170, 135);
    }
    for (int i = 0; i < 3; ++i) {
        auto& c = choices[size_t(i)];
        place(c.label, 326+i*278, 182, 248, 22);
        place(c.combo, 326+i*278, 208, 248, 36);
    }
    // The same grid gives every control its own label, gesture area and value.
    auto knob = [&](int index, int column, int row, int columns=3) {
        const int width = 816/columns;
        auto& c = *controls[size_t(index)];
        place(c.label, 326+column*width, 274+row*146, width-20, 22);
        place(c.slider, 326+column*width, 300+row*146, width-20, 110);
    };
    for (int i=0; i<3; ++i) { knob(5+i,i,0); knob(25+i,i,1); }
    // FX in four columns rather than three, which is what makes room for the
    // exciter and the warmth control without a third row crowding the meters.
    for (int i=0; i<6; ++i) knob(8+i,i%4,i/4,4);
    knob(34,2,1,4); knob(35,3,1,4);
    for (int i=0; i<3; ++i) knob(14+i,i,1);
    for (int i=0; i<3; ++i) {
        auto& c=choices[size_t(3+i)];
        place(c.label,326+i*278,182,248,22); place(c.combo,326+i*278,208,248,36);
    }
    for (int i=0; i<7; ++i) knob(17+i,i%4,i/4,4);
    place(compressorChoice.label,326,178,242,22);
    place(compressorChoice.combo,326,208,242,36);
    place(loadModel,588,208,260,36); place(insertButton,868,208,274,36);
    knob(24,0,0,4);
    for (int i=0; i<6; ++i) knob(28+i,(i+1)%4,(i+1)/4,4);
    knob(36,3,1,4);
    place(analyze,326,206,240,38); place(learnVoice,588,206,240,38);
    place(advancedViewport,320,186,832,376);
    advancedEditor->setSize(advancedViewport.getWidth()-18, advancedViewport.getHeight());
}
void VoxeraAudioProcessorEditor::timerCallback()
{
    if (hostedLease && hostedLease != processor.insertEditorLease()) closeInsertWindow();
    const juce::String report = processor.autoMixReport().isEmpty()
        ? "Play a vocal phrase, then press Auto Mix.\n\nListen for 8 seconds. Compare the result and adjust any control.\n\nUse LOW LATENCY while recording. Tuning resumes when it is off."
        : processor.autoMixReport();
    if (mixReport.getText() != report) mixReport.setText(report, false);
    autoMix.setButtonText(processor.capturing.load() ? "LISTENING " + juce::String(int(processor.captureProgress.load()*100)) + "%" : "AUTO MIX (8s)");
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
        juce::PopupMenu mapping;
        mapping.addItem(10000, "Keep external settings unchanged", true, processor.insertAmountMapping() < 0);
        const auto names = processor.insertParameterNames();
        for (int i = 0; i < names.size(); ++i)
            if (!voxera::PluginSlot::isBoilerplate(names[i]))
                mapping.addItem(10001+i, names[i], true, processor.insertAmountMapping()==i);
        menu.addSubMenu("Auto Mix control (choose explicitly)", mapping);
        menu.addItem(-4, "More compression = lower value", true, processor.insertAmountIsReversed());
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(insertButton),
        [safe = juce::Component::SafePointer<VoxeraAudioProcessorEditor>(this), found](int choice) {
            if (safe == nullptr || choice == 0) return;

            if (choice >= 10000) { safe->processor.mapInsertAmount(choice-10001, safe->processor.insertAmountIsReversed()); return; }
            if (choice == -4) { safe->processor.mapInsertAmount(safe->processor.insertAmountMapping(), !safe->processor.insertAmountIsReversed()); return; }
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

    hostedLease = processor.insertEditorLease();
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
    if (insertWindow == nullptr) { hostedLease.reset(); return; }
    // Simply released: the window owns the editor, so this destroys it, and the
    // editor's destructor is what tells the plugin it no longer has one.
    // Clearing the content first was the fault — it detached without deleting.
    insertWindow.reset();
    hostedLease.reset();
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

            if (choice >= 10000) { safe->processor.mapInsertAmount(choice-10001, safe->processor.insertAmountIsReversed()); return; }
            if (choice == -4) { safe->processor.mapInsertAmount(safe->processor.insertAmountMapping(), !safe->processor.insertAmountIsReversed()); return; }
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

    g.setFont(font(12.0f, true));
    g.setColour(pink.withAlpha(0.9f));
    g.drawText(label, name.toNearestInt(), juce::Justification::centredLeft);

    const float filled = juce::jlimit(0.0f, 1.0f, std::abs(db) / fullScaleDb);
    const float y = bar.getCentreY() - 2.5f;
    g.setColour(pink.withAlpha(0.13f));
    g.fillRoundedRectangle(bar.getX(), y, bar.getWidth(), 5.0f, 2.5f);
    if (filled > 0.005f) {
        g.setColour(pink.withAlpha(0.85f));
        g.fillRoundedRectangle(bar.getX(), y, bar.getWidth() * filled, 5.0f, 2.5f);
    }

    g.setFont(font(12.0f));
    g.setColour(juce::Colour(0xfff6c0e4));
    /*  Negative zero is still zero, and it should not be shown as "-0.0".

        The reduction meters negate a positive figure for display, so a stage
        doing nothing produced a minus sign attached to nothing — visible on
        two of the four meters at rest. Tiny, and exactly the sort of thing
        that makes a plugin feel unfinished before anyone has heard it.
    */
    const float shown = std::abs(db) < 0.05f ? 0.0f : db;
    g.drawText(juce::String(shown, 1), value.toNearestInt(), juce::Justification::centredRight);
}
void VoxeraAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::scale(float(getWidth()) / 1200.0f));
    g.fillAll(juce::Colour(0xffc9c8c6));
    g.setColour(ink); g.fillRoundedRectangle(16, 16, 1168, 568, 18);
    g.setColour(juce::Colour(0xff211a24)); g.fillRoundedRectangle(28, 94, 252, 474, 12);
    g.setColour(pink); g.setFont(juce::Font(juce::FontOptions(40.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText("VOXERA", 40, 24, 244, 45, juce::Justification::left);
    g.setFont(font(13)); g.setColour(silver);
    g.drawText("YOUR VOICE. YOUR SOUND.",40,69,240,18,juce::Justification::left);
    drawMeter(g,{878,24,126,54},inPeak,"IN"); drawMeter(g,{1024,24,126,54},outPeak,"OUT");
    g.setFont(font(20,true)); g.setColour(juce::Colour(0xfff2eaf2));
    g.drawText("Start with your voice",44,112,220,28,juce::Justification::left);
    g.setFont(font(13)); g.setColour(silver);
    g.drawText("Play  >  Listen  >  Make it yours",44,143,220,20,juce::Justification::left);
    const char* titles[] = {"Tuning & cleanup", "Width, echo & colour", "Compression & analogue colour", "Every control, when you need it", "Adaptive tone correction", "Rhythm & character"};
    g.setColour(juce::Colour(0xfff2eaf2)); g.setFont(font(18,true));
    g.drawText(titles[activePage],326,146,820,28,juce::Justification::left);
    if (activePage==1) {
        g.setColour(silver); g.setFont(font(14));
        g.drawText("Add space around the lead. Keep the words in front.",326,207,810,36,juce::Justification::left);
    }
    if (activePage==4) {
        const char* bands[]={"250 Hz","500 Hz","1 kHz","2 kHz","4 kHz"};
        for (int i=0;i<5;++i) {
            const float x=326.0f+i*164.0f, gain=processor.smartEQGain(size_t(i));
            g.setColour(pink.withAlpha(0.12f)); g.fillRoundedRectangle(x,282,138,84,5);
            g.setColour(pink.withAlpha(0.65f)); g.fillRoundedRectangle(x,282,138,juce::jlimit(1.0f,84.0f,-gain*14),5);
            g.setColour(silver); g.setFont(font(14));
            g.drawText(juce::String(bands[i])+"  /  "+juce::String(gain,1)+" dB",int(x),375,146,22,juce::Justification::centred);
        }
    }
    // Live metering has one quiet, dedicated strip, outside all controls.
    drawWorking(g,{326,564,146,16},"GATE",-processor.gateReductionDb(),40);
    drawWorking(g,{492,564,146,16},"COMP",-processor.compressorReductionDb(),12);
    drawWorking(g,{658,564,146,16},"DE-ESS",-processor.deEssReductionDb(),7);
    drawWorking(g,{824,564,146,16},"LIMIT",-processor.limiterReductionDb(),6);
    g.setColour(silver); g.setFont(font(12));
    const double rate=processor.getSampleRate();
    g.drawText(juce::String(rate>0 ? processor.reportedLatencySamples()*1000.0/rate : 0.0,1)+" ms",990,562,158,20,juce::Justification::right);
    const char* captions[]={"PITCH CORRECTION","DARK / BRIGHT","HIGH FREQUENCIES","REVERB","DRY / PROCESSED"};
    g.setColour(ink.withAlpha(0.7f)); g.setFont(font(12));
    for (int i=0;i<5;++i) g.drawText(captions[i],55+i*230,773,170,18,juce::Justification::centred);
    g.setColour(ink.withAlpha(0.18f)); g.drawHorizontalLine(791,40,1160);
    g.setColour(ink); g.setFont(font(12));
    g.drawText("HOME STUDIO  /  v" JucePlugin_VersionString,44,798,680,24,juce::Justification::left);
}

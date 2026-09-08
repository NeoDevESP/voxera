#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/RealtimeUtilities.h"

namespace ParamIDs
{
    static constexpr auto inputDb = "inputDb";
    static constexpr auto autoGain = "autoGain";
    static constexpr auto targetDb = "targetDb";

    static constexpr auto pitchOn = "pitchOn";
    static constexpr auto pitchKey = "pitchKey";
    static constexpr auto pitchScale = "pitchScale";
    static constexpr auto pitchMode = "pitchMode";
    static constexpr auto pitchEngine = "pitchEngine";
    static constexpr auto tuneAmount = "tuneAmount";
    static constexpr auto retune = "retune";
    static constexpr auto humanize = "humanize";
    static constexpr auto formant = "formant";
    static constexpr auto analyzeVoice = "analyzeVoice";
    static constexpr auto autoVoice = "autoVoice";

    static constexpr auto spectralOn = "spectralOn";
    static constexpr auto clean = "clean";
    static constexpr auto bodyDb = "bodyDb";
    static constexpr auto presenceDb = "presenceDb";
    static constexpr auto deEss = "deEss";
    static constexpr auto airDb = "airDb";

    static constexpr auto compThreshold = "compThreshold";
    static constexpr auto compRatio = "compRatio";
    static constexpr auto compAttack = "compAttack";
    static constexpr auto compRelease = "compRelease";
    static constexpr auto compSidechain = "compSidechain";
    static constexpr auto compMix = "compMix";

    static constexpr auto satDrive = "satDrive";
    static constexpr auto satMix = "satMix";

    static constexpr auto spatialOn = "spatialOn";
    static constexpr auto width = "width";
    static constexpr auto doubler = "doubler";
    static constexpr auto delayMix = "delayMix";
    static constexpr auto delayFeedback = "delayFeedback";
    static constexpr auto delayDivision = "delayDivision";
    static constexpr auto space = "space";
    static constexpr auto duck = "duck";

    static constexpr auto outputDb = "outputDb";

    static constexpr auto toneMacro = "toneMacro";
    static constexpr auto globalMix = "globalMix";
    static constexpr auto bypass = "bypass";
    static constexpr auto smartEQAmount = "smartEQAmount";
    static constexpr auto smartEQRange = "smartEQRange";
    static constexpr auto smartEQResponse = "smartEQResponse";

    static constexpr auto limiterOn = "limiterOn";
    static constexpr auto limiterCeiling = "limiterCeiling";

    static constexpr auto punch = "punch";
    static constexpr auto exciter = "exciter";

    static constexpr auto gateOn = "gateOn";
    static constexpr auto gateThreshold = "gateThreshold";
    static constexpr auto optical = "optical";
    static constexpr auto clipAmount = "clipAmount";
    static constexpr auto character = "character";
    static constexpr auto lowLatency = "lowLatency";
    static constexpr auto density = "density";
    static constexpr auto vocalLock = "vocalLock";
    static constexpr auto satWarmth = "satWarmth";
    static constexpr auto reverbBody = "reverbBody";
    static constexpr auto reverbAir = "reverbAir";

    static constexpr auto chopAmount = "chopAmount";
    static constexpr auto chopDivision = "chopDivision";
    static constexpr auto chopPattern = "chopPattern";
    static constexpr auto crush = "crush";
    static constexpr auto crushMix = "crushMix";
    static constexpr auto modType = "modType";
    static constexpr auto modRate = "modRate";
    static constexpr auto modDepth = "modDepth";
    static constexpr auto modMix = "modMix";
    static constexpr auto glue = "glue";
    static constexpr auto neuralMix = "neuralMix";
    static constexpr auto compType = "compType";
    static constexpr auto voiceMatch = "voiceMatch";
}

VoxeraAudioProcessor::VoxeraAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    bindParameters();
}

void VoxeraAudioProcessor::bindParameters()
{
    const auto bind = [this](const char* id)
    {
        auto* raw = apvts.getRawParameterValue(id);
        jassert(raw != nullptr);
        return raw;
    };

    prm.inputDb = bind(ParamIDs::inputDb);
    prm.autoGain = bind(ParamIDs::autoGain);
    prm.targetDb = bind(ParamIDs::targetDb);
    prm.pitchOn = bind(ParamIDs::pitchOn);
    prm.pitchKey = bind(ParamIDs::pitchKey);
    prm.pitchScale = bind(ParamIDs::pitchScale);
    prm.pitchMode = bind(ParamIDs::pitchMode);
    prm.pitchEngine = bind(ParamIDs::pitchEngine);
    prm.tuneAmount = bind(ParamIDs::tuneAmount);
    prm.retune = bind(ParamIDs::retune);
    prm.humanize = bind(ParamIDs::humanize);
    prm.formant = bind(ParamIDs::formant);
    prm.analyzeVoice = bind(ParamIDs::analyzeVoice);
    prm.autoVoice = bind(ParamIDs::autoVoice);
    prm.spectralOn = bind(ParamIDs::spectralOn);
    prm.clean = bind(ParamIDs::clean);
    prm.bodyDb = bind(ParamIDs::bodyDb);
    prm.presenceDb = bind(ParamIDs::presenceDb);
    prm.deEss = bind(ParamIDs::deEss);
    prm.airDb = bind(ParamIDs::airDb);
    prm.compThreshold = bind(ParamIDs::compThreshold);
    prm.compRatio = bind(ParamIDs::compRatio);
    prm.compAttack = bind(ParamIDs::compAttack);
    prm.compRelease = bind(ParamIDs::compRelease);
    prm.compSidechain = bind(ParamIDs::compSidechain);
    prm.compMix = bind(ParamIDs::compMix);
    prm.satDrive = bind(ParamIDs::satDrive);
    prm.satMix = bind(ParamIDs::satMix);
    prm.spatialOn = bind(ParamIDs::spatialOn);
    prm.width = bind(ParamIDs::width);
    prm.doubler = bind(ParamIDs::doubler);
    prm.delayMix = bind(ParamIDs::delayMix);
    prm.delayFeedback = bind(ParamIDs::delayFeedback);
    prm.delayDivision = bind(ParamIDs::delayDivision);
    prm.space = bind(ParamIDs::space);
    prm.duck = bind(ParamIDs::duck);
    prm.outputDb = bind(ParamIDs::outputDb);
    prm.toneMacro = bind(ParamIDs::toneMacro);
    prm.globalMix = bind(ParamIDs::globalMix);
    prm.bypass = bind(ParamIDs::bypass);
    prm.smartEQAmount = bind(ParamIDs::smartEQAmount);
    prm.smartEQRange = bind(ParamIDs::smartEQRange);
    prm.smartEQResponse = bind(ParamIDs::smartEQResponse);
    prm.limiterOn = bind(ParamIDs::limiterOn);
    prm.limiterCeiling = bind(ParamIDs::limiterCeiling);
    prm.punch = bind(ParamIDs::punch);
    prm.exciter = bind(ParamIDs::exciter);
    prm.gateOn = bind(ParamIDs::gateOn);
    prm.gateThreshold = bind(ParamIDs::gateThreshold);
    prm.optical = bind(ParamIDs::optical);
    prm.clipAmount = bind(ParamIDs::clipAmount);
    prm.character = bind(ParamIDs::character);
    prm.lowLatency = bind(ParamIDs::lowLatency);
    prm.density = bind(ParamIDs::density);
    prm.vocalLock = bind(ParamIDs::vocalLock);
    prm.satWarmth = bind(ParamIDs::satWarmth);
    prm.reverbBody = bind(ParamIDs::reverbBody);
    prm.reverbAir = bind(ParamIDs::reverbAir);
    prm.chopAmount = bind(ParamIDs::chopAmount);
    prm.chopDivision = bind(ParamIDs::chopDivision);
    prm.chopPattern = bind(ParamIDs::chopPattern);
    prm.crush = bind(ParamIDs::crush);
    prm.crushMix = bind(ParamIDs::crushMix);
    prm.modType = bind(ParamIDs::modType);
    prm.modRate = bind(ParamIDs::modRate);
    prm.modDepth = bind(ParamIDs::modDepth);
    prm.modMix = bind(ParamIDs::modMix);
    prm.glue = bind(ParamIDs::glue);
    prm.neuralMix = bind(ParamIDs::neuralMix);
    prm.compType = bind(ParamIDs::compType);
    prm.voiceMatch = bind(ParamIDs::voiceMatch);
}

juce::File VoxeraAudioProcessor::neuralModelFolder()
{
    auto folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                      .getChildFile("VOXERA").getChildFile("Models");
    folder.createDirectory();
    return folder;
}

juce::Array<juce::File> VoxeraAudioProcessor::availableNeuralModels()
{
    juce::Array<juce::File> found;
    neuralModelFolder().findChildFiles(found, juce::File::findFiles, true, "*.nam;*.json");
    // Alphabetical, so the list does not reshuffle itself between sessions on
    // whatever order the filesystem happens to return.
    std::sort(found.begin(), found.end(), [](const juce::File& a, const juce::File& b) {
        return a.getFileNameWithoutExtension().compareIgnoreCase(b.getFileNameWithoutExtension()) < 0;
    });
    return found;
}

voxera::NeuralStage::LoadResult VoxeraAudioProcessor::loadNeuralModel(const juce::File& file)
{
    auto result = neural.load(file);
    if (!result.ok) return result;

    const double session = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;

    suspendProcessing(true);
    neural.commitLoad();
    neural.prepare(session, preparedBlockSize, getTotalNumOutputChannels());
    suspendProcessing(false);

    loadedNeuralFile = file;

    // Reported rather than warned about: the stage now runs the network at its
    // own rate whatever the session is doing, so this is information about what
    // is happening and not a caveat about it sounding wrong.
    if (result.modelSampleRate > 0.0 && std::abs(result.modelSampleRate - session) > 1.0)
        result.message += " — captured at " + juce::String(result.modelSampleRate, 0)
                        + " Hz and resampled to run at that rate in a "
                        + juce::String(session, 0) + " Hz session.";

    return result;
}

void VoxeraAudioProcessor::unloadNeuralModel()
{
    suspendProcessing(true);
    neural.unload();
    suspendProcessing(false);
    loadedNeuralFile = {};
}

/*  Parameters are grouped so hosts and the advanced page show a structured tree
    instead of forty flat entries. Declaration order is unchanged from 0.8.0, so
    parameter indices and IDs — and therefore existing automation — still match.
    New parameters are appended at the end for the same reason.
*/
juce::AudioProcessorValueTreeState::ParameterLayout VoxeraAudioProcessor::createParameterLayout()
{
    using Group = juce::AudioProcessorParameterGroup;

    const auto group = [](const char* id, const char* name, auto&&... members)
    {
        return std::make_unique<Group>(id, name, "|", std::move(members)...);
    };
    const auto number = [](const char* id, const char* name,
                           juce::NormalisableRange<float> range, float initial)
    {
        return std::make_unique<juce::AudioParameterFloat>(id, name, range, initial);
    };
    const auto toggle = [](const char* id, const char* name, bool initial)
    {
        return std::make_unique<juce::AudioParameterBool>(id, name, initial);
    };
    const auto choice = [](const char* id, const char* name, juce::StringArray options, int initial)
    {
        return std::make_unique<juce::AudioParameterChoice>(id, name, std::move(options), initial);
    };

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(
        group("io", "Input",
            number(ParamIDs::inputDb, "Input", { -24.0f, 24.0f, 0.1f }, 0.0f),
            toggle(ParamIDs::autoGain, "Auto Gain", true),
            number(ParamIDs::targetDb, "Auto Gain Target", { -30.0f, -10.0f, 0.1f }, -18.0f)),

        group("pitch", "Pitch",
            toggle(ParamIDs::pitchOn, "Pitch Correction", true),
            choice(ParamIDs::pitchKey, "Key",
                { "C", "C#/Db", "D", "D#/Eb", "E", "F",
                  "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" }, 0),
            choice(ParamIDs::pitchScale, "Scale",
                { "Chromatic", "Major", "Natural Minor", "Harmonic Minor", "Dorian" }, 0),
            choice(ParamIDs::pitchMode, "Tune Mode", { "Natural", "Modern", "Hard" }, 1),
            /*  PSOLA leads because it measures better on every axis that was
                compared — a third less latency, a tenth of the processing, and
                tuning accurate to under a cent where the other engine's is
                limited by its frame size. Rubber Band stays available because
                it is the general one: it will shift material that has more
                than one voice in it, or a room around the voice, which the
                time-domain engine cannot and does not pretend to.
            */
            choice(ParamIDs::pitchEngine, "Pitch Engine", { "PSOLA", "Rubber Band" }, 0),
            number(ParamIDs::tuneAmount, "Tune Amount", { 0.0f, 100.0f, 0.1f }, 100.0f),
            number(ParamIDs::retune, "Retune", { 0.0f, 100.0f, 0.1f }, 65.0f),
            number(ParamIDs::humanize, "Humanize", { 0.0f, 100.0f, 0.1f }, 35.0f),
            number(ParamIDs::formant, "Formant", { -12.0f, 12.0f, 0.1f }, 0.0f),
            toggle(ParamIDs::analyzeVoice, "Analyze Voice (8s)", false),
            number(ParamIDs::autoVoice, "Auto Voice", { 0.0f, 100.0f, 0.1f }, 0.0f)),

        group("spectral", "Spectral",
            toggle(ParamIDs::spectralOn, "Adaptive Spectral Engine", true),
            number(ParamIDs::clean, "Clean", { 0.0f, 100.0f, 0.1f }, 55.0f),
            number(ParamIDs::bodyDb, "Body", { -6.0f, 6.0f, 0.1f }, 0.0f),
            number(ParamIDs::presenceDb, "Presence", { -6.0f, 6.0f, 0.1f }, 0.0f),
            number(ParamIDs::deEss, "De-Ess", { 0.0f, 100.0f, 0.1f }, 55.0f),
            number(ParamIDs::airDb, "Air", { -8.0f, 8.0f, 0.1f }, 0.0f)),

        group("dynamics", "Dynamics",
            number(ParamIDs::compThreshold, "Comp Threshold", { -48.0f, 0.0f, 0.1f }, -18.0f),
            number(ParamIDs::compRatio, "Comp Ratio", { 1.0f, 20.0f, 0.1f, 0.5f }, 3.0f),
            number(ParamIDs::compAttack, "Comp Attack", { 0.1f, 100.0f, 0.1f, 0.35f }, 8.0f),
            number(ParamIDs::compRelease, "Comp Release", { 10.0f, 500.0f, 1.0f, 0.5f }, 90.0f),
            number(ParamIDs::compSidechain, "Comp Sidechain HPF", { 20.0f, 400.0f, 1.0f, 0.5f }, 85.0f),
            number(ParamIDs::compMix, "Comp Mix", { 0.0f, 100.0f, 0.1f }, 100.0f)),

        group("saturation", "Saturation",
            number(ParamIDs::satDrive, "Saturation Drive", { 0.0f, 24.0f, 0.1f }, 4.0f),
            number(ParamIDs::satMix, "Saturation Mix", { 0.0f, 100.0f, 0.1f }, 15.0f)),

        group("spatial", "Spatial",
            toggle(ParamIDs::spatialOn, "Spatial Engine", true),
            number(ParamIDs::width, "Width", { 0.0f, 100.0f, 0.1f }, 65.0f),
            number(ParamIDs::doubler, "Double", { 0.0f, 100.0f, 0.1f }, 22.0f),
            number(ParamIDs::delayMix, "Delay", { 0.0f, 100.0f, 0.1f }, 12.0f),
            number(ParamIDs::delayFeedback, "Delay Feedback", { 0.0f, 100.0f, 0.1f }, 34.0f),
            choice(ParamIDs::delayDivision, "Delay Division",
                { "1/8", "1/4", "1/4 Dotted", "1/8 Triplet", "1/2" }, 1),
            number(ParamIDs::space, "Space", { 0.0f, 100.0f, 0.1f }, 18.0f),
            number(ParamIDs::duck, "Spatial Ducking", { 0.0f, 100.0f, 0.1f }, 65.0f)),

        group("master", "Output",
            number(ParamIDs::outputDb, "Output", { -24.0f, 24.0f, 0.1f }, 0.0f),
            number(ParamIDs::toneMacro, "Tone", { -100.0f, 100.0f, 0.1f }, 0.0f),
            number(ParamIDs::globalMix, "Global Mix", { 0.0f, 100.0f, 0.1f }, 100.0f),
            toggle(ParamIDs::bypass, "Bypass", false)),

        group("smarteq", "Smart EQ",
            number(ParamIDs::smartEQAmount, "Smart EQ Amount", { 0.0f, 100.0f, 0.1f }, 0.0f),
            number(ParamIDs::smartEQRange, "Smart EQ Budget dB", { 1.0f, 6.0f, 0.1f }, 3.0f),
            number(ParamIDs::smartEQResponse, "Smart EQ Response ms", { 100.0f, 1000.0f, 1.0f }, 250.0f)),

        group("limiter", "Limiter",
            toggle(ParamIDs::limiterOn, "Limiter", true),
            number(ParamIDs::limiterCeiling, "Ceiling", { -6.0f, 0.0f, 0.1f }, -0.3f)),

        group("character", "Character",
            number(ParamIDs::punch, "Punch", { 0.0f, 100.0f, 0.1f }, 0.0f),
            number(ParamIDs::exciter, "Exciter", { 0.0f, 100.0f, 0.1f }, 0.0f)),

        group("gate", "Gate",
            toggle(ParamIDs::gateOn, "Gate", true),
            number(ParamIDs::gateThreshold, "Gate Threshold", { -80.0f, -20.0f, 0.5f }, -55.0f)),

        group("colour", "Colour",
            number(ParamIDs::optical, "Optical", { 0.0f, 100.0f, 0.1f }, 0.0f),
            number(ParamIDs::clipAmount, "Clip", { 0.0f, 100.0f, 0.1f }, 0.0f),
            choice(ParamIDs::character, "Voice Character",
                { "Neutral", "Bright", "Dark", "Ghost", "Robot", "Demon" }, 0)),

        group("tracking", "Tracking",
            toggle(ParamIDs::lowLatency, "Low Latency", false)),

        group("density", "Density",
            number(ParamIDs::density, "Density", { 0.0f, 100.0f, 0.1f }, 0.0f)),

        group("lock", "Vocal Lock",
            number(ParamIDs::vocalLock, "Vocal Lock", { 0.0f, 100.0f, 0.1f }, 0.0f)),

        group("tone", "Tone",
            number(ParamIDs::satWarmth, "Warmth", { 0.0f, 100.0f, 0.1f }, 0.0f),
            number(ParamIDs::reverbBody, "Reverb Body", { 0.0f, 100.0f, 0.1f }, 50.0f),
            number(ParamIDs::reverbAir, "Reverb Air", { -8.0f, 8.0f, 0.1f }, 0.0f)),

        group("chop", "Chop",
            number(ParamIDs::chopAmount, "Chop", { 0.0f, 100.0f, 0.1f }, 0.0f),
            choice(ParamIDs::chopDivision, "Chop Rate", { "1/8", "1/16", "1/4", "1/8T" }, 1),
            choice(ParamIDs::chopPattern, "Chop Pattern",
                { "Alternate", "Offbeat", "Stutter", "Broken" }, 0)),

        group("crush", "Crush",
            number(ParamIDs::crush, "Crush", { 0.0f, 100.0f, 0.1f }, 0.0f),
            number(ParamIDs::crushMix, "Crush Mix", { 0.0f, 100.0f, 0.1f }, 100.0f)),

        group("mod", "Modulation",
            choice(ParamIDs::modType, "Mod Type", { "Off", "Flanger", "Phaser" }, 0),
            number(ParamIDs::modRate, "Mod Rate", { 0.02f, 8.0f, 0.01f, 0.4f }, 0.4f),
            number(ParamIDs::modDepth, "Mod Depth", { 0.0f, 100.0f, 0.1f }, 50.0f),
            number(ParamIDs::modMix, "Mod Mix", { 0.0f, 100.0f, 0.1f }, 0.0f)),

        group("glue", "Glue",
            number(ParamIDs::glue, "Glue", { 0.0f, 100.0f, 0.1f }, 0.0f)),

        group("neural", "Neural",
            number(ParamIDs::neuralMix, "Neural Mix", { 0.0f, 100.0f, 0.1f }, 0.0f)),

        group("comptype", "Compressor Character",
            choice(ParamIDs::compType, "Comp Character",
                { "Clean", "FET", "VCA", "Vari-Mu" }, 1)),

        group("match", "Voice Match",
            number(ParamIDs::voiceMatch, "Voice Match", { 0.0f, 100.0f, 0.1f }, 0.0f)));

    return layout;
}

void VoxeraAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0) sampleRate = 48000.0;
    samplesPerBlock = juce::jmax(1, samplesPerBlock);
    preparedBlockSize = samplesPerBlock;
    const juce::dsp::ProcessSpec spec
    {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        static_cast<juce::uint32>(getTotalNumOutputChannels())
    };

    previousAnalyzeState = false;
    autoGain.prepare(sampleRate);
    voiceProfile.prepare(sampleRate);
    voiceProfile.restoreProfile(publishedProfile.read());

    // Picked up again from the start rather than resumed: the partial average
    // was discarded with the old rate, and eight seconds of listening is a
    // better answer than a button that quietly did nothing.
    if (captureInterrupted) {
        voiceProfile.startCapture(8.0f);
        captureInterrupted = false;
    }
    pitchEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    spectralEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    smartEQ.prepare(sampleRate, getTotalNumOutputChannels());

    compressor.prepare(sampleRate, getTotalNumOutputChannels());
    compressor.reset();

    gate.prepare(sampleRate, getTotalNumOutputChannels());
    optical.prepare(sampleRate, getTotalNumOutputChannels());
    upward.prepare(sampleRate, getTotalNumOutputChannels());
    vocalLock.prepare(sampleRate, getTotalNumOutputChannels());
    voiceMatch.prepare(sampleRate, getTotalNumOutputChannels());
    punch.prepare(spec);
    exciter.prepare(sampleRate, getTotalNumOutputChannels());
    character.prepare(sampleRate, getTotalNumOutputChannels());
    crush.prepare(sampleRate, getTotalNumOutputChannels());
    modulation.prepare(sampleRate, getTotalNumOutputChannels());
    chop.prepare(sampleRate, getTotalNumOutputChannels());
    neural.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    glue.prepare(sampleRate, getTotalNumOutputChannels());
    softClip.prepare(sampleRate, getTotalNumOutputChannels());
    saturator.prepare(spec);
    spatialEngine.prepare(sampleRate, getTotalNumOutputChannels());
    limiter.prepare(sampleRate, getTotalNumOutputChannels());
    limiter.setReleaseMs(80.0f);

    inputGain.reset(sampleRate, 0.020);
    outputGain.reset(sampleRate, 0.020);
    inputGain.setCurrentAndTargetValue(1.0f);
    outputGain.setCurrentAndTargetValue(1.0f);

    /*  The gate and limiter look-ahead delays run whether or not those stages
        are doing anything, so their contribution is constant. The shifter's is
        not: bypassing it for tracking removes around 55 ms, which is the entire
        reason that mode exists.
    */
    shifterBypassed = prm.lowLatency->load() > 0.5f;
    pitchEngine.setShifterBypassed(false);
    const int fixedLatency =
        gate.getLatencySamples()
        + static_cast<int>(std::ceil(saturator.getLatencySamples()))
        + limiter.getLatencySamples();

    /*  Both engines are asked what they cost, not just the selected one.

        The dry path has to be delayed to match the wet one, and switching
        engines mid-song changes by how much. Sizing that delay for whichever
        engine happened to be chosen when the host called prepare would mean
        the other one needs a longer buffer than exists — and the only place
        left to grow it would be the audio thread, which is the one place a
        plugin must never allocate.
    */
    pitchEngine.setEngine(0); const int rubberBandLatency = pitchEngine.getLatencySamples();
    pitchEngine.setEngine(1); const int psolaLatency = pitchEngine.getLatencySamples();
    selectedEngine = engineFromParameter();
    pitchEngine.setEngine(selectedEngine);

    fullLatencySamples = fixedLatency + pitchEngine.getLatencySamples();
    trackingLatencySamples = fixedLatency;
    const int widestLatency = fixedLatency + juce::jmax(rubberBandLatency, psolaLatency);

    pitchEngine.setShifterBypassed(shifterBypassed);
    const int latency = shifterBypassed ? trackingLatencySamples : fullLatencySamples;
    activeLatencySamples.store(latency);
    setLatencySamples(latency);

    // Allocated for the longest any combination of engine and mode can need, so
    // that every switch afterwards only moves an offset.
    dryDelay.prepare(getTotalNumOutputChannels(), widestLatency);
    dryDelay.setDelay(latency);
    dryBuffer.setSize(getTotalNumOutputChannels(), preparedBlockSize);
    globalWet.reset(sampleRate, 0.020);
    globalWet.setCurrentAndTargetValue(prm.bypass->load() > 0.5f
        ? 0.0f : prm.globalMix->load() * 0.01f);
    scopeWrite = scopeDecimation = 0;
    for (auto& sample : scope) sample.store(0.0f);
    scopeHead.store(0);
}

void VoxeraAudioProcessor::releaseResources()
{
    autoGain.reset();

    /*  A listen in progress survives the host restarting us.

        Toggling LOW LATENCY changes the reported latency, and a host that is
        told its latency changed will often stop and restart the plugin. That
        landed here, where the reset silently threw away whatever Auto Mix had
        heard so far — and because Auto Mix only acts when the capture
        completes, the button appeared to do nothing at all. Nothing reported a
        problem, because from the code's point of view nothing went wrong.
    */
    captureInterrupted = voiceProfile.isCapturing();
    voiceProfile.reset();
    pitchEngine.reset();
    spectralEngine.reset();
    smartEQ.reset();
    compressor.reset();
    saturator.reset();
    spatialEngine.reset();
    gate.reset();
    optical.reset();
    upward.reset();
    vocalLock.reset();
    voiceMatch.reset();
    punch.reset();
    exciter.reset();
    character.reset();
    crush.reset();
    modulation.reset();
    chop.reset();
    neural.reset();
    glue.reset();
    softClip.reset();
    limiter.reset();
}

/*  Mono in / stereo out is accepted alongside the matched layouts: vocals are
    usually recorded mono, and Width, Double and the stereo delay have nothing to
    work with unless the chain is allowed to open out to two channels.
*/
bool VoxeraAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn == mainOut)
        return true;

    return mainIn == juce::AudioChannelSet::mono()
        && mainOut == juce::AudioChannelSet::stereo();
}

void VoxeraAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processAudio(buffer, false);
}

void VoxeraAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processAudio(buffer, true);
}

juce::AudioProcessorParameter* VoxeraAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter("bypass");
}

void VoxeraAudioProcessor::processAudio(juce::AudioBuffer<float>& buffer, bool hostBypass)
{
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0) return;
    inputMeters.analyse(buffer);
    for (int offset = 0; offset < buffer.getNumSamples(); offset += preparedBlockSize)
    {
        const int count = juce::jmin(preparedBlockSize, buffer.getNumSamples() - offset);
        float* pointers[2] { buffer.getWritePointer(0) + offset,
            buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) + offset : nullptr };
        juce::AudioBuffer<float> chunk(pointers, juce::jmin(2, buffer.getNumChannels()), count);
        processChunk(chunk, hostBypass);
    }
    meters.analyse(buffer);
}

void VoxeraAudioProcessor::processChunk(juce::AudioBuffer<float>& buffer, bool hostBypass)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;

    const auto totalIn = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();

    // Mono in / stereo out: mirror the input before anything else so the dry
    // path, the spatial stage and the limiter all see two channels. Matched
    // layouts never enter this loop.
    const int upmixTo = juce::jmin(totalOut, buffer.getNumChannels());
    for (auto ch = totalIn; ch < upmixTo; ++ch)
    {
        if (totalIn > 0) buffer.copyFrom(ch, 0, buffer, 0, 0, buffer.getNumSamples());
        else             buffer.clear(ch, 0, buffer.getNumSamples());
    }

    VoiceProfileEngine::Profile restored;
    if (pendingProfile.tryRead(restored, true)) {
        voiceProfile.restoreProfile(restored);
        profileNeedsPublish = false;
    }
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            dryBuffer.getWritePointer(ch)[i] = dryDelay.process(ch, buffer.getReadPointer(ch)[i]);
        dryDelay.advance();
    }

    /*  A tracking-mode switch changes the reported latency, which only the
        message thread may tell the host about. Here the change is applied to the
        signal path — both of which are integer writes into storage prepare
        already sized — and the notification is handed over.
    */
    const int wantsEngine = engineFromParameter();
    if (const bool wantsTracking = prm.lowLatency->load() > 0.5f;
        wantsTracking != shifterBypassed || wantsEngine != selectedEngine)
    {
        shifterBypassed = wantsTracking;
        selectedEngine = wantsEngine;
        pitchEngine.setEngine(wantsEngine);
        pitchEngine.setShifterBypassed(wantsTracking);
        pitchEngine.reset();
        fullLatencySamples = trackingLatencySamples + pitchEngine.getLatencySamples();
        const int latency = wantsTracking ? trackingLatencySamples : fullLatencySamples;
        activeLatencySamples.store(latency);
        dryDelay.setDelay(latency);
        latencyChangePending.store(true);
        triggerAsyncUpdate();
    }

    const float inDb = prm.inputDb->load();
    inputGain.setTargetValue(juce::Decibels::decibelsToGain(inDb));

    voxera::applyLinkedGain(buffer, inputGain);

    // First in the chain, so room noise is never pitch-shifted, compressed up
    // or sent to the reverb — and so the profiler measures the voice, not the
    // room it was recorded in.
    gate.setEnabled(prm.gateOn->load() > 0.5f);
    gate.setThresholdDb(prm.gateThreshold->load());
    gate.process(buffer);

    const bool analyzeNow = prm.analyzeVoice->load() > 0.5f;

    if (captureRequested.exchange(false) || (analyzeNow && !previousAnalyzeState))
        voiceProfile.startCapture(8.0f);

    previousAnalyzeState = analyzeNow;
    const bool captureWasRunning = voiceProfile.isCapturing();

    if (voiceProfile.isCapturing())
    {
        const auto* l = buffer.getReadPointer(0);
        const auto* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float mono = r != nullptr
                ? 0.5f * (l[i] + r[i])
                : l[i];

            voiceProfile.pushSample(mono);
        }
    }

    autoGain.setEnabled(prm.autoGain->load() > 0.5f);
    autoGain.setTargetDb(prm.targetDb->load());
    autoGain.process(buffer);

    // Pitch before spectral repair: the spectral engine cleans any new
    // resonant emphasis generated by resynthesis.
    pitchEngine.setEnabled(prm.pitchOn->load() > 0.5f);
    pitchEngine.setRoot(static_cast<int>(std::lround(prm.pitchKey->load())));
    pitchEngine.setScale(static_cast<int>(std::lround(prm.pitchScale->load())));
    pitchEngine.setMode(static_cast<int>(std::lround(prm.pitchMode->load())));
    pitchEngine.setAmount(prm.tuneAmount->load() * 0.01f);
    pitchEngine.setRetune(prm.retune->load() * 0.01f);
    pitchEngine.setHumanize(prm.humanize->load() * 0.01f);
    // The character's formant offset rides on top of the user's own setting;
    // the shifter is what moves formants without moving pitch, so it is applied
    // here rather than approximated with filters later.
    character.setType(static_cast<int>(std::lround(prm.character->load())));
    pitchEngine.setFormantSemitones(prm.formant->load() + character.formantOffsetSemitones());
    pitchEngine.process(buffer);

    if (voiceProfile.isCapturing())
        voiceProfile.pushPitch(
            pitchEngine.getDetectedHz(),
            pitchEngine.getConfidence());

    /*  First corrective step, before anything else shapes the spectrum: it
        removes rumble and boxiness relative to this singer's own register
        rather than to a frequency picked in advance. The detector runs in
        tracking mode too, so this keeps working with the shifter bypassed.
    */
    vocalLock.observePitch(pitchEngine.getDetectedHz(), pitchEngine.getConfidence(),
                           buffer.getNumSamples());
    vocalLock.setAmount(prm.vocalLock->load() * 0.01f);
    vocalLock.process(buffer);

    const auto profile = voiceProfile.getProfile();
    const float autoVoice = prm.autoVoice->load() * 0.01f;

    float clean = prm.clean->load() * 0.01f;
    float deEss = prm.deEss->load() * 0.01f;
    float airDb = prm.airDb->load();
    float bodyDb = prm.bodyDb->load();
    float presenceDb = prm.presenceDb->load();
    const float tone = prm.toneMacro->load() * 0.01f;
    bodyDb -= 2.5f * tone;
    presenceDb += 2.0f * tone;
    airDb += 2.0f * tone;

    if (profile.ready && autoVoice > 0.0f)
    {
        const float sibilantBias =
            juce::jlimit(0.0f, 1.0f, profile.sibilance * 28.0f);

        const float brightBias =
            juce::jlimit(0.0f, 1.0f, profile.brightness * 18.0f);

        const float denseLow =
            juce::jlimit(0.0f, 1.0f, profile.lowMid * 12.0f);

        deEss = juce::jlimit(
            0.0f, 1.0f,
            deEss + autoVoice * 0.35f * sibilantBias);

        clean = juce::jlimit(
            0.0f, 1.0f,
            clean + autoVoice * 0.20f * denseLow);

        airDb += autoVoice * (1.2f - 2.0f * brightBias);
        bodyDb -= autoVoice * 1.5f * denseLow;
        presenceDb += autoVoice * 0.8f * (1.0f - brightBias);
    }

    spectralEngine.setEnabled(prm.spectralOn->load() > 0.5f);
    spectralEngine.setClean(clean);
    spectralEngine.setBodyDb(bodyDb);
    spectralEngine.setPresenceDb(presenceDb);
    spectralEngine.setDeEss(deEss);
    spectralEngine.setAirDb(airDb);
    spectralEngine.process(buffer);
    /*  The spectral engine's transform is handed straight to the Smart EQ.

        It was being computed and thrown away, while the stage immediately after
        it estimated the same spectrum with a bank of filters. The frame is of
        the spectral engine's input rather than its output, so what the Smart EQ
        reads is the spectrum before that stage corrected anything; both are
        cut-only and share a budget the user sets, so the worst case is that the
        two together take a little more out of a resonance than either would
        alone, which is what a listener wants from a resonance in the first
        place.
    */
    smartEQ.useSpectrum(spectralEngine.analysisMagnitudes(),
                        AdaptiveSpectralEngine::analysisBinCount,
                        spectralEngine.analysisBinHz(),
                        spectralEngine.analysisScale(),
                        spectralEngine.analysisFrame());
    smartEQ.setParameters(prm.smartEQAmount->load() * 0.01f,
        prm.smartEQRange->load(), prm.smartEQResponse->load());
    smartEQ.process(buffer);

    /*  Matched to the reference take before the dynamics, so the compressors
        respond to the voice as it will be heard.

        Reads the same shared spectrum as the Smart EQ, which is of this chain's
        signal before either stage corrects it — the same point the reference
        was captured from, which is what makes comparing them meaningful.
    */
    if (referenceRequested.exchange(false)) voiceMatch.startCapture(6.0f);
    voiceMatch.useSpectrum(spectralEngine.analysisMagnitudes(),
                           AdaptiveSpectralEngine::analysisBinCount,
                           spectralEngine.analysisBinHz(),
                           spectralEngine.analysisScale());
    voiceMatch.setAmount(prm.voiceMatch->load() * 0.01f);
    voiceMatch.process(buffer);

    float compThreshold = prm.compThreshold->load();

    if (profile.ready && autoVoice > 0.0f)
    {
        const float crestNeed =
            juce::jlimit(0.0f, 1.0f, (profile.crestDb - 7.0f) / 10.0f);

        compThreshold -= autoVoice * 4.0f * crestNeed;
    }

    compressor.setType(static_cast<int>(std::lround(prm.compType->load())));
    compressor.setThresholdDb(compThreshold);
    compressor.setRatio(prm.compRatio->load());
    compressor.setAttackMs(prm.compAttack->load());
    compressor.setReleaseMs(prm.compRelease->load());
    compressor.setSidechainHz(prm.compSidechain->load());
    compressor.setMix(prm.compMix->load() * 0.01f);
    compressor.process(buffer);

    // The slow half of the two-stage topology: the compressor above catches
    // individual peaks, this levels whole syllables and phrases. Splitting the
    // work across two time scales is what avoids audible pumping.
    optical.setAmount(prm.optical->load() * 0.01f);
    optical.process(buffer);

    // After both downward stages, working from the opposite direction: they have
    // already decided where the ceiling is, so what is left to do is raise the
    // floor towards it. Doing this earlier would only give those stages more to
    // push back down.
    upward.setAmount(prm.density->load() * 0.01f);
    upward.process(buffer);

    // Parallel, and last of the three, so it fills the gaps the serial stages
    // just opened rather than fighting an already-dense signal.
    punch.setAmount(prm.punch->load() * 0.01f);
    punch.process(buffer);

    /*  Tone and character, after every dynamics stage and before any harmonic
        one — the position they occupy on a professional vocal chain, and for a
        reason rather than by convention.

        What is taken away belongs ahead of the compressors, so they work on the
        signal that is staying instead of reacting to mud and sibilance already
        on their way out. What is added belongs behind them, because a boost
        made in front of a compressor is simply more level in the band the
        detector is watching: the compressor pulls it back down, and the control
        appears to stop working past a certain point. Both of these ran before
        the dynamics until now, so the presence and air controls were fighting
        the compressor for the same decibels, and the voice character was being
        levelled away by the stage after it.

        Then the harmonic stages hear the voice already shaped, which is the
        other half of the arrangement: saturation follows tone, never leads it.
    */
    spectralEngine.processTone(buffer);
    character.process(buffer);

    /*  Before the saturator, in the place a preamp occupies on a real desk:
        these captures are of the stage a microphone hits first, so anything
        after it should be hearing what that stage produced.
    */
    neural.setMix(prm.neuralMix->load() * 0.01f);
    neural.process(buffer);

    saturator.setDriveDb(prm.satDrive->load());
    saturator.setWarmth(prm.satWarmth->load() * 0.01f);
    saturator.setMix(prm.satMix->load() * 0.01f);
    saturator.process(buffer);

    // After saturation so the generated top is not put through the shaper a
    // second time, and before the spatial stage so the delay and reverb carry
    // that air into their tails.
    exciter.setAmount(prm.exciter->load() * 0.01f);
    exciter.process(buffer);

    double bpm = 120.0;
    // Negative means the host offered no musical position; the chop falls back
    // to counting rather than pretending it knows where the bar is.
    double ppq = -1.0;
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto hostBpm = position->getBpm())
                bpm = *hostBpm;
            if (auto hostPpq = position->getPpqPosition())
                ppq = *hostPpq;
        }
    }

    // Destruction before movement before rhythm: the crush is a tone, the
    // modulation moves that tone, and the chop cuts the result into the bar.
    crush.setAmount(prm.crush->load() * 0.01f);
    crush.setMix(prm.crushMix->load() * 0.01f);
    crush.process(buffer);

    modulation.setType(static_cast<int>(std::lround(prm.modType->load())));
    modulation.setRateHz(prm.modRate->load());
    modulation.setDepth(prm.modDepth->load() * 0.01f);
    modulation.setMix(prm.modMix->load() * 0.01f);
    modulation.process(buffer);

    // Ahead of the spatial stage on purpose, so the reverb and delay ring on
    // through the closed steps instead of being cut off with them.
    chop.setTempo(bpm);
    chop.setPosition(ppq);
    chop.setDivision(static_cast<int>(std::lround(prm.chopDivision->load())));
    chop.setPattern(static_cast<int>(std::lround(prm.chopPattern->load())));
    chop.setAmount(prm.chopAmount->load() * 0.01f);
    chop.process(buffer);

    spatialEngine.setEnabled(prm.spatialOn->load() > 0.5f);
    spatialEngine.setTempo(bpm);
    spatialEngine.setDivision(static_cast<int>(std::lround(prm.delayDivision->load())));
    spatialEngine.setWidth(prm.width->load() * 0.01f);
    spatialEngine.setDouble(prm.doubler->load() * 0.01f);
    spatialEngine.setDelay(prm.delayMix->load() * 0.01f);
    spatialEngine.setDelayFeedback(prm.delayFeedback->load() * 0.01f);
    spatialEngine.setSpace(prm.space->load() * 0.01f);
    spatialEngine.setReverbBody(prm.reverbBody->load() * 0.01f);
    spatialEngine.setReverbAirDb(prm.reverbAir->load());
    spatialEngine.setDuck(prm.duck->load() * 0.01f);
    spatialEngine.process(buffer);

    // Last thing that shapes dynamics, and after the spatial stage so the tails
    // move with the voice rather than on their own envelope.
    glue.setAmount(prm.glue->load() * 0.01f);
    glue.process(buffer);

    const float outDb = prm.outputDb->load();
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(outDb));

    voxera::applyLinkedGain(buffer, outputGain);

    // Rounding the peaks here means the limiter below has far less to pull down,
    // which is what lets the average level rise without it becoming audible.
    softClip.setAmount(prm.clipAmount->load() * 0.01f);
    softClip.process(buffer);

    // Last in the chain: saturation, spatial feedback and output gain all sit
    // upstream, so this is the only point that can promise the ceiling holds.
    limiter.setEnabled(prm.limiterOn->load() > 0.5f);
    limiter.setCeilingDb(prm.limiterCeiling->load());
    limiter.process(buffer);

    const bool bypass = hostBypass || prm.bypass->load() > 0.5f;
    globalWet.setTargetValue(bypass ? 0.0f : prm.globalMix->load() * 0.01f);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        const float wet = globalWet.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            auto& y = buffer.getWritePointer(ch)[i];
            y = wet * y + (1.0f - wet) * dryBuffer.getReadPointer(ch)[i];
        }
        if (++scopeDecimation >= 16) {
            scopeDecimation = 0;
            scope[static_cast<size_t>(scopeWrite)].store(buffer.getReadPointer(0)[i], std::memory_order_relaxed);
            scopeWrite = (scopeWrite + 1) % 256;
        }
    }
    scopeHead.store(scopeWrite, std::memory_order_relaxed);
    capturing.store(voiceProfile.isCapturing());
    captureProgress.store(voiceProfile.getProgress());
    profileReady.store(profile.ready);
    if (captureWasRunning && !voiceProfile.isCapturing()) profileNeedsPublish = true;
    if (profileNeedsPublish && publishedProfile.tryPublish(profile)) profileNeedsPublish = false;
    // Only once the profile is visible to the other thread is there anything
    // for the auto-mix to read.
    if (captureWasRunning && !voiceProfile.isCapturing() && autoMixRequested.load())
        triggerAsyncUpdate();
}

juce::AudioProcessorEditor* VoxeraAudioProcessor::createEditor()
{
    return new VoxeraAudioProcessorEditor(*this);
}

void VoxeraAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("voxeraStateVersion", stateVersion, nullptr);
    // Capture is a transient action, not a task to repeat when reopening a session.
    auto captureParameter = state.getChildWithProperty("id", "analyzeVoice");
    if (captureParameter.isValid()) captureParameter.setProperty("value", 0.0f, nullptr);
    auto oldProfile = state.getChildWithName("VoiceProfile");
    if (oldProfile.isValid()) state.removeChild(oldProfile, nullptr);
    const auto profile = publishedProfile.read();
    juce::ValueTree saved("VoiceProfile");
    saved.setProperty("ready", profile.ready, nullptr);
    saved.setProperty("rms", profile.avgRmsDb, nullptr);
    saved.setProperty("crest", profile.crestDb, nullptr);
    saved.setProperty("low", profile.lowMid, nullptr);
    saved.setProperty("presence", profile.presence, nullptr);
    saved.setProperty("sibilance", profile.sibilance, nullptr);
    saved.setProperty("brightness", profile.brightness, nullptr);
    saved.setProperty("confidence", profile.pitchConfidence, nullptr);
    saved.setProperty("range", profile.pitchRangeSemitones, nullptr);
    state.addChild(saved, -1, nullptr);
    // The path rather than the weights: a capture is someone's file on disk, and
    // copying it into every session that used it would be both wasteful and a
    // way of redistributing it without meaning to.
    if (loadedNeuralFile.existsAsFile())
        state.setProperty("neuralModel", loadedNeuralFile.getFullPathName(), nullptr);

    /*  The voice reference travels with the session. It is eight numbers, and
        without it the Voice Match control would be a knob that does nothing
        every time a project is reopened — which is worse than not having it.
    */
    if (voiceMatch.hasReference()) {
        auto stored = state.getChildWithName("VoiceReference");
        if (stored.isValid()) state.removeChild(stored, nullptr);
        juce::ValueTree shape("VoiceReference");
        const auto& reference = voiceMatch.getReference();
        for (int b = 0; b < voxera::VoiceMatch::numBands; ++b)
            shape.setProperty("b" + juce::String(b), reference.shapeDb[static_cast<size_t>(b)], nullptr);
        state.addChild(shape, -1, nullptr);
    }
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VoxeraAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType())) {
        auto state = juce::ValueTree::fromXml(*xml);
        const auto saved = state.getChildWithName("VoiceProfile");
        VoiceProfileEngine::Profile p;
        auto number = [&saved](const char* key, float fallback, float lo, float hi) {
            const float value = static_cast<float>(saved.getProperty(key, fallback));
            return std::isfinite(value) ? juce::jlimit(lo, hi, value) : fallback;
        };
        if (saved.isValid()) {
            p.avgRmsDb = number("rms", -60.0f, -120.0f, 24.0f);
            p.crestDb = number("crest", 0.0f, 0.0f, 120.0f);
            p.lowMid = number("low", 0.0f, 0.0f, 1.0f);
            p.presence = number("presence", 0.0f, 0.0f, 1.0f);
            p.sibilance = number("sibilance", 0.0f, 0.0f, 1.0f);
            p.brightness = number("brightness", 0.0f, 0.0f, 2.0f);
            p.pitchConfidence = number("confidence", 0.0f, 0.0f, 1.0f);
            p.pitchRangeSemitones = number("range", 0.0f, 0.0f, 96.0f);
            p.ready = static_cast<bool>(saved.getProperty("ready", false)) && p.avgRmsDb > -55.0f;
        }
        /*  A state written by an older build has no entry for parameters added
            since. Walking the current parameter list rather than a hand-kept ID
            list means every future addition is restored to its default without
            anyone having to remember to update this loop.
        */
        const int savedVersion = static_cast<int>(state.getProperty("voxeraStateVersion", 1));
        if (savedVersion < stateVersion) {
            for (auto* parameter : getParameters()) {
                auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
                if (ranged == nullptr) continue;
                if (state.getChildWithProperty("id", ranged->paramID).isValid()) continue;
                juce::ValueTree restored("PARAM");
                restored.setProperty("id", ranged->paramID, nullptr);
                restored.setProperty("value", ranged->convertFrom0to1(ranged->getDefaultValue()), nullptr);
                state.addChild(restored, -1, nullptr);
            }
        }
        auto captureParameter = state.getChildWithProperty("id", "analyzeVoice");
        if (captureParameter.isValid()) captureParameter.setProperty("value", 0.0f, nullptr);
        // Silently ignored when the file has moved or the session was written on
        // another machine: the mix control is restored either way, so the worst
        // case is a chain with that stage doing nothing rather than a failure.
        if (const auto shape = state.getChildWithName("VoiceReference"); shape.isValid()) {
            voxera::VoiceMatch::Reference reference;
            bool sane = true;
            for (int b = 0; b < voxera::VoiceMatch::numBands; ++b) {
                const float value = shape.getProperty("b" + juce::String(b), 0.0f);
                // A stored shape that is not finite or is wildly out of range
                // came from a corrupt session; a silent default beats applying
                // sixty decibels of correction to somebody's vocal.
                if (!std::isfinite(value) || std::abs(value) > 60.0f) { sane = false; break; }
                reference.shapeDb[static_cast<size_t>(b)] = value;
            }
            reference.ready = sane;
            if (sane) voiceMatch.setReference(reference);
        }

        if (const auto path = state.getProperty("neuralModel").toString(); path.isNotEmpty()) {
            if (const juce::File file(path); file.existsAsFile()) loadNeuralModel(file);
        }

        apvts.replaceState(state);
        publishedProfile.publish(p);
        pendingProfile.publish(p);
        profileReady.store(p.ready);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxeraAudioProcessor();
}

void VoxeraAudioProcessor::setParameterNotifying(const char* id, float value)
{
    if (auto* parameter = apvts.getParameter(id)) {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}

void VoxeraAudioProcessor::handleAsyncUpdate()
{
    if (latencyChangePending.exchange(false)) {
        setLatencySamples(activeLatencySamples.load());
        updateHostDisplay(ChangeDetails{}.withLatencyChanged(true));
    }
    if (autoMixRequested.exchange(false)) applyAutoMix();
}

void VoxeraAudioProcessor::applyAutoMix()
{
    const auto profile = publishedProfile.read();
    const auto diagnosis = voxera::diagnose(profile);
    lastReport = voxera::describe(diagnosis, voxera::decide(diagnosis));

    // A capture that never heard a usable signal must not rewrite the chain.
    if (!profile.ready) return;

    const auto settings = voxera::decide(diagnosis);
    const auto set = [this](const char* id, float value) { setParameterNotifying(id, value); };

    set(ParamIDs::clean, settings.clean);
    set(ParamIDs::bodyDb, settings.bodyDb);
    set(ParamIDs::presenceDb, settings.presenceDb);
    set(ParamIDs::airDb, settings.airDb);
    set(ParamIDs::deEss, settings.deEss);
    set(ParamIDs::smartEQAmount, settings.smartEQAmount);
    set(ParamIDs::vocalLock, settings.vocalLock);
    set(ParamIDs::compThreshold, settings.compThreshold);
    set(ParamIDs::compRatio, settings.compRatio);
    set(ParamIDs::optical, settings.optical);
    set(ParamIDs::density, settings.density);
    set(ParamIDs::punch, settings.punch);
    set(ParamIDs::clipAmount, settings.clipAmount);
    set(ParamIDs::gateThreshold, settings.gateThresholdDb);
    set(ParamIDs::gateOn, 1.0f);
    set(ParamIDs::satDrive, settings.satDrive);
    set(ParamIDs::satMix, settings.satMix);
    set(ParamIDs::satWarmth, settings.satWarmth);
    set(ParamIDs::compType, static_cast<float>(settings.compType));
    set(ParamIDs::exciter, settings.exciter);
    set(ParamIDs::tuneAmount, settings.tuneAmount);
    set(ParamIDs::retune, settings.retune);
    set(ParamIDs::humanize, settings.humanize);

    // The analysis decides the treatment, not whether the chain runs; key,
    // scale, gains, spatial settings, the chosen voice character and the
    // learned profile are all left alone.
    set(ParamIDs::pitchOn, 1.0f);
    set(ParamIDs::spectralOn, 1.0f);
    set(ParamIDs::limiterOn, 1.0f);
    set(ParamIDs::toneMacro, 0.0f);
    set(ParamIDs::globalMix, 100.0f);
}

void VoxeraAudioProcessor::applyFactoryPreset(int index)
{
    // Deliberately preserve key, scale, input/output gain and learned voice profile.
    const auto set = [this](const char* id, float value) { setParameterNotifying(id, value); };
    /*  Declaring both with the same extent makes a mismatched row a compile
        error. Every preset drives the whole chain rather than a corner of it:
        a preset that leaves the density, optical and clip stages at zero is
        heard as the plugin sounding thin, whatever the rest is doing.
    */
    static constexpr int numPresetValues = 27;
    static constexpr const char* ids[numPresetValues] = {
        "tuneAmount", "retune", "humanize", "toneMacro", "airDb",
        "space", "satDrive", "satMix", "punch", "exciter",
        "optical", "density", "clipAmount", "smartEQAmount", "vocalLock",
        // Warmth belongs in every preset. Left out, the one stage that puts
        // even harmonics below 7 kHz never runs, and a chain that cannot
        // produce them cannot sound like a valve stage however it is set.
        "satWarmth",
        // And the gain element, for the same reason: leaving every preset on
        // the arithmetic compressor would make the character choice something
        // only a user who went looking would ever hear.
        "compType",

        /*  The eight that decide which production sound this is.

            The general presets could leave these out and still differ from each
            other usefully, because they differ in degree. A style does not: the
            distance between one modern rap vocal and another is almost entirely
            how hard the tuner grips, where the formants sit, how much of the
            voice is doubled, and how much room is thrown behind it. Setting the
            saturation and leaving those at their defaults produces five presets
            that all sound like the same singer wearing different coats.

            Tune mode in particular was previously derived from the preset's own
            index, which worked while there were five of them and no style
            depended on it.
        */
        "pitchMode", "formant", "doubler", "width",
        "delayMix", "reverbBody", "presenceDb", "deEss",

        /*  How hard the gain element is blended in, and what it is deaf to.

            Parallel compression is not a refinement of the amount — it is a
            different result. Crushing flat and sitting it under the dry keeps
            the transient the dry still has while bringing the quiet detail and
            the element's own distortion up underneath, and no setting of
            threshold and ratio alone arrives at that. The style presets that
            want an aggressive element mostly want it blended.
        */
        "compMix", "compSidechain"
    };
    /*  Comp: 0 Clean, 1 FET, 2 VCA, 3 Vari-Mu.  Tune mode: 0 Natural, 1 Modern, 2 Hard.

        The first five move along one axis — how much of everything. The last
        five are shapes rather than amounts, and each is built around the one
        decision that actually identifies it:

          Rage     the tuner locked hard with the retune at its fastest, so
                   every note snaps instead of sliding, and the formants pushed
                   up so the voice reads younger and thinner than it was sung.
                   Clipped rather than compressed.
          Astro    the same hard grip, but the formants pulled slightly down
                   and most of the sound is what is behind the voice: a long
                   room, a wide double, and a valve element leaning on it.
          Melodic  the tuner audible but not gripping, so the singing survives.
                   This is the one that fails if the retune is too fast.
          Drill    dry and forward. Almost no room, a fast gain element, and
                   the presence lifted — it has to sit in front of the beat
                   rather than in a space of its own.
          Ad-Lib   not a lead sound. Everything wide, everything drenched,
                   formants well up, and no attempt at keeping it natural,
                   because it is meant to sit behind another vocal.
    */
    static constexpr float values[numFactoryPresets][numPresetValues] = {
        // tune retune human  tone  air space drive  mix punch excite optic dens clip smrtEQ lock warm comp | mode form dbl width delay verb pres deEss | cmix schpf
        {   35,    30,   70,    0,   1,    8,    2,   8,   20,    15,   25,  35,   5,    20,   55,  20,   2,     0,   0,  10,   50,    5,  40,   1,  55, 100,  60 }, // Clean
        {   55,    40,   60,  -30,  -1,   14,    7,  30,   35,    10,   45,  45,  12,    30,   60,  70,   3,     1,   0,  20,   60,   10,  50,   0,  50,  85,  75 }, // Warm
        {  100,    75,   25,   10,   3,   18,    5,  20,   60,    50,   55,  65,  25,    40,   70,  45,   1,     1,   0,  30,   70,   15,  45,   2,  60,  90,  95 }, // Modern
        {   70,    45,   65,   20,   4,   65,    3,  15,   30,    40,   40,  50,  10,    25,   50,  40,   3,     1,   0,  45,   80,   30,  70,   1,  50,  80,  70 }, // Dream
        {   90,    85,   10,  -55,  -3,   10,   14,  60,   75,    35,   70,  80,  45,    35,   75,  60,   1,     2,   0,  15,   55,    8,  35,   3,  65, 100, 110 }, // Radio

        {  100,    98,    0,   40,   5,   12,   18,  70,   80,    65,   60,  75,  70,    30,   60,  35,   1,     2,   3,  35,   75,   15,  30,   4,  70,  75, 150 }, // Rage
        {  100,    90,    5,  -10,   3,   55,   12,  55,   55,    45,   55,  60,  35,    35,   65,  55,   3,     2,  -2,  55,   85,   35,  75,   2,  60,  70, 120 }, // Astro
        {   85,    60,   30,    0,   3,   45,    6,  30,   40,    40,   50,  55,  15,    35,   60,  60,   3,     1,   0,  40,   75,   28,  65,   2,  55,  85,  90 }, // Melodic
        {   60,    70,   25,   15,   2,   10,   10,  40,   70,    40,   55,  60,  30,    40,   70,  40,   1,     1,   0,  15,   45,    8,  25,   3,  65,  65, 140 }, // Drill
        {  100,    95,    0,   25,   5,   80,   14,  60,   45,    60,   45,  65,  45,    25,   50,  45,   1,     2,   5,  70,  100,   50,  85,   3,  60,  60, 130 }  // Ad-Lib
    };
    index = juce::jlimit(0, numFactoryPresets - 1, index);
    for (int i = 0; i < numPresetValues; ++i) set(ids[i], values[index][i]);
    // The gate belongs on for all of them: everything above works by raising
    // quiet material, and room tone is quiet material.
    set(ParamIDs::gateOn, 1.0f);
    set(ParamIDs::limiterOn, 1.0f);
    set("pitchOn", 1); set("spectralOn", 1); set("spatialOn", 1);
    // Tune mode is a column now rather than a function of the preset's position
    // in the list, which stopped being meaningful the moment presets started
    // being chosen for their sound rather than their order.
    set("globalMix", 100);
    currentProgram = index;
    updateHostDisplay();
}

const juce::String VoxeraAudioProcessor::getProgramName(int index)
{
    static const char* names[numFactoryPresets] {
        "Clean", "Warm", "Modern", "Dream", "Radio",
        "Rage", "Astro", "Melodic", "Drill", "Ad-Lib"
    };
    return names[juce::jlimit(0, numFactoryPresets - 1, index)];
}

void VoxeraAudioProcessor::setCurrentProgram(int index)
{
    index = juce::jlimit(0, numFactoryPresets - 1, index);
    // A program change is a preset recall: the same parameter moves the editor
    // buttons make, so the host sees them as ordinary automatable changes.
    applyFactoryPreset(index);
}

// Conservative -60 dB tail estimate using the longest supported synced delay
// (half note at 40 BPM = 3s), current feedback, and maximum reverb decay.
double VoxeraAudioProcessor::getTailLengthSeconds() const
{
    const double feedback = juce::jlimit(0.0, 0.78,
        static_cast<double>(prm.delayFeedback->load()) * 0.0078);
    const double repeats = feedback > 0.000001 ? std::log(0.001) / std::log(feedback) : 0.0;
    return 4.8 + 3.0 * (1.0 + repeats);
}

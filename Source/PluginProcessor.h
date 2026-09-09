#pragma once

#include <JuceHeader.h>
#include "DSP/AutoGain.h"
#include "DSP/PitchEngine.h"
#include "DSP/Saturator.h"
#include "DSP/Metering.h"
#include "DSP/SpectralEngine.h"
#include "DSP/SmartEQ.h"
#include "DSP/SpatialEngine.h"
#include "DSP/VoiceProfileEngine.h"
#include "DSP/ProfileTransfer.h"
#include "DSP/ValueTransfer.h"
#include "DSP/LevelMatch.h"
#include "DSP/Limiter.h"
#include "DSP/Punch.h"
#include "DSP/Exciter.h"
#include "DSP/Gate.h"
#include "DSP/Optical.h"
#include "DSP/Upward.h"
#include "DSP/VocalLock.h"
#include "DSP/Chop.h"
#include "DSP/Modulation.h"
#include "DSP/NeuralStage.h"
#include "DSP/ColourCompressor.h"
#include "DSP/PluginSlot.h"
#include "DSP/VoiceMatch.h"
#include "DSP/SoftClip.h"
#include "DSP/Character.h"
#include "DSP/AutoMix.h"
#include "DSP/RealtimeUtilities.h"

class VoxeraAudioProcessor : public juce::AudioProcessor,
                             private juce::AsyncUpdater
{
public:
    /*  Five general starting points, then five that chase a particular
        production sound rather than a general quality.

        The second group is named for what it does, not for who is known for
        it. The processing itself is nobody's property — an EQ curve, a retune
        speed and a reverb size are technique, and technique has always been
        shared — but a real person's name on a preset in something you sell
        says they endorsed it, which is a different claim entirely and not
        one that is ours to make.
    */
    static constexpr int numFactoryPresets = 10;
    // Bumped whenever parameters are added, so an older saved state is known to
    // need the missing-parameter pass in setStateInformation.
    // 4: density, vocal lock, warmth, reverb tone, chop, crush, modulation and
    // glue. Every addition has to bump this, or a session saved by the previous
    // build loads without the pass that gives new parameters their defaults.
    // 5: pitch engine choice, compressor sidechain filter and compressor mix.
    // Missed on the first two of those and caught by re-reading this comment,
    // which is the whole reason it is written as an instruction rather than as
    // a description.
    static constexpr int stateVersion = 6;

    VoxeraAudioProcessor();
    // Cancels here rather than relying on the base destructor: by the time
    // AsyncUpdater is destroyed this object's own members are already gone, and
    // a callback still in flight would reach them.
    ~VoxeraAudioProcessor() override { cancelPendingUpdate(); }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    juce::AudioProcessorEditor* createEditor() override;
    void requestVoiceCapture() noexcept { captureRequested.store(true); }

    /*  Captures eight seconds of voice and then sets the whole chain from what
        it measured. The settings arrive as ordinary parameter changes, so the
        host records them, the user can undo them, and every one stays editable
        afterwards — the analysis proposes a starting point, it does not take
        the controls away.
    */
    void requestAutoMix() noexcept { autoMixRequested.store(true); captureRequested.store(true); }
    bool canUndoAutoMix() const { return autoMixHistoryValid.load(); }
    bool isComparingBefore() const { return canUndoAutoMix() && comparingBefore; }
    void compareAutoMix();
    void undoAutoMix();

    /*  Learns the spectral shape of the take now playing, so later takes can be
        matched to it. Voice conversion where the target is the singer's own
        best day rather than somebody else's voice.
    */
    void requestVoiceReference() noexcept { referenceRequested.store(true); }
    bool isLearningReference() const noexcept { return referenceCapturing.load(); }
    float referenceProgress() const noexcept { return referenceCaptureProgress.load(); }
    bool hasVoiceReference() const noexcept { return referenceReady.load(); }
    float voiceMatchRangeDb() const noexcept { return voiceMatch.appliedRangeDb(); }
    void clearVoiceReference() { pendingReference.publish({}); publishedReference.publish({}); referenceReady.store(false); }
    bool isAutoMixPending() const noexcept { return autoMixRequested.load(); }

    /*  The insert slot: somebody else's compressor or EQ, inside this chain.

        Loading suspends audio, because the pointer the audio thread follows has
        to stop being read before the instance behind it is destroyed, and
        because instantiating a licensed plugin can take seconds.
    */
    voxera::PluginSlot::LoadResult loadInsertPlugin(const juce::File& file);
    void unloadInsertPlugin();
    bool hasInsertPlugin() const noexcept { return insertSlot.hasPlugin(); }
    juce::String insertPluginName() const { return insertSlot.pluginName(); }
    juce::StringArray insertParameterNames() const { return insertSlot.parameterNames(); }
    juce::String insertParameterText(int index) const { return insertSlot.parameterText(index); }
    float insertParameterValue(int index) const { return insertSlot.getParameter(index); }
    void setInsertParameter(int index, float normalised) { insertSlot.setParameter(index, normalised); }
    int findInsertParameter(const juce::StringArray& words) const { return insertSlot.findParameter(words); }

    void applyFactoryPreset(int index);
    float smartEQGain(size_t band) const noexcept { return smartEQ.gainDb(band); }
    float detectedHz() const noexcept { return pitchEngine.getDetectedHz(); }
    float targetHz() const noexcept { return pitchEngine.getTargetHz(); }
    float pitchConfidence() const noexcept { return pitchEngine.getConfidence(); }
    float shiftSemitones() const noexcept { return pitchEngine.getShiftSemitones(); }
    float limiterReductionDb() const noexcept { return limiter.getReductionDb(); }
    float gateReductionDb() const noexcept { return gate.getReductionDb(); }
    int reportedLatencySamples() const noexcept { return activeLatencySamples.load(); }
    float opticalReductionDb() const noexcept { return optical.getReductionDb(); }
    float compressorReductionDb() const noexcept { return compressor.getReductionDb(); }
    float densityLiftDb() const noexcept { return upward.getLiftDb(); }
    // Shown in the editor so the singer can see the chain following their range.
    float lockHighPassHz() const noexcept { return vocalLock.highPassHz(); }
    float lockMudHz() const noexcept { return vocalLock.mudHz(); }

    /*  What the last auto-mix concluded, in words. Written in handleAsyncUpdate
        and read by the editor's timer, both of which are the message thread, so
        no synchronisation is needed here.
    */
    const juce::String& autoMixReport() const noexcept { return lastReport; }

    /*  Loads a neural capture. Message thread only.

        Audio is suspended across the swap. The audio thread holds a raw pointer
        to the model while it runs, so the object behind it cannot be released
        until that thread has certainly stopped reading it, and suspending is
        the mechanism a plugin already has for saying so.
    */
    /*  Where captures live, so they are chosen from a list instead of hunted
        for. Created on first use: a folder that has to be made by hand is a
        folder nobody uses.
    */
    static juce::File neuralModelFolder();
    static juce::Array<juce::File> availableNeuralModels();

    voxera::NeuralStage::LoadResult loadNeuralModel(const juce::File& file);
    void unloadNeuralModel();
    bool hasNeuralModel() const noexcept { return neural.hasModel(); }
    juce::String neuralModelName() const { return neural.modelName(); }
    const juce::File& neuralModelFile() const noexcept { return loadedNeuralFile; }
    std::atomic<float> captureProgress { 0.0f };
    std::atomic<bool> capturing { false }, profileReady { false };
    std::array<std::atomic<float>, 256> scope {};
    std::atomic<int> scopeHead { 0 };
    Metering inputMeters;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    // The factory bank is exposed as host programs so DAW preset browsers and
    // program automation can reach the same five starting points as the editor.
    int getNumPrograms() override { return numFactoryPresets; }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    Metering meters;

private:
    void processAudio(juce::AudioBuffer<float>&, bool hostBypass);
    void processChunk(juce::AudioBuffer<float>&, bool hostBypass);

    /*  Resolving a parameter by string costs a lookup on every call, and the
        chain reads roughly forty of them per block. The pointers are stable for
        the lifetime of the APVTS, so they are bound once in the constructor and
        the audio thread only ever does an atomic load.
    */
    struct ParameterCache
    {
        std::atomic<float>* inputDb {};
        std::atomic<float>* autoGain {};
        std::atomic<float>* targetDb {};
        std::atomic<float>* pitchOn {};
        std::atomic<float>* pitchKey {};
        std::atomic<float>* pitchScale {};
        std::atomic<float>* pitchMode {};
        std::atomic<float>* pitchEngine {};

        std::atomic<float>* tuneAmount {};
        std::atomic<float>* retune {};
        std::atomic<float>* humanize {};
        std::atomic<float>* formant {};
        std::atomic<float>* analyzeVoice {};
        std::atomic<float>* autoVoice {};
        std::atomic<float>* spectralOn {};
        std::atomic<float>* clean {};
        std::atomic<float>* bodyDb {};
        std::atomic<float>* presenceDb {};
        std::atomic<float>* deEss {};
        std::atomic<float>* airDb {};
        std::atomic<float>* compThreshold {};
        std::atomic<float>* compRatio {};
        std::atomic<float>* compAttack {};
        std::atomic<float>* compRelease {};
        std::atomic<float>* compSidechain {};
        std::atomic<float>* compMix {};
        std::atomic<float>* satDrive {};
        std::atomic<float>* satMix {};
        std::atomic<float>* spatialOn {};
        std::atomic<float>* width {};
        std::atomic<float>* doubler {};
        std::atomic<float>* delayMix {};
        std::atomic<float>* delayFeedback {};
        std::atomic<float>* delayDivision {};
        std::atomic<float>* space {};
        std::atomic<float>* duck {};
        std::atomic<float>* outputDb {};
        std::atomic<float>* toneMacro {};
        std::atomic<float>* globalMix {};
        std::atomic<float>* bypass {};
        std::atomic<float>* smartEQAmount {};
        std::atomic<float>* smartEQRange {};
        std::atomic<float>* smartEQResponse {};
        std::atomic<float>* limiterOn {};
        std::atomic<float>* limiterCeiling {};
        std::atomic<float>* punch {};
        std::atomic<float>* exciter {};
        std::atomic<float>* gateOn {};
        std::atomic<float>* gateThreshold {};
        std::atomic<float>* optical {};
        std::atomic<float>* clipAmount {};
        std::atomic<float>* character {};
        std::atomic<float>* lowLatency {};
        std::atomic<float>* density {};
        std::atomic<float>* vocalLock {};
        std::atomic<float>* satWarmth {};
        std::atomic<float>* reverbBody {};
        std::atomic<float>* reverbAir {};
        std::atomic<float>* chopAmount {};
        std::atomic<float>* chopDivision {};
        std::atomic<float>* chopPattern {};
        std::atomic<float>* crush {};
        std::atomic<float>* crushMix {};
        std::atomic<float>* modType {};
        std::atomic<float>* modRate {};
        std::atomic<float>* modDepth {};
        std::atomic<float>* modMix {};
        std::atomic<float>* glue {};
        std::atomic<float>* neuralMix {};
        std::atomic<float>* compType {};
        std::atomic<float>* voiceMatch {};
    } prm;

    // The chain can report two different latencies. Both are worked out once in
    // prepareToPlay; the dry delay is allocated for the larger of the two so
    // switching between them never allocates on the audio thread.
    int fullLatencySamples = 0, trackingLatencySamples = 0;
    // The parts the figure is rebuilt from when the insert slot changes.
    int baseFixedLatency = 0, widestPitchLatency = 0;
    void republishLatency();
    std::atomic<int> activeLatencySamples { 0 };
    std::atomic<bool> latencyChangePending { false };
    bool shifterBypassed = false;
    bool captureInterrupted = false;
    int selectedEngine = 1;

    /*  Translates the menu into the engine the pitch stage understands.

        The two orders are not the same: the menu lists PSOLA first because it
        is the default, while the engine enumeration keeps Rubber Band at zero
        because it was there first.

        Worth being plain about what that default means for existing work. A
        project saved before this parameter existed has no value stored for it,
        so it opens on PSOLA — a different engine than it was mixed through,
        and it will not sound identical. That is the deliberate choice for a
        new version rather than an oversight, and the old sound is one menu
        entry away; the earlier builds remain installed separately for anyone
        who needs a mix to recall exactly.
    */
    int engineFromParameter() const noexcept
    {
        return prm.pitchEngine != nullptr && prm.pitchEngine->load() < 0.5f ? 1 : 0;
    }

    void bindParameters();
    void setParameterNotifying(const char* id, float value);
    void applyAutoMix();
    // The capture finishes on the audio thread; setting parameters belongs to
    // the message thread, so completion is handed over rather than acted on.
    void handleAsyncUpdate() override;

    ProfileTransfer publishedProfile, pendingProfile;
    ValueTransfer<voxera::VoiceMatch::Reference> publishedReference, pendingReference;
    std::atomic<bool> referenceCapturing { false }, referenceReady { false };
    std::atomic<float> referenceCaptureProgress { 0.0f };
    std::atomic<bool> captureRequested { false };
    std::atomic<bool> autoMixRequested { false };
    std::atomic<bool> referenceRequested { false };
    voxera::IntegerDelay dryDelay;
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float> globalWet;
    int preparedBlockSize = 1, scopeDecimation = 0, scopeWrite = 0;
    int currentProgram = 0;
    AutoGain autoGain;
    PitchEngine pitchEngine;
    AdaptiveSpectralEngine spectralEngine;
    SmartEQ smartEQ;
    voxera::ColourCompressor compressor;
    voxera::PluginSlot insertSlot;
    Saturator saturator;
    SpatialEngine spatialEngine;
    VoiceProfileEngine voiceProfile;
    voxera::Gate gate;
    voxera::Optical optical;
    voxera::Upward upward;
    voxera::VocalLock vocalLock;
    voxera::VoiceMatch voiceMatch;
    voxera::Punch punch;
    voxera::Exciter exciter;
    voxera::Character character;
    voxera::Crush crush;
    voxera::Modulation modulation;
    voxera::Chop chop;
    voxera::NeuralStage neural;
    voxera::Glue glue;
    voxera::SoftClip softClip;
    voxera::Limiter limiter;

    std::vector<std::pair<juce::String, float>> autoMixBefore, autoMixAfter;
    bool comparingBefore = false;
    std::atomic<bool> autoMixHistoryValid { false };
    std::atomic<bool> autoMixComplete { false };
    voxera::LevelMatch levelMatch;
    std::atomic<float>* levelMatchParameter {};
    std::atomic<float>* compColourParameter {};
    juce::String lastReport;
    // Kept so the session can reopen with the same capture in place.
    juce::File loadedNeuralFile;
    bool previousAnalyzeState = false;
    bool profileNeedsPublish = false;

    juce::SmoothedValue<float> inputGain;
    juce::SmoothedValue<float> outputGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoxeraAudioProcessor)
};

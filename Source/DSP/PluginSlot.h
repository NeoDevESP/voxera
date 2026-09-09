#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <utility>

namespace voxera
{
/*  An insert point for another plugin, inside this chain.

    The reason to want this is narrow and real: someone who already owns a
    compressor they trust would rather hear it in the position this chain puts a
    compressor in than run it afterwards, where it works on a signal the rest of
    the chain has already finished with. Position is most of what a chain is.

    Hosting is not free, and three of the costs are worth naming because all
    three are invisible until they bite.

    A hosted plugin has latency of its own, and unless that figure is added to
    what this plugin reports, everything else in the session drifts against it.
    It is queried after preparation and folded in.

    A hosted plugin has state of its own, and unless it is saved inside ours,
    reopening a session restores an insert with every control back at its
    default — which is worse than restoring nothing, because it looks like it
    worked.

    And a hosted plugin runs on our audio thread. If it allocates, blocks or
    crashes, it does so as us: the host will name VOXERA as the plugin that
    brought the session down. Nothing here can prevent that, so the loading path
    at least refuses clearly rather than half-succeeding.
*/
class PluginSlot
{
public:
    struct LoadResult
    {
        bool ok = false;
        juce::String message;
        int latencySamples = 0;
    };

    void prepare(double sampleRate, int maximumBlockSize, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        capacity = juce::jmax(1, maximumBlockSize);
        numChannels = juce::jlimit(1, 2, channels);

        mix.reset(sr, 0.050);
        mix.setCurrentAndTargetValue(mix.getTargetValue());
        scratch.setSize(juce::jmax(2, numChannels), capacity, false, true, true);
        // After the scratch exists, and sized from what the plugin asks for
        // rather than from our own channel count.
        if (current != nullptr) { prepareHosted(*current); sizeScratch(*current); }
    }

    void reset()
    {
        if (auto* hosted = active.load(std::memory_order_acquire)) hosted->reset();
        mix.setCurrentAndTargetValue(mix.getTargetValue());
    }

    void setMix(float normalised) { mix.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }

    bool hasPlugin() const noexcept { return active.load(std::memory_order_acquire) != nullptr; }
    const juce::String& pluginName() const noexcept { return loadedName; }
    const juce::File& pluginFile() const noexcept { return loadedFile; }
    int getLatencySamples() const noexcept { return hostedLatency; }

    /*  Builds the instance. Message thread only, with audio stopped.

        Scanning and instantiating both allocate, and either can take seconds
        because a licensed plugin may go and talk to its authorisation service
        on the way up. None of it can happen near the audio callback.
    */
    LoadResult load(const juce::File& file)
    {
        LoadResult result;
        if (!file.exists()) { result.message = "That file is not there."; return result; }

        /*  VST3 only, and named rather than taken from the default set.

            JUCE 9 removed the convenience that added every format at once, and
            the removal is convenient here: this build hosts the one format it
            has been given the SDK for, and adding others would advertise
            support that has never been tried.
        */
        if (formats.getNumFormats() == 0) formats.addFormat(new juce::VST3PluginFormat());

        juce::OwnedArray<juce::PluginDescription> found;
        for (int i = 0; i < formats.getNumFormats(); ++i)
            formats.getFormat(i)->findAllTypesForFile(found, file.getFullPathName());

        if (found.isEmpty()) {
            result.message = "Nothing loadable in that file. It has to be a VST3 this build can open, "
                             "and the same architecture: a 32-bit plugin cannot run here.";
            return result;
        }

        juce::String error;
        auto instance = formats.createPluginInstance(*found.getFirst(), sr, capacity, error);
        if (instance == nullptr) {
            result.message = "Could not start it: " + error;
            return result;
        }

        prepareHosted(*instance);
        if (instance->getTotalNumOutputChannels() < 1) {
            result.message = "That plugin would not give a usable output bus.";
            return result;
        }

        /*  The new buffer is allocated here, while nothing is holding the audio
            thread, so the swap below has nothing left to do but exchange
            pointers.
        */
        const int wide = juce::jmax(2, instance->getTotalNumInputChannels(),
                                       instance->getTotalNumOutputChannels());
        pendingScratch.setSize(wide, juce::jmax(capacity, 1), false, true, true);

        pending = std::move(instance);
        pendingName = found.getFirst()->name;
        pendingFile = file;

        result.ok = true;
        result.latencySamples = pending->getLatencySamples();
        result.message = "Loaded " + pendingName;
        return result;
    }

    // Called with audio stopped, so the previous instance is safe to release.
    void commitLoad()
    {
        /*  The swap, and nothing but the swap.

            This is called while the caller holds the lock the host uses around
            processBlock, so every line of it runs with the audio thread
            certainly outside this object. That is also why there is no work
            here beyond exchanging pointers: the buffer was allocated during
            load, and the plugin being replaced is set aside rather than
            destroyed, because tearing down a VST3 can take long enough to be
            heard if it happens while audio is waiting.
        */
        retired = std::move(current);
        active.store(pending.get(), std::memory_order_release);
        current = std::move(pending);
        // std::swap on the buffers themselves: AudioBuffer has no swap of its
        // own, and moving is what makes this free rather than a reallocation.
        std::swap(scratch, pendingScratch);
        loadedName = pendingName;
        loadedFile = pendingFile;
        hostedLatency = current != nullptr ? current->getLatencySamples() : 0;
    }

    /*  Destroys whatever the last swap set aside. Message thread, no lock.

        Kept separate so that the instance being replaced is released after the
        audio thread has been let go, not while it is waiting: a licensed plugin
        can take a noticeable moment to shut down, and doing that inside the
        callback lock would put that moment straight into the audio.
    */
    void releaseRetired() { retired.reset(); }

    void unload()
    {
        /*  The instance is simply released.

            An earlier version called editorBeingDeleted here, which is not what
            that function is for: it is how an editor tells its processor that
            it is going away, not how a processor disposes of one. Whoever
            opened the window is the one that has to close it, and the editor
            in this project does exactly that before calling here.
        */
        active.store(nullptr, std::memory_order_release);
        // Set aside rather than destroyed here, for the same reason as above.
        retired = std::move(current);
        pending.reset();
        loadedName = {};
        loadedFile = {};
        hostedLatency = 0;
    }

    /*  The hosted plugin's own settings, so a session restores what was set.

        Kept as the opaque block the plugin hands over rather than anything this
        code interprets: its meaning belongs to whoever wrote it, and the only
        correct thing to do with it is hand it back unchanged.
    */
    juce::MemoryBlock getHostedState() const
    {
        juce::MemoryBlock state;
        if (current != nullptr) current->getStateInformation(state);
        return state;
    }

    void setHostedState(const juce::MemoryBlock& state)
    {
        if (current != nullptr && state.getSize() > 0)
            current->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    }

    /*  What the hosted plugin calls its own controls.

        Message thread only. Reading a name is cheap but reading it inside the
        callback would still be asking somebody else's code a question while
        audio is waiting on the answer.
    */
    juce::StringArray parameterNames() const
    {
        juce::StringArray names;
        if (current != nullptr)
            for (auto* parameter : current->getParameters())
                names.add(parameter->getName(64));
        return names;
    }

    /*  The controls a person would recognise, with the boilerplate left out.

        Asking a real plugin what it exposes is sobering: the LA-2A used to test
        this reports two thousand and eighty-eight parameters, of which eight
        are knobs and the rest are MIDI CC mappings it publishes because the
        format lets it. Listing all of them is useless to a person and searching
        all of them is worse than useless, because a keyword can match a CC slot
        and then the wrong thing moves.
    */
    static bool isBoilerplate(const juce::String& name)
    {
        return name.startsWithIgnoreCase("MIDI CC") || name.containsIgnoreCase("Master Bypass");
    }

    juce::Array<int> realParameters() const
    {
        juce::Array<int> indices;
        if (current == nullptr) return indices;
        const auto& parameters = current->getParameters();
        for (int i = 0; i < parameters.size(); ++i)
            if (!isBoilerplate(parameters[i]->getName(64))) indices.add(i);
        return indices;
    }

    /*  Finds the control that does a particular job, by what it is called.

        This is a heuristic and is worth saying so plainly. There is no standard
        that tells a host which knob on a compressor is the one that decides how
        hard it works: the format hands over a list of names and nothing else.
        Matching on words the industry actually uses gets the common cases —
        an opto unit calls it Peak Reduction, most others call it Threshold —
        and will simply fail to find anything on a plugin that names its
        controls some other way.

        Failing to find is the right failure. Guessing an index would move a
        control that happens to sit in that position, and moving the wrong knob
        confidently is worse than moving none.
    */
    int findParameter(const juce::StringArray& words) const
    {
        if (current == nullptr) return -1;
        const auto& parameters = current->getParameters();

        // Words in the order given, so the caller decides which naming it
        // prefers, and the boilerplate skipped so a keyword cannot land on a
        // MIDI mapping that happens to contain it.
        for (const auto& word : words)
            for (int i = 0; i < parameters.size(); ++i) {
                const auto name = parameters[i]->getName(64);
                if (!isBoilerplate(name) && name.containsIgnoreCase(word)) return i;
            }
        return -1;
    }

    // Normalised, because that is the only scale the format guarantees.
    float getParameter(int index) const
    {
        if (current == nullptr) return 0.0f;
        const auto& parameters = current->getParameters();
        return juce::isPositiveAndBelow(index, parameters.size()) ? parameters[index]->getValue() : 0.0f;
    }

    juce::String parameterText(int index) const
    {
        if (current == nullptr) return {};
        const auto& parameters = current->getParameters();
        return juce::isPositiveAndBelow(index, parameters.size())
             ? parameters[index]->getCurrentValueAsText() : juce::String();
    }

    /*  Moves one of its controls. Message thread only.

        Announced to the plugin the way a host announces it, so the plugin
        redraws and records the change rather than finding its own control
        somewhere it did not put it.
    */
    void setParameter(int index, float normalised)
    {
        if (current == nullptr) return;
        const auto& parameters = current->getParameters();
        if (!juce::isPositiveAndBelow(index, parameters.size())) return;
        parameters[index]->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalised));
    }

    /*  The control that decides how hard the hosted unit works.

        Named differently by almost every design, and the list below is what
        those names actually are rather than what they ought to be. An opto
        compressor calls it Peak Reduction and abbreviates it — the LA-2A tested
        here says "Peak Reduct" — while most others call it Threshold, and a few
        call it Input because driving the input harder is how you drive them.

        Ordered most specific first. "Input" is last precisely because it is the
        vaguest: on a unit that has both, it is not the one that decides the
        compression.
    */
    int findAmountControl() const
    {
        return findParameter({ "Peak Reduct", "Peak Reduction", "Threshold",
                               "Compression", "Amount", "Input" });
    }

    /*  The hosted plugin's own window, if it has one.

        Handed out rather than owned, because whoever opens it decides when it
        closes — but the instance behind it is owned here, so the caller has to
        close it before this slot is unloaded. That contract is the reason
        unload() below is not simply a reset: the editor has to go first.
    */
    juce::AudioProcessorEditor* createHostedEditor()
    {
        return current != nullptr && current->hasEditor() ? current->createEditorIfNeeded() : nullptr;
    }

    bool hasHostedEditor() const noexcept { return current != nullptr && current->hasEditor(); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        auto* hosted = active.load(std::memory_order_acquire);
        const int count = buffer.getNumSamples();
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        if (hosted == nullptr || count == 0 || channels <= 0) { mix.skip(count); return; }

        /*  Handed as many channels as it was prepared for, not as many as we
            have. A stereo plugin given a one-channel buffer still writes to the
            channel it believes is there, and what it writes into is memory
            belonging to something else.
        */
        const int wide = juce::jmax(1, juce::jmax(hosted->getTotalNumInputChannels(),
                                                  hosted->getTotalNumOutputChannels()));
        if (scratch.getNumChannels() < wide) { mix.skip(count); return; }

        /*  Never handed more samples at once than it was prepared for.

            A plugin allocates its buffers in prepareToPlay from the block size
            it was given, and a host is entitled to send a longer block than it
            promised. Passing that straight through is how a hosted plugin
            writes past the end of its own storage, and the crash lands on
            whoever is hosting rather than on whoever wrote it.
        */
        const int slice = juce::jmax(1, juce::jmin(capacity, scratch.getNumSamples()));

        for (int at = 0; at < count; at += slice)
        {
            const int piece = juce::jmin(slice, count - at);

            for (int ch = 0; ch < wide; ++ch)
                scratch.copyFrom(ch, 0, buffer, juce::jmin(ch, channels - 1), at, piece);

            juce::AudioBuffer<float> view(scratch.getArrayOfWritePointers(), wide, 0, piece);
            /*  A MIDI buffer it can write into, kept as a member and emptied
                each time rather than made here. Clearing a MidiBuffer keeps the
                storage it already has, so this costs nothing on the audio
                thread, while a local one would allocate on the first plugin
                that sends anything.
            */
            hostedMidi.clear();
            hosted->processBlock(view, hostedMidi);

            for (int i = 0; i < piece; ++i) {
                const float wet = mix.getNextValue();
                for (int ch = 0; ch < channels; ++ch) {
                    const float processed = scratch.getSample(juce::jmin(ch, wide - 1), i);
                    auto& sample = buffer.getWritePointer(ch)[at + i];
                    // Guarded, because what came back is not code this project
                    // wrote, and a stage downstream should not have to cope with
                    // a value that is not a number.
                    sample += wet * ((std::isfinite(processed) ? processed : sample) - sample);
                }
            }
        }
    }

private:
    /*  Asks the hosted plugin for a plain main bus and nothing else.

        This was the fault behind both of the symptoms this class first shipped
        with. Almost every compressor worth inserting has a sidechain input, so
        taking whatever bus layout the plugin happened to default to meant its
        channel count came back as four rather than two — and then the guard in
        process() found a scratch buffer too narrow and quietly declined to run
        it at all. A compressor that loads, reports its latency, appears in the
        interface and does nothing to the sound.

        Left unconfigured it is also a crash waiting to happen, because a plugin
        told it has four channels will read and write four, and the buffer handed
        to it has two.

        So the extra buses are switched off explicitly and the main one is asked
        for stereo, then mono. If the plugin will accept neither, that is worth
        knowing at load time rather than discovering as silence.
    */
    static bool configureBuses(juce::AudioPluginInstance& instance, int wanted)
    {
        auto layout = instance.getBusesLayout();

        for (int i = 1; i < layout.inputBuses.size(); ++i)
            layout.inputBuses.getReference(i) = juce::AudioChannelSet::disabled();
        for (int i = 1; i < layout.outputBuses.size(); ++i)
            layout.outputBuses.getReference(i) = juce::AudioChannelSet::disabled();

        for (const auto& set : { wanted >= 2 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono(),
                                 juce::AudioChannelSet::stereo(),
                                 juce::AudioChannelSet::mono() })
        {
            if (!layout.inputBuses.isEmpty())  layout.inputBuses.getReference(0) = set;
            if (!layout.outputBuses.isEmpty()) layout.outputBuses.getReference(0) = set;
            if (instance.setBusesLayout(layout)) return true;
        }
        return false;
    }

    void prepareHosted(juce::AudioPluginInstance& instance)
    {
        configureBuses(instance, numChannels);
        instance.setPlayConfigDetails(instance.getTotalNumInputChannels(),
                                      instance.getTotalNumOutputChannels(), sr, capacity);
        instance.prepareToPlay(sr, capacity);
        instance.setNonRealtime(false);
    }

    // Wide enough for whatever the hosted plugin ended up wanting, worked out
    // once at load rather than assumed to be two.
    void sizeScratch(const juce::AudioPluginInstance& instance)
    {
        const int wide = juce::jmax(2, instance.getTotalNumInputChannels(),
                                       instance.getTotalNumOutputChannels());
        scratch.setSize(wide, juce::jmax(capacity, scratch.getNumSamples()), false, true, true);
    }

    juce::AudioPluginFormatManager formats;
    std::atomic<juce::AudioPluginInstance*> active { nullptr };
    std::unique_ptr<juce::AudioPluginInstance> current, pending, retired;
    juce::AudioBuffer<float> scratch, pendingScratch;
    juce::MidiBuffer hostedMidi;
    juce::SmoothedValue<float> mix;

    juce::String loadedName, pendingName;
    juce::File loadedFile, pendingFile;
    double sr = 48000.0;
    int capacity = 512, numChannels = 2, hostedLatency = 0;
};
}

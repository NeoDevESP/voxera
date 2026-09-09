#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>

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

        if (current != nullptr) prepareHosted(*current);
        mix.reset(sr, 0.050);
        mix.setCurrentAndTargetValue(mix.getTargetValue());
        scratch.setSize(juce::jmax(2, numChannels), capacity, false, true, true);
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
        active.store(pending.get(), std::memory_order_release);
        current = std::move(pending);
        loadedName = pendingName;
        loadedFile = pendingFile;
        hostedLatency = current != nullptr ? current->getLatencySamples() : 0;
    }

    void unload()
    {
        active.store(nullptr, std::memory_order_release);
        current.reset();
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
        if (scratch.getNumChannels() < wide || scratch.getNumSamples() < count) { mix.skip(count); return; }

        for (int ch = 0; ch < wide; ++ch)
            scratch.copyFrom(ch, 0, buffer, juce::jmin(ch, channels - 1), 0, count);

        juce::AudioBuffer<float> view(scratch.getArrayOfWritePointers(), wide, 0, count);
        /*  A MIDI buffer it can write into, kept as a member and emptied each
            time rather than made here. Clearing a MidiBuffer keeps the storage
            it already has, so this costs nothing on the audio thread, while a
            local one would allocate on the first plugin that sends anything.
        */
        hostedMidi.clear();
        hosted->processBlock(view, hostedMidi);

        for (int i = 0; i < count; ++i) {
            const float wet = mix.getNextValue();
            for (int ch = 0; ch < channels; ++ch) {
                const float processed = scratch.getSample(juce::jmin(ch, wide - 1), i);
                auto& sample = buffer.getWritePointer(ch)[i];
                // Guarded, because what came back is not code this project wrote,
                // and a stage downstream should not have to cope with a value
                // that is not a number.
                sample += wet * ((std::isfinite(processed) ? processed : sample) - sample);
            }
        }
    }

private:
    void prepareHosted(juce::AudioPluginInstance& instance)
    {
        instance.setPlayConfigDetails(instance.getTotalNumInputChannels(),
                                      instance.getTotalNumOutputChannels(), sr, capacity);
        instance.prepareToPlay(sr, capacity);
    }

    juce::AudioPluginFormatManager formats;
    std::atomic<juce::AudioPluginInstance*> active { nullptr };
    std::unique_ptr<juce::AudioPluginInstance> current, pending;
    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer hostedMidi;
    juce::SmoothedValue<float> mix;

    juce::String loadedName, pendingName;
    juce::File loadedFile, pendingFile;
    double sr = 48000.0;
    int capacity = 512, numChannels = 2, hostedLatency = 0;
};
}

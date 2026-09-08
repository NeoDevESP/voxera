#pragma once
#include <JuceHeader.h>
#include <RTNeural/RTNeural.h>
#include <NAM/dsp.h>
#include <NAM/get_dsp.h>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace voxera
{
/*  Runs a trained network over the signal as an analogue-colour stage.

    Two file formats are accepted, because the models worth loading are split
    across both. RTNeural's own JSON is what comes out of Keras and out of the
    Aida-X tools. The `.nam` files from Neural Amp Modeler are the larger
    library by far, and although that project is aimed at guitar amplifiers the
    same capture process works on a microphone preamp or a console channel,
    which is what makes it useful here.

    The `.nam` files are read by the Neural Amp Modeler core itself rather than
    by a loader written here. That was not the original arrangement and the
    change was forced by evidence. A hand-written reader had the weight layout
    wrong — it read the input and hidden matrices as two blocks where the format
    interleaves them by row, and stepped past the trained initial state onto the
    output head — so every real capture loaded without complaint and produced a
    different amplifier. Using the reference implementation makes that class of
    fault impossible rather than merely fixed.

    It also decides which models exist at all. Of the first forty-one captures
    returned for "preamp" on the largest public library, forty-one were WaveNet
    and none were LSTM, so a stage that read only LSTM was a stage that would
    never load anything anyone would actually download.

    What is still refused is size, not architecture. WaveNet comes in several
    widths, and the widest — sixteen channels, what that project calls
    "standard" — costs several times the rest of this chain put together. The
    narrower ones are a fraction of that and are what a vocal wants anyway,
    since the job here is colour rather than reproducing a cabinet. The limit
    below is on channel count for that reason, and it says so when it refuses.

    No captures are bundled. A capture uploaded by someone else carries its own
    terms, and the hardware it models carries a name that is not ours to print.
    Capturing your own gear avoids both questions and is the intended path.
*/
class NeuralStage
{
public:
    // Errors are reported to the caller rather than thrown: this is loaded in
    // response to a file chooser, and the user needs to be told what was wrong.
    struct LoadResult
    {
        bool ok = false;
        juce::String message;
        double modelSampleRate = 0.0;
    };

private:
    /*  Whichever of the two kinds of model is loaded, behind one interface.

        Holding both in one object is what lets the audio thread follow a single
        pointer. Two separate pointers would need two atomics and a rule about
        which to read first, and a swap between formats would have a window
        where both or neither were live.
    */
    struct Loaded
    {
        std::unique_ptr<RTNeural::Model<float>> rtneural;
        std::unique_ptr<nam::DSP> amp;

        void prepare(double rate, int maxFrames)
        {
            if (rtneural != nullptr) rtneural->reset();
            // The core allocates inside Reset, so this belongs on the message
            // thread with audio stopped, never in the processing call.
            if (amp != nullptr) amp->Reset(rate, maxFrames);
        }

        void run(const float* in, float* out, int count)
        {
            if (amp != nullptr) {
                // The core takes channel pointers even for a mono model.
                auto* source = const_cast<float*>(in);
                float* inputs[1] { source };
                float* outputs[1] { out };
                amp->process(inputs, outputs, count);
            } else if (rtneural != nullptr) {
                for (int i = 0; i < count; ++i) {
                    float sample[1] { in[i] };
                    out[i] = rtneural->forward(sample);
                }
            }
        }
    };

public:

    void prepare(double sampleRate, int maximumBlockSize, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);
        capacity = juce::jmax(1, maximumBlockSize);

        // Worst case is the session running slower than the model, where one
        // block of input becomes more than a block of model-rate samples.
        modelBuffer.assign(static_cast<size_t>(capacity) * 4u + 16u, 0.0f);
        outBuffer.assign(static_cast<size_t>(capacity) + 16u, 0.0f);

        updateRatio();
        mix.reset(sr, 0.050);
        mix.setCurrentAndTargetValue(0.0f);
        reset();
    }

    void reset()
    {
        if (auto* model = active.load(std::memory_order_acquire)) model->prepare(loadedRate > 0.0 ? loadedRate : sr, capacity * 2 + 16);
        down.reset();
        up.reset();
        carry = 0.0;
        mix.setCurrentAndTargetValue(mix.getTargetValue());
    }

    void setMix(float normalised) { mix.setTargetValue(juce::jlimit(0.0f, 1.0f, normalised)); }

    bool hasModel() const noexcept { return active.load(std::memory_order_acquire) != nullptr; }
    const juce::String& modelName() const noexcept { return loadedName; }
    double modelSampleRate() const noexcept { return loadedRate; }

    /*  Builds the model. Message thread only.

        The caller must stop audio around the swap — the pointer handed to the
        audio thread has to stop being read before the object behind it is
        destroyed, and a plugin already has a way to do that.
    */
    LoadResult load(const juce::File& file)
    {
        LoadResult result;
        if (!file.existsAsFile()) { result.message = "File not found."; return result; }

        auto json = nlohmann::json {};
        try { json = nlohmann::json::parse(file.loadFileAsString().toStdString()); }
        catch (...) { result.message = "Not readable JSON."; return result; }

        auto built = std::make_unique<Loaded>();
        double rate = 0.0;

        try {
            if (json.contains("architecture")) {
                if (tooExpensive(json, result.message)) return result;

                // Handed the path rather than the parsed JSON: the core reads
                // its own format, which is the entire point of using it.
                built->amp = nam::get_dsp(std::filesystem::path(file.getFullPathName().toStdString()));
                if (built->amp == nullptr) { result.message = "The core could not build that capture."; return result; }
                rate = built->amp->GetExpectedSampleRate();
                if (!(rate > 0.0)) rate = 48000.0;   // older captures do not record one
            } else {
                built->rtneural = RTNeural::json_parser::parseJson<float>(json, true);
                if (built->rtneural == nullptr) { result.message = "Unrecognised RTNeural model."; return result; }
                if (built->rtneural->getInSize() != 1 || built->rtneural->getOutSize() != 1) {
                    result.message = "This stage needs a model with one input and one output; that one has "
                                   + juce::String(built->rtneural->getInSize()) + " and "
                                   + juce::String(built->rtneural->getOutSize()) + ".";
                    return result;
                }
            }
        } catch (const std::exception& e) {
            result.message = juce::String("Could not build the model: ") + e.what();
            return result;
        }

        // Sized for the widest block the resampler can ask for: a session slower
        // than the capture turns one host block into rather more than one.
        built->prepare(rate, capacity * 2 + 16);
        pending = std::move(built);
        loadedName = file.getFileNameWithoutExtension();
        loadedRate = rate;

        result.ok = true;
        result.modelSampleRate = rate;
        result.message = "Loaded " + loadedName;
        return result;
    }

    // Called with audio stopped, so the previous model is safe to release.
    void commitLoad()
    {
        active.store(pending.get(), std::memory_order_release);
        current = std::move(pending);
        updateRatio();
    }

    void unload()
    {
        active.store(nullptr, std::memory_order_release);
        current.reset();
        pending.reset();
        loadedName = {};
        loadedRate = 0.0;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        auto* model = active.load(std::memory_order_acquire);
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        if (model == nullptr || channels <= 0 || buffer.getNumSamples() == 0) {
            mix.skip(buffer.getNumSamples());
            return;
        }

        /*  The network is monophonic, so it is driven from the sum and the one
            result is applied to both channels. Running it twice would double the
            cost for a difference nobody can hear on a vocal, and would also let
            the two channels drift apart inside a recurrent layer that carries
            state from sample to sample.
        */
        const int count = buffer.getNumSamples();

        // Mono sum in: the network has one input, and running it twice would
        // double the cost while letting the two channels drift apart inside a
        // layer that carries state from sample to sample.
        for (int i = 0; i < count; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < channels; ++ch) sum += buffer.getReadPointer(ch)[i];
            outBuffer[static_cast<size_t>(i)] = sum / static_cast<float>(channels);
        }

        if (resampling) runResampled(model, count);
        else            runDirect(model, count);

        for (int i = 0; i < count; ++i) {
            const float wet = mix.getNextValue();
            const float processed = outBuffer[static_cast<size_t>(i)];
            for (int ch = 0; ch < channels; ++ch) {
                auto& value = buffer.getWritePointer(ch)[i];
                value += wet * (processed - value);
            }
        }
    }

private:
    // A network can be driven into producing anything; a stage in the middle of
    // a chain must never pass on something that is not a number, or every meter
    // and filter after it is poisoned.
    static float guard(float x) noexcept
    {
        return std::isfinite(x) ? juce::jlimit(-4.0f, 4.0f, x) : 0.0f;
    }

    void runDirect(Loaded* model, int count)
    {
        // Through a separate destination rather than in place: the core is not
        // documented as tolerating aliased buffers, and scratch is already here.
        model->run(outBuffer.data(), modelBuffer.data(), count);
        for (int i = 0; i < count; ++i)
            outBuffer[static_cast<size_t>(i)] = guard(modelBuffer[static_cast<size_t>(i)]);
    }

    /*  Runs the network at the rate it was trained on.

        A capture is a recurrent network whose state advances once per sample,
        so its time constants are defined in samples and not in seconds. Feed it
        44.1 kHz audio when it learned at 48 and every one of them stretches by
        nine per cent: the attack of the modelled circuit changes, and the
        capture stops being a capture. Resampling around it is what makes the
        session rate stop mattering.

        The fractional carry is the part that has to be right. Asking for a
        rounded number of model-rate samples each block would drift against the
        input by a fraction of a sample every time, and that accumulates into
        audible pitch error over a few minutes. Keeping the remainder means the
        long-run average is exact.
    */
    void runResampled(Loaded* model, int count)
    {
        const double wanted = static_cast<double>(count) / ratio + carry;
        int modelCount = static_cast<int>(wanted);
        carry = wanted - static_cast<double>(modelCount);
        modelCount = juce::jlimit(1, static_cast<int>(modelBuffer.size()), modelCount);

        down.process(ratio, outBuffer.data(), modelBuffer.data(), modelCount);

        model->run(modelBuffer.data(), modelBuffer.data(), modelCount);
        for (int i = 0; i < modelCount; ++i)
            modelBuffer[static_cast<size_t>(i)] = guard(modelBuffer[static_cast<size_t>(i)]);

        up.process(1.0 / ratio, modelBuffer.data(), outBuffer.data(), count);
    }

    void updateRatio()
    {
        // Below a tenth of a percent the difference is inaudible and not worth
        // two interpolators in the path.
        resampling = loadedRate > 0.0 && std::abs(loadedRate - sr) / sr > 0.001;
        ratio = resampling ? sr / loadedRate : 1.0;
        down.reset();
        up.reset();
        carry = 0.0;
    }

    /*  Refuses a capture that would cost more than the rest of the plugin.

        The core will happily load any width; the judgement about whether this
        plugin can afford it belongs here. WaveNet channel count is what decides
        the arithmetic — sixteen is the "standard" size, and twelve, eight and
        four are the progressively cheaper ones — so that is what is read and
        what the message names, since a refusal that does not say what to look
        for instead is a dead end.
    */
    bool tooExpensive(const nlohmann::json& json, juce::String& message) const
    {
        if (!json.contains("config")) return false;
        const auto& config = json.at("config");
        if (!config.contains("layers")) return false;

        int widest = 0;
        for (const auto& layer : config.at("layers"))
            widest = juce::jmax(widest, layer.value("channels", 0));

        if (widest > maxChannels) {
            message = "That capture is " + juce::String(widest) + " channels wide, which costs "
                      "several times the rest of this chain. Look for a lite, feather or nano "
                      "capture instead — up to " + juce::String(maxChannels) + " channels.";
            return true;
        }
        return false;
    }

    // Twelve keeps NAM's lite, feather and nano sizes and turns away standard.
    static constexpr int maxChannels = 12;

    std::atomic<Loaded*> active { nullptr };
    std::unique_ptr<Loaded> current, pending;
    juce::SmoothedValue<float> mix;
    juce::String loadedName;

    juce::LagrangeInterpolator down, up;
    std::vector<float> modelBuffer, outBuffer;
    double ratio = 1.0, carry = 0.0;
    bool resampling = false;

    double sr = 48000.0, loadedRate = 0.0;
    int numChannels = 2, capacity = 1;
};
}

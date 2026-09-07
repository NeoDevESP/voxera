#pragma once
#include <JuceHeader.h>
#include <RTNeural/RTNeural.h>
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

    Only the LSTM architecture is loaded from `.nam`. That is a deliberate
    limit rather than an unfinished one: the WaveNet models in that ecosystem
    are accurate but cost on the order of a tenth of a core each, and this whole
    chain currently runs in a tenth of a core. A model that triples the plugin's
    cost is not a model this plugin can offer.

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

    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);
        mix.reset(sr, 0.050);
        mix.setCurrentAndTargetValue(0.0f);
        reset();
    }

    void reset()
    {
        if (auto* model = active.load(std::memory_order_acquire)) model->reset();
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

        std::unique_ptr<RTNeural::Model<float>> built;
        double rate = 0.0;

        try {
            if (json.contains("architecture")) {
                built = buildFromNam(json, rate, result.message);
                if (built == nullptr) return result;
            } else {
                built = RTNeural::json_parser::parseJson<float>(json, true);
                if (built == nullptr) { result.message = "Unrecognised RTNeural model."; return result; }
            }
        } catch (const std::exception& e) {
            result.message = juce::String("Could not build the model: ") + e.what();
            return result;
        }

        if (built->getInSize() != 1 || built->getOutSize() != 1) {
            result.message = "This stage needs a model with one input and one output; that one has "
                           + juce::String(built->getInSize()) + " and " + juce::String(built->getOutSize()) + ".";
            return result;
        }

        built->reset();
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
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float wet = mix.getNextValue();

            float input = 0.0f;
            for (int ch = 0; ch < channels; ++ch) input += buffer.getReadPointer(ch)[i];
            input /= static_cast<float>(channels);

            float sample[1] { input };
            const float output = model->forward(sample);

            // A network can be pushed into producing anything; a stage in the
            // middle of a chain must never hand on something that is not a
            // number, or every meter and filter after it is poisoned.
            const float safe = std::isfinite(output) ? juce::jlimit(-4.0f, 4.0f, output) : 0.0f;

            for (int ch = 0; ch < channels; ++ch) {
                auto& value = buffer.getWritePointer(ch)[i];
                value += wet * (safe - value);
            }
        }
    }

private:
    /*  Unpacks a Neural Amp Modeler LSTM capture.

        The file stores every weight in one flat array, in the order the
        reference implementation writes them: the input-to-hidden and
        hidden-to-hidden matrices of each layer, then that layer's biases, then
        finally the head that reduces the hidden state to one sample. NAM orders
        its gates i, f, g, o, which is the order RTNeural expects as well, so the
        matrices transfer without rearrangement — only the split points have to
        be right.
    */
    std::unique_ptr<RTNeural::Model<float>> buildFromNam(const nlohmann::json& json,
                                                         double& rate, juce::String& message)
    {
        const auto architecture = juce::String(json.value("architecture", std::string {}));
        if (!architecture.equalsIgnoreCase("LSTM")) {
            message = "This build loads LSTM captures only; that file is \"" + architecture
                    + "\". WaveNet models cost far more CPU than the rest of this plugin put together.";
            return nullptr;
        }

        const auto& config = json.at("config");
        const int layers = config.value("num_layers", 1);
        const int inputSize = config.value("input_size", 1);
        const int hidden = config.value("hidden_size", 0);
        rate = json.value("sample_rate", 48000.0);

        if (layers != 1 || inputSize != 1 || hidden <= 0) {
            message = "Only single-layer, single-input captures are supported here.";
            return nullptr;
        }

        const std::vector<float> weights = json.at("weights").get<std::vector<float>>();
        const size_t expected = static_cast<size_t>(4 * hidden * (inputSize + hidden))  // W and U
                              + static_cast<size_t>(4 * hidden)                          // gate biases
                              + static_cast<size_t>(hidden) + 1;                         // head and its bias
        if (weights.size() < expected) {
            message = "The capture is shorter than its own configuration describes.";
            return nullptr;
        }

        auto model = std::make_unique<RTNeural::Model<float>>(1);
        auto lstm = std::make_unique<RTNeural::LSTMLayer<float>>(inputSize, hidden);
        auto dense = std::make_unique<RTNeural::Dense<float>>(hidden, 1);

        size_t at = 0;
        const auto take = [&weights, &at](int rows, int columns) {
            std::vector<std::vector<float>> out(static_cast<size_t>(rows),
                                                std::vector<float>(static_cast<size_t>(columns)));
            for (int r = 0; r < rows; ++r)
                for (int c = 0; c < columns; ++c) out[static_cast<size_t>(r)][static_cast<size_t>(c)] = weights[at++];
            return out;
        };

        lstm->setWVals(take(inputSize, 4 * hidden));
        lstm->setUVals(take(hidden, 4 * hidden));

        std::vector<float> gateBias(static_cast<size_t>(4 * hidden));
        for (auto& b : gateBias) b = weights[at++];
        lstm->setBVals(gateBias);

        std::vector<std::vector<float>> headWeights(1, std::vector<float>(static_cast<size_t>(hidden)));
        for (int h = 0; h < hidden; ++h) headWeights[0][static_cast<size_t>(h)] = weights[at++];
        dense->setWeights(headWeights);
        const float headBias = weights[at++];
        dense->setBias(&headBias);

        model->addLayer(lstm.release());
        model->addLayer(dense.release());
        return model;
    }

    std::atomic<RTNeural::Model<float>*> active { nullptr };
    std::unique_ptr<RTNeural::Model<float>> current, pending;
    juce::SmoothedValue<float> mix;
    juce::String loadedName;
    double sr = 48000.0, loadedRate = 0.0;
    int numChannels = 2;
};
}

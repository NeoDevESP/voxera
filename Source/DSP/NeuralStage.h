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
        if (auto* model = active.load(std::memory_order_acquire)) model->reset();
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

    void runDirect(RTNeural::Model<float>* model, int count)
    {
        for (int i = 0; i < count; ++i) {
            float sample[1] { outBuffer[static_cast<size_t>(i)] };
            outBuffer[static_cast<size_t>(i)] = guard(model->forward(sample));
        }
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
    void runResampled(RTNeural::Model<float>* model, int count)
    {
        const double wanted = static_cast<double>(count) / ratio + carry;
        int modelCount = static_cast<int>(wanted);
        carry = wanted - static_cast<double>(modelCount);
        modelCount = juce::jlimit(1, static_cast<int>(modelBuffer.size()), modelCount);

        down.process(ratio, outBuffer.data(), modelBuffer.data(), modelCount);

        for (int i = 0; i < modelCount; ++i) {
            float sample[1] { modelBuffer[static_cast<size_t>(i)] };
            modelBuffer[static_cast<size_t>(i)] = guard(model->forward(sample));
        }

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

    /*  Unpacks a Neural Amp Modeler LSTM capture.

        The file stores every weight in one flat array. Getting the layout wrong
        does not fail: the model loads, runs, and produces confident nonsense
        that sounds like a different amplifier, so the layout is worth stating
        exactly. Per layer, the reference implementation writes

            the input and hidden weight matrices CONCATENATED SIDE BY SIDE,
              as one (4*hidden) x (input + hidden) matrix in row order —
              so each row holds that gate-row's input weights immediately
              followed by its hidden weights, interleaved all the way down
            the summed gate biases                              (4 * hidden)
            the initial hidden state                            (hidden)
            the initial cell state                              (hidden)

        and then, once per model, the head that reduces the hidden state to one
        sample, and the head's bias.

        Two things here were wrong for a long time and both were invisible.
        Reading the two matrices as separate contiguous blocks — which is the
        obvious reading of "input weights, then hidden weights" — takes the
        right number of values from the wrong places once the rows interleave.
        And the initial states sit between the biases and the head, so skipping
        them silently shifts the head onto somebody else's numbers.

        What hid it was the test: it built its fixture with the same layout the
        loader assumed, so the two agreed with each other and neither agreed
        with NAM. The test alongside this now writes the layout documented
        above and checks the result against an LSTM worked out independently,
        which is the only version of this test that can fail.

        NAM orders its gates i, f, g, o, which is the order RTNeural expects as
        well, so the values themselves transfer without rearrangement.
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

        /*  Stacked layers are ordinary in these captures, so refusing them was
            refusing most of what anyone would actually download. The format
            simply repeats the same per-layer block, and each layer after the
            first takes its input from the one below, so the only thing that
            changes down the stack is the width of the input half of the matrix.

            The limits that remain are real ones. A single audio input is what
            the format means for an amp capture, and WaveNet is refused above on
            CPU grounds rather than format ones.
        */
        if (layers < 1 || layers > 4 || inputSize != 1 || hidden <= 0 || hidden > 64) {
            message = "This stage takes LSTM captures of one to four layers, one input, "
                      "and up to 64 hidden units; that one is "
                    + juce::String(layers) + " x " + juce::String(hidden) + ".";
            return nullptr;
        }

        const std::vector<float> weights = json.at("weights").get<std::vector<float>>();

        size_t expected = static_cast<size_t>(hidden) + 1;   // the head and its bias
        for (int layer = 0; layer < layers; ++layer) {
            const int in = layer == 0 ? inputSize : hidden;
            expected += static_cast<size_t>(4 * hidden * (in + hidden))   // W and U together
                      + static_cast<size_t>(4 * hidden)                    // gate biases
                      + static_cast<size_t>(2 * hidden);                   // initial h and c
        }
        if (weights.size() < expected) {
            message = "The capture is shorter than its own configuration describes.";
            return nullptr;
        }

        auto model = std::make_unique<RTNeural::Model<float>>(1);
        auto dense = std::make_unique<RTNeural::Dense<float>>(hidden, 1);

        size_t at = 0;
        const int gates = 4 * hidden;

        for (int layer = 0; layer < layers; ++layer)
        {
            // Only the first layer takes the audio; the rest are fed the layer
            // below, so their input half is as wide as the hidden state.
            const int in = layer == 0 ? inputSize : hidden;
            const int stride = in + hidden;

            auto lstm = std::make_unique<RTNeural::LSTMLayer<float>>(in, hidden);

            /*  Walked row by row across the combined matrix, splitting each row
                at the input/hidden boundary. RTNeural wants the transpose of
                this — indexed by source first and gate second — so the two
                loops below scatter rather than copy.
            */
            std::vector<std::vector<float>> W(static_cast<size_t>(in),
                                              std::vector<float>(static_cast<size_t>(gates)));
            std::vector<std::vector<float>> U(static_cast<size_t>(hidden),
                                              std::vector<float>(static_cast<size_t>(gates)));
            for (int r = 0; r < gates; ++r) {
                const size_t row = at + static_cast<size_t>(r) * static_cast<size_t>(stride);
                for (int i = 0; i < in; ++i)
                    W[static_cast<size_t>(i)][static_cast<size_t>(r)] = weights[row + static_cast<size_t>(i)];
                for (int h = 0; h < hidden; ++h)
                    U[static_cast<size_t>(h)][static_cast<size_t>(r)] =
                        weights[row + static_cast<size_t>(in + h)];
            }
            at += static_cast<size_t>(gates) * static_cast<size_t>(stride);

            lstm->setWVals(W);
            lstm->setUVals(U);

            std::vector<float> gateBias(static_cast<size_t>(gates));
            for (auto& b : gateBias) b = weights[at++];
            lstm->setBVals(gateBias);

            /*  The trained initial state is stepped over rather than applied.

                It is what the network had settled to at the start of the
                capture, and a plugin does not start at the start of anything —
                it starts wherever the singer dropped in. RTNeural begins from
                zero, which after a handful of samples is where a stable
                recurrent layer ends up regardless. What matters is that these
                values are consumed here: leaving them in the stream shifts
                everything after them onto the wrong numbers, which is not an
                approximation but a different model.
            */
            at += static_cast<size_t>(2 * hidden);

            model->addLayer(lstm.release());
        }

        std::vector<std::vector<float>> headWeights(1, std::vector<float>(static_cast<size_t>(hidden)));
        for (int h = 0; h < hidden; ++h) headWeights[0][static_cast<size_t>(h)] = weights[at++];
        dense->setWeights(headWeights);
        const float headBias = weights[at++];
        dense->setBias(&headBias);

        model->addLayer(dense.release());
        return model;
    }

    std::atomic<RTNeural::Model<float>*> active { nullptr };
    std::unique_ptr<RTNeural::Model<float>> current, pending;
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

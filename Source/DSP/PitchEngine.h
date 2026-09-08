#pragma once
#include <JuceHeader.h>
#include <rubberband/RubberBandLiveShifter.h>
#include "YinPitchDetector.h"
#include "PitchQuantizer.h"
#include "PitchIntelligence.h"
#include "PitchSmoother.h"
#include "PsolaShifter.h"
#include <vector>
#include <memory>
#include <array>
#include <atomic>
#include <cmath>

/*
    VOXERA v0.4 pitch engine.

    Analysis:
      YIN/CMNDF -> voiced confidence -> musical quantizer -> hysteresis

    Resynthesis:
      RubberBandLiveShifter 4.0.0, formant-preserved.

    Important:
      Rubber Band LiveShifter has material latency. This is deliberately
      the MIX-quality path. A later VOXERA TRACK path will use a lower
      latency proprietary shifter.
*/
class PitchEngine
{
public:
    enum class Mode
    {
        natural = 0,
        modern,
        hard
    };

    /*  Which resynthesis method puts the corrected pitch back into the signal.

        Rubber Band is the general one and the safer default: it will shift
        anything, including a stereo take with room on it. PSOLA only works on a
        signal that has one clear period, which is to say one voice — but that
        is what this plugin is for, and in exchange it costs about a third of
        the latency and a fraction of the arithmetic, and it never has to
        reassign a phase, so the vowel comes out with its own texture rather
        than a reconstructed one.
    */
    enum class Engine
    {
        rubberBand = 0,
        psola
    };

    void prepare(double sampleRate, int maximumBlockSize, int channels)
    {
        sr = sampleRate;
        numChannels = juce::jlimit(1, 2, channels);

        detector.prepare(sr);
        quantizer.reset();
        // Intelligence runs once per shifter block; initialise below.

        using RBL = RubberBand::RubberBandLiveShifter;

        const int options =
            RBL::OptionFormantPreserved |
            RBL::OptionChannelsTogether;

        shifter = std::make_unique<RBL>(
            static_cast<size_t>(std::lround(sr)),
            static_cast<size_t>(numChannels),
            options);

        shifter->setDebugLevel(0);
        shifter->setPitchScale(1.0);
        shifter->setFormantScale(0.0);

        rbBlockSize = static_cast<int>(shifter->getBlockSize());
        rbStartDelay = static_cast<int>(shifter->getStartDelay());

        inputBlock.assign(
            static_cast<size_t>(numChannels),
            std::vector<float>(static_cast<size_t>(rbBlockSize), 0.0f));

        outputBlock.assign(
            static_cast<size_t>(numChannels),
            std::vector<float>(static_cast<size_t>(rbBlockSize), 0.0f));

        inputPtrs.resize(static_cast<size_t>(numChannels));
        outputPtrs.resize(static_cast<size_t>(numChannels));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            inputPtrs[static_cast<size_t>(ch)] =
                inputBlock[static_cast<size_t>(ch)].data();

            outputPtrs[static_cast<size_t>(ch)] =
                outputBlock[static_cast<size_t>(ch)].data();
        }

        // Queue has ample bounded headroom. In steady state we push/pop at
        // the same rate; the extra capacity avoids any host-block dependence.
        fifoCapacity = juce::jmax(
            rbBlockSize * 8,
            maximumBlockSize * 8 + rbBlockSize * 2);

        outputFifo.assign(
            static_cast<size_t>(numChannels),
            std::vector<float>(static_cast<size_t>(fifoCapacity), 0.0f));

        fifoRead = fifoWrite = fifoCount = 0;
        inFill = 0;

        // Adapter latency: one LiveShifter block must be collected before
        // the first shift() call. The shifter adds its own start delay.
        adapterDelay = rbBlockSize - 1;
        intelligence.prepare(sr / static_cast<double>(rbBlockSize));

        /*  Prepared whichever engine is selected, and the decision cadence is
            shared. The pitch intelligence and the retune smoother are both
            tuned in units of one decision per Rubber Band block, so PSOLA
            deciding at the same rate keeps that tuning meaningful instead of
            silently changing every time constant with the engine.
        */
        psola.prepare(sr, numChannels);

        pitchSmoother.reset();

        lastDetectedHz.store(0.0f);
        lastTargetHz.store(0.0f);
        lastConfidence.store(0.0f);
        lastShiftSemitones.store(0.0f);

        reset();
    }

    void reset()
    {
        detector.reset();
        quantizer.reset();
        intelligence.reset();

        if (shifter)
        {
            shifter->reset();
            shifter->setPitchScale(1.0);
            shifter->setFormantScale(0.0);
        }

        for (auto& ch : inputBlock)
            std::fill(ch.begin(), ch.end(), 0.0f);

        for (auto& ch : outputBlock)
            std::fill(ch.begin(), ch.end(), 0.0f);

        for (auto& ch : outputFifo)
            std::fill(ch.begin(), ch.end(), 0.0f);

        fifoRead = fifoWrite = fifoCount = 0;
        inFill = 0;
        decisionFill = 0;
        psola.reset();
        pitchSmoother.reset();
    }

    void setEnabled(bool v) noexcept { enabled = v; }
    void setRoot(int rootPitchClass) noexcept { root = rootPitchClass; }
    void setScale(int index) noexcept { scaleIndex = index; }
    void setMode(int index) noexcept { mode = static_cast<Mode>(juce::jlimit(0, 2, index)); }
    void setAmount(float amount01) noexcept { amount = juce::jlimit(0.0f, 1.0f, amount01); }
    void setRetune(float value01) noexcept { retune = juce::jlimit(0.0f, 1.0f, value01); }
    void setHumanize(float value01) noexcept { humanize = juce::jlimit(0.0f, 1.0f, value01); }
    void setFormantSemitones(float semitones) noexcept { formantSemitones = juce::jlimit(-12.0f, 12.0f, semitones); }
    void setEngine(int index) noexcept { engine = static_cast<Engine>(juce::jlimit(0, 1, index)); }
    Engine getEngine() const noexcept { return engine; }

    /*  Takes the shifter out of the signal path for tracking.

        Rubber Band LiveShifter accounts for essentially all of this plugin's
        latency — around 55 ms — which is unusable for a singer monitoring
        themselves through the chain. Everything else here costs a few
        milliseconds combined, so bypassing this one stage turns the plugin into
        something you can perform through, at the cost of the tuning.
    */
    void setShifterBypassed(bool bypassed) noexcept { shifterBypassed = bypassed; }
    bool isShifterBypassed() const noexcept { return shifterBypassed; }

    int getLatencySamples() const noexcept
    {
        if (shifterBypassed) return 0;
        return engine == Engine::psola ? psola.getLatencySamples()
                                       : adapterDelay + rbStartDelay;
    }

    int getRubberBandBlockSize() const noexcept { return rbBlockSize; }
    int getRubberBandStartDelay() const noexcept { return rbStartDelay; }

    float getDetectedHz() const noexcept { return lastDetectedHz.load(); }
    float getTargetHz() const noexcept { return lastTargetHz.load(); }
    float getConfidence() const noexcept { return lastConfidence.load(); }
    float getShiftSemitones() const noexcept { return lastShiftSemitones.load(); }
    float getTransitionAmount() const noexcept { return lastTransition.load(); }
    float getVibratoAmount() const noexcept { return lastVibrato.load(); }
    float getProtectionAmount() const noexcept { return lastProtection.load(); }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (!shifter || buffer.getNumSamples() == 0)
            return;

        const int channels = juce::jmin(numChannels, buffer.getNumChannels());

        auto* l = buffer.getWritePointer(0);
        auto* r = channels > 1 ? buffer.getWritePointer(1) : nullptr;

        if (shifterBypassed)
        {
            // The audio is left untouched, but the detector is still fed so the
            // editor keeps showing the sung pitch while tracking.
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                detector.pushSample(0.5f * (l[i] + (r != nullptr ? r[i] : l[i])));

            lastDetectedHz.store(detector.isVoiced() ? detector.getFrequencyHz() : 0.0f);
            lastTargetHz.store(0.0f);
            lastConfidence.store(detector.isVoiced() ? 1.0f : 0.0f);
            lastShiftSemitones.store(0.0f);
            return;
        }

        if (engine == Engine::psola)
        {
            processPsola(buffer, channels, l, r);
            return;
        }

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float inL = l[i];
            const float inR = r != nullptr ? r[i] : inL;
            const float mono = 0.5f * (inL + inR);

            detector.pushSample(mono);

            inputBlock[0][static_cast<size_t>(inFill)] = inL;
            if (channels > 1)
                inputBlock[1][static_cast<size_t>(inFill)] = inR;

            ++inFill;

            if (inFill >= rbBlockSize)
            {
                updatePitchDecision();

                shifter->shift(
                    inputPtrs.data(),
                    outputPtrs.data());

                pushOutputBlock();
                inFill = 0;
            }

            float outL = 0.0f, outR = 0.0f;
            popOutputSample(outL, outR);

            l[i] = outL;
            if (r != nullptr)
                r[i] = outR;
        }
    }

private:
    /*  The PSOLA path, cut into pieces at the decision cadence.

        There is no fixed-block adapter here — grains are laid down as the
        samples arrive — so the only reason to subdivide the host block at all
        is to keep the tuning decision landing at the same rate it does on the
        Rubber Band path. A host handing over 2048 samples at once would
        otherwise make one decision where the other engine makes eight, and
        every retune time would silently stretch to match.
    */
    void processPsola(juce::AudioBuffer<float>& buffer, int channels, float* l, float* r)
    {
        const int count = buffer.getNumSamples();
        int offset = 0;

        while (offset < count)
        {
            const int piece = juce::jmin(count - offset, rbBlockSize - decisionFill);

            for (int i = offset; i < offset + piece; ++i)
                detector.pushSample(0.5f * (l[i] + (r != nullptr ? r[i] : l[i])));

            decisionFill += piece;
            if (decisionFill >= rbBlockSize) {
                updatePitchDecision();
                decisionFill = 0;
            }

            juce::AudioBuffer<float> slice(buffer.getArrayOfWritePointers(),
                                           channels, offset, piece);
            psola.process(slice);
            offset += piece;
        }
    }

    void updatePitchDecision()
    {
        quantizer.setRoot(root);

        PitchQuantizer::ScaleType scale = PitchQuantizer::ScaleType::chromatic;
        switch (scaleIndex)
        {
            case 1: scale = PitchQuantizer::ScaleType::major; break;
            case 2: scale = PitchQuantizer::ScaleType::naturalMinor; break;
            case 3: scale = PitchQuantizer::ScaleType::harmonicMinor; break;
            case 4: scale = PitchQuantizer::ScaleType::dorian; break;
            default: break;
        }
        quantizer.setScale(scale);

        const float rawDetected = detector.getFrequencyHz();
        const float confidence = detector.getConfidence();
        const bool voiced = detector.isVoiced();

        const auto intelligenceResult =
            intelligence.process(rawDetected, confidence);

        const float detected =
            intelligenceResult.correctedHz > 0.0f
                ? intelligenceResult.correctedHz
                : rawDetected;

        lastDetectedHz.store(detected);
        lastConfidence.store(confidence);
        lastTransition.store(intelligenceResult.transition);
        lastVibrato.store(intelligenceResult.vibrato);
        lastProtection.store(intelligenceResult.protection);

        float target = detected;
        float desiredSemitones = 0.0f;
        float timeMs = 30.0f;

        if (enabled && voiced && detected > 0.0f)
        {
            target = quantizer.quantizeHz(detected, confidence);

            if (target > 0.0f)
            {
                const float fullShiftSemitones =
                    12.0f * std::log2(target / detected);

                float modeStrength = 1.0f;
                float baseMs = 35.0f;

                switch (mode)
                {
                    case Mode::natural:
                        modeStrength = 0.72f;
                        baseMs = 95.0f;
                        break;
                    case Mode::modern:
                        modeStrength = 0.92f;
                        baseMs = 28.0f;
                        break;
                    case Mode::hard:
                        modeStrength = 1.0f;
                        baseMs = 2.5f;
                        break;
                }

                // Humanize matters most in Natural mode and around small
                // deviations. Large errors are still corrected.
                const float deviation = std::abs(fullShiftSemitones);
                const float smallNoteProtection =
                    juce::jlimit(0.0f, 1.0f, deviation / 0.55f);

                const float humanizeProtection =
                    1.0f - humanize
                        * (mode == Mode::natural ? 0.55f : 0.25f)
                        * (1.0f - smallNoteProtection);

                const float performanceProtection =
                    1.0f - intelligenceResult.protection
                        * (mode == Mode::hard ? 0.20f
                           : mode == Mode::modern ? 0.45f
                           : 0.72f);

                const float correctedSemitones =
                    fullShiftSemitones
                    * amount
                    * modeStrength
                    * humanizeProtection
                    * performanceProtection;

                desiredSemitones = juce::jlimit(-12.0f, 12.0f, correctedSemitones);

                // Retune 0 -> slower than mode default; 1 -> much faster.
                const float retuneFactor =
                    std::pow(2.0f, (0.5f - retune) * 2.8f);

                const float retuneMs =
                    juce::jlimit(1.0f, 220.0f, baseMs * retuneFactor);

                timeMs = retuneMs;
            }
        }
        else
        {
            if (!voiced) quantizer.reset();
        }

        lastTargetHz.store(target);

        const float actualShift = pitchSmoother.process(desiredSemitones,
            static_cast<double>(rbBlockSize) / sr, timeMs);
        lastShiftSemitones.store(actualShift);
        const double smoothedRatio = std::pow(2.0, actualShift / 12.0);

        if (engine == Engine::psola)
        {
            /*  PSOLA needs the raw reading rather than the corrected one: the
                grains have to be cut where the periods actually are in the
                recording, not where the tuner would like them to be. The
                correction is carried entirely by the spacing they are laid
                down at.
            */
            psola.setPitch(rawDetected, voiced ? confidence : 0.0f);
            psola.setShiftRatio(static_cast<float>(smoothedRatio));
            psola.setFormantRatio(std::pow(2.0f, formantSemitones / 12.0f));
            return;
        }

        shifter->setPitchScale(smoothedRatio);

        if (!enabled || std::abs(formantSemitones) < 0.01f)
        {
            // Automatic preserved formant behaviour.
            shifter->setFormantScale(0.0);
        }
        else
        {
            shifter->setFormantScale(
                std::pow(2.0, static_cast<double>(formantSemitones) / 12.0));
        }
    }

    void pushOutputBlock() noexcept
    {
        for (int i = 0; i < rbBlockSize; ++i)
        {
            if (fifoCount >= fifoCapacity)
            {
                // Defensive overflow policy: discard oldest frame.
                fifoRead = (fifoRead + 1) % fifoCapacity;
                --fifoCount;
            }

            for (int ch = 0; ch < numChannels; ++ch)
                outputFifo[static_cast<size_t>(ch)][static_cast<size_t>(fifoWrite)] =
                    outputBlock[static_cast<size_t>(ch)][static_cast<size_t>(i)];

            fifoWrite = (fifoWrite + 1) % fifoCapacity;
            ++fifoCount;
        }
    }

    void popOutputSample(float& left, float& right) noexcept
    {
        // Until the first fixed block has been processed, output silence.
        // This is the adapter portion of reported latency.
        if (fifoCount <= 0)
        {
            left = right = 0.0f;
            return;
        }

        left = outputFifo[0][static_cast<size_t>(fifoRead)];
        right = numChannels > 1
            ? outputFifo[1][static_cast<size_t>(fifoRead)]
            : left;

        fifoRead = (fifoRead + 1) % fifoCapacity;
        --fifoCount;
    }

    double sr = 48000.0;
    int numChannels = 2;

    bool enabled = true;
    bool shifterBypassed = false;
    int root = 0;
    int scaleIndex = 0;
    Mode mode = Mode::modern;
    float amount = 1.0f;
    float retune = 0.65f;
    float humanize = 0.35f;
    float formantSemitones = 0.0f;

    YinPitchDetector detector;
    PitchQuantizer quantizer;
    PitchIntelligence intelligence;

    std::unique_ptr<RubberBand::RubberBandLiveShifter> shifter;
    voxera::PsolaShifter psola;
    Engine engine = Engine::rubberBand;
    int decisionFill = 0;

    int rbBlockSize = 0;
    int rbStartDelay = 0;
    int adapterDelay = 0;

    std::vector<std::vector<float>> inputBlock;
    std::vector<std::vector<float>> outputBlock;
    std::vector<const float*> inputPtrs;
    std::vector<float*> outputPtrs;
    int inFill = 0;

    std::vector<std::vector<float>> outputFifo;
    int fifoCapacity = 0;
    int fifoRead = 0;
    int fifoWrite = 0;
    int fifoCount = 0;

    PitchSmoother pitchSmoother;

    std::atomic<float> lastDetectedHz { 0.0f };
    std::atomic<float> lastTargetHz { 0.0f };
    std::atomic<float> lastConfidence { 0.0f };
    std::atomic<float> lastShiftSemitones { 0.0f };
    std::atomic<float> lastTransition { 0.0f };
    std::atomic<float> lastVibrato { 0.0f };
    std::atomic<float> lastProtection { 0.0f };
};

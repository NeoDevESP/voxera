#pragma once
#include <JuceHeader.h>
#include <vector>
#include <limits>
#include <cmath>

namespace voxera
{
/*  Pitch shifting by pitch-synchronous overlap-add, in the time domain.

    The phase vocoder upstream works by transforming to the frequency domain,
    moving the partials and transforming back. It is general — it will shift a
    whole mix — and it pays for that generality in latency, in arithmetic, and
    in the smeared quality that comes from reassembling a signal out of
    reassigned phases.

    A single voice does not need any of that. It is one periodic waveform, and
    a period of it already contains everything that makes the voice sound like
    itself: the pitch is how often the period repeats, and the timbre is the
    shape inside it. So instead of touching the spectrum, this cuts the signal
    into single periods and lays them back down closer together or further
    apart. The pitch changes because the repetition rate changed. The shape
    inside each period is untouched, which is why the formants survive without
    anyone having to preserve them — there is no step at which they could have
    been lost.

    What it costs is two periods of delay, and the reason is worth stating
    because it is easy to talk yourself into one. A grain is centred on its
    mark and reaches a period either side, so a grain cannot be laid down until
    a period past its mark has been recorded. And a finished output sample is
    the sum of every grain overlapping it, the last of which is centred a
    period later — so that sample is not finished until a further period has
    gone by. The two waits are consecutive, not concurrent, and the lowest note
    the shifter admits decides how long they are.
*/
class PsolaShifter
{
public:
    void prepare(double sampleRate, int channels)
    {
        sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
        numChannels = juce::jlimit(1, 2, channels);

        maxPeriod = static_cast<int>(std::ceil(sr / lowestHz));
        minPeriod = juce::jmax(4, static_cast<int>(std::floor(sr / highestHz)));
        /*  Half a period, and it has to be half.

            A mark can fall anywhere in the cycle relative to the peaks of the
            waveform, and the furthest it can ever be from one is half a period.
            Searching less than that does not merely find a worse peak — when
            the marks drift out of step with the source, as they do the moment
            the pitch is moved down, there is no peak inside the window at all
            and the cut lands mid-cycle on every grain.
        */
        searchMargin = maxPeriod / 2;

        // Derived from the two waits above, plus the room the peak search needs
        // to look either side of a mark, plus a little slack.
        latency = 2 * maxPeriod + searchMargin + 64;

        ringSize = juce::nextPowerOfTwo(latency + maxPeriod * 2);
        ringMask = ringSize - 1;

        input.assign(static_cast<size_t>(numChannels),
                     std::vector<float>(static_cast<size_t>(ringSize), 0.0f));
        output.assign(static_cast<size_t>(numChannels),
                      std::vector<float>(static_cast<size_t>(ringSize), 0.0f));

        // Hann, sampled per grain. Two Hann windows of this length spaced half
        // their length apart sum to unity, which is the spacing an unshifted
        // grain train uses, so the joins do not modulate the level.
        window.resize(static_cast<size_t>(windowResolution));
        for (int i = 0; i < windowResolution; ++i)
            window[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(
                juce::MathConstants<float>::twoPi * static_cast<float>(i)
                / static_cast<float>(windowResolution - 1));

        reset();
    }

    void reset()
    {
        for (auto& ch : input) std::fill(ch.begin(), ch.end(), 0.0f);
        for (auto& ch : output) std::fill(ch.begin(), ch.end(), 0.0f);
        writePos = 0;
        /*  The first mark sits at the very first sample, and the grain around
            it reaches back before the recording starts — into positions that
            are silent and stay silent, which costs nothing.

            Starting it further in instead, so that its grain fits entirely
            inside recorded audio, looks tidier and quietly drops the opening
            of every take: no grain would ever reach back far enough to read
            it. What made that hard to see is that it does not sound like a
            dropout — the missing stretch is exactly as long as the delay, so
            it hides underneath the latency the plugin already reports.
        */
        nextMark = 0;
        markPos = inputMark = 0.0;
        currentPeriod = static_cast<float>(sr / 200.0);
        voiced = false;
        shift = 1.0f;
    }

    int getLatencySamples() const noexcept { return latency; }

    /*  The detector's reading for this block.

        An unvoiced stretch has no period to cut on. Rather than special-case
        it out of the signal path, the grain train keeps running at unity —
        windows spaced half their length apart, laid down where they were taken
        from, which sums back to the original signal. Consonants and breath
        pass through as themselves, and there is no seam at the moment the
        voicing decision flips.
    */
    void setPitch(float detectedHz, float confidence) noexcept
    {
        const bool nowVoiced = confidence >= confidenceFloor
                            && detectedHz >= lowestHz && detectedHz <= highestHz;
        if (nowVoiced) {
            const float period = static_cast<float>(sr) / detectedHz;
            // Followed rather than jumped to, so a detector octave error lasting
            // one block does not chop a phrase in half.
            currentPeriod = voiced ? currentPeriod + 0.25f * (period - currentPeriod) : period;
        }
        voiced = nowVoiced;
    }

    void setShiftRatio(float ratio) noexcept
    {
        shift = juce::jlimit(0.25f, 4.0f, std::isfinite(ratio) ? ratio : 1.0f);
    }

    /*  Moves the formants without moving the note.

        The pitch is the rate the grains are laid down at, and the timbre is
        the shape inside one — two quantities this method keeps in separate
        hands, which is why the phase vocoder's expensive formant machinery has
        no counterpart here. Playing a grain's contents faster raises
        everything inside it, including the resonances of the throat and mouth
        that decide whether a voice reads as large or small; laying those
        grains down at the spacing they always had leaves the note exactly
        where the singer put it.

        This is the control that makes a voice sound bigger or more forward
        rather than merely louder, and on this engine it costs one
        interpolation per sample.
    */
    void setFormantRatio(float ratio) noexcept
    {
        formant = juce::jlimit(0.5f, 2.0f, std::isfinite(ratio) ? ratio : 1.0f);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());
        const int count = buffer.getNumSamples();
        if (channels <= 0 || count == 0) return;

        for (int i = 0; i < count; ++i)
        {
            for (int ch = 0; ch < channels; ++ch)
                input[static_cast<size_t>(ch)][static_cast<size_t>(writePos & ringMask)] =
                    buffer.getReadPointer(ch)[i];
            ++writePos;

            emitGrains(channels);

            const int readPos = writePos - latency;
            for (int ch = 0; ch < channels; ++ch) {
                auto& slot = output[static_cast<size_t>(ch)][static_cast<size_t>(readPos & ringMask)];
                buffer.getWritePointer(ch)[i] = slot;
                // Emptied on the way past. The slot is next written into a full
                // ring later, so clearing it here cannot erase a pending grain.
                slot = 0.0f;
            }
        }
    }

private:
    /*  Lays down every grain whose input is now fully recorded.

        Output marks are spaced by the target period while the waveform they
        are cut from still repeats at the original one, so raising the pitch
        means marks come round faster than the source provides fresh periods
        and some get used twice, and lowering it means some are passed over.
        Time is not stretched either way — the marks advance with the clock.
        That is the whole mechanism.
    */
    void emitGrains(int channels)
    {
        while (nextMark + maxPeriod + searchMargin <= writePos - 1)
        {
            const int period = juce::jlimit(minPeriod, maxPeriod, static_cast<int>(currentPeriod));

            /*  The spacing is carried as a real number and only rounded when a
                mark is actually placed, with the remainder kept for the next
                one.

                Rounding it per grain instead costs real tuning accuracy, and
                the amount depends on the sample rate in a way nobody would
                predict from the outside: at 48 kHz a 180 Hz note wants marks
                266.67 samples apart, and truncating to 266 sharpens it by four
                cents before the tuner has done anything. The error is small,
                constant, and exactly the kind a singer hears as the plugin
                being slightly out.
            */
            const double spacing = juce::jlimit(
                static_cast<double>(minPeriod), static_cast<double>(maxPeriod),
                voiced ? static_cast<double>(currentPeriod) / shift
                       : static_cast<double>(currentPeriod));
            const int step = juce::jmax(1, static_cast<int>(std::lround(spacing)));

            // Voiced grains snap to a peak so that consecutive cuts land at the
            // same point in the cycle; unvoiced ones must not move at all,
            // since laying noise down away from where it was taken would smear
            // a consonant into a buzz.
            /*  Which cycle of the source this grain is cut from.

                The input mark only ever moves forward, and only ever by whole
                periods, because the cycles of the source are a fixed sequence
                and a grain has to be one of them. It advances until it is the
                cycle nearest this output mark — which for a raised pitch means
                it often does not advance at all and the same cycle is laid down
                twice, and for a lowered one means it advances twice and a cycle
                is passed over.

                Searching around each output mark independently instead, which
                is the obvious shortcut, does not work: nothing then stops the
                mark from picking the cycle before its predecessor, and a grain
                train that goes backwards is not a signal.
            */
            if (voiced) {
                // Re-anchored to the peak it lands on each time rather than to
                // its own prediction, so a period estimate that is slightly off
                // cannot accumulate into a drift away from the actual cycles.
                while (inputMark + currentPeriod <= static_cast<double>(nextMark) + 0.5 * currentPeriod)
                    inputMark = alignToPeak(
                        static_cast<int>(std::lround(inputMark + currentPeriod)), period / 4);
            } else {
                inputMark = nextMark;
            }
            const int centre = static_cast<int>(std::lround(inputMark));

            /*  A grain has to be at least a period long, or it does not carry a
                whole cycle of the waveform and there is nothing to repeat. It
                also has to be at least as long as the gap to the next mark, or
                consecutive grains stop overlapping and the level dips between
                them at the mark rate — which is audible as a rasp on a held
                note and, being periodic, will fool a pitch detector outright.
                Pitching down widens the gaps, so there the spacing is what
                decides the length.
            */
            /*  Raising the formants means reading further out from the mark
                than the grain is written, and the delay budget only stretches
                to one longest period either side. Narrowing the grain is what
                gives there, and it gives where it is harmless: at any ordinary
                sung pitch the grain is a fraction of that budget and nothing is
                narrowed at all — only a note near the very bottom of the range,
                asked for a large upward formant shift at the same time, ever
                reaches the limit.
            */
            const int reach = formant > 1.0f
                ? juce::jmax(minPeriod, static_cast<int>(static_cast<float>(maxPeriod) / formant))
                : maxPeriod;
            const int half = juce::jmin(reach, juce::jmax(period, step));

            /*  Closer spacing means more windows overlap any given sample, so
                the sum would rise with the shift. Scaling by how far the marks
                actually advanced against how wide they are holds the level flat.
            */
            const float gain = static_cast<float>(spacing / half);

            for (int n = -half; n <= half; ++n)
            {
                const float phase = 0.5f * (static_cast<float>(n) / static_cast<float>(half) + 1.0f);
                const int w = juce::jlimit(0, windowResolution - 1,
                    static_cast<int>(phase * static_cast<float>(windowResolution - 1)));
                const float weight = gain * window[static_cast<size_t>(w)];

                // Read at a scaled distance from the mark, written at the true
                // one. That difference is the whole of the formant shift.
                const float source = static_cast<float>(centre) + static_cast<float>(n) * formant;
                const int base = static_cast<int>(std::floor(source));
                const float frac = source - static_cast<float>(base);
                const int lo = base & ringMask;
                const int hi = (base + 1) & ringMask;
                const int to = (nextMark + n) & ringMask;

                for (int ch = 0; ch < channels; ++ch) {
                    const auto& in = input[static_cast<size_t>(ch)];
                    const float sample = in[static_cast<size_t>(lo)]
                        + frac * (in[static_cast<size_t>(hi)] - in[static_cast<size_t>(lo)]);
                    output[static_cast<size_t>(ch)][static_cast<size_t>(to)] += weight * sample;
                }
            }

            markPos += spacing;
            nextMark = static_cast<int>(std::lround(markPos));
        }
    }

    /*  Moves a cut to the nearest waveform peak.

        Cutting on a regular grid would join pieces that were at different
        points in their cycle, and every such join is a step in the waveform —
        heard as a click once per grain, which at a couple of hundred grains a
        second is a buzz sitting on top of the voice. Searching for the largest
        excursion puts every cut at the same place in the cycle instead, and
        the pieces then meet where they already agreed.

        The largest excursion, not the largest magnitude. Taking the magnitude
        looks more natural and is worse: a waveform whose positive and negative
        lobes are close in size — which a voice with strong upper harmonics is
        — will hand the search whichever lobe won by a hair that cycle, so the
        mark alternates between two points half a period apart and every other
        grain is laid down inverted. The result loses its periodicity entirely
        while keeping its level, which is the failure that looks like nothing
        on a meter. Following the signed peak can mean tracking the smaller
        lobe when the take's polarity is flipped, and that costs nothing:
        what the cuts have to be is consistent, not correct.
    */
    int alignToPeak(int around, int radius) const
    {
        const int search = juce::jlimit(2, searchMargin, radius);
        int best = around;
        float largest = -std::numeric_limits<float>::max();

        for (int n = -search; n <= search; ++n) {
            const float value = input[0][static_cast<size_t>((around + n) & ringMask)];
            if (value > largest) { largest = value; best = around + n; }
        }
        return best;
    }

    static constexpr float lowestHz = 65.0f;    // below this a voice is not singing a note
    static constexpr float highestHz = 1200.0f;
    static constexpr float confidenceFloor = 0.55f;
    static constexpr int windowResolution = 2048;

    std::vector<std::vector<float>> input, output;
    std::vector<float> window;

    double sr = 48000.0;
    float currentPeriod = 240.0f, shift = 1.0f, formant = 1.0f;
    int numChannels = 2, ringSize = 0, ringMask = 0, latency = 0;
    int maxPeriod = 0, minPeriod = 0, searchMargin = 0;
    int writePos = 0, nextMark = 0;
    // Both kept as reals: the output spacing because it decides the tuning, and
    // the input mark because it is compared against it.
    double markPos = 0.0, inputMark = 0.0;
    bool voiced = false;
};
}

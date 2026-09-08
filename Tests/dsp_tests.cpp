#include "../Source/DSP/SmartEQ.h"
#include "../Source/DSP/Biquad.h"
#include "../Source/DSP/Limiter.h"
#include "../Source/DSP/Punch.h"
#include "../Source/DSP/Exciter.h"
#include "../Source/DSP/Gate.h"
#include "../Source/DSP/Optical.h"
#include "../Source/DSP/Upward.h"
#include "../Source/DSP/VocalLock.h"
#include "../Source/DSP/Chop.h"
#include "../Source/DSP/Modulation.h"
#include "../Source/DSP/ColourCompressor.h"
#include <algorithm>
#include <memory>
#include "../Source/DSP/SoftClip.h"
#include "../Source/DSP/Character.h"
#include "../Source/DSP/AutoMix.h"
#include "../Source/DSP/YinPitchDetector.h"
#include <vector>
#include "../Source/DSP/AutoGain.h"
#include "../Source/DSP/Saturator.h"
#include "../Source/DSP/PitchEngine.h"
#include "../Source/DSP/VoiceProfileEngine.h"
#include "../Source/DSP/ProfileTransfer.h"
#include "Checks.h"
#include <iostream>
#include "../Source/DSP/SpectralEngine.h"
#include "../Source/DSP/SpatialEngine.h"
#include <thread>
void prepare(AdaptiveSpectralEngine& e) { e.prepare(48000, 128, 2); e.setBodyDb(6); e.setAirDb(6); }
void prepare(SpatialEngine& e) { e.prepare(48000, 2); e.setSpace(0.8f); }
template<class Engine> void checkBypass()
{
    Engine reference, toggled; prepare(reference); prepare(toggled);
    juce::AudioBuffer<float> a(2, 8192), b(2, 8192);
    for (int ch=0;ch<2;++ch) for (int i=0;i<8192;++i) a.setSample(ch,i,0.2f*std::sin(i*0.03f));
    b.makeCopyOf(a); reference.process(a); toggled.process(b);
    a.setSize(2,1); b.setSize(2,1);
    for (int ch=0;ch<2;++ch) { a.setSample(ch,0,0.2f); b.setSample(ch,0,0.2f); }
    toggled.setEnabled(false); reference.process(a); toggled.process(b);
    for(int ch=0;ch<2;++ch)
        CHECK(std::abs(a.getSample(ch,0)-b.getSample(ch,0)) <= std::abs(a.getSample(ch,0)-0.2f)/960.0f + 1.0e-6f);
    b.setSize(2,1200); for(int ch=0;ch<2;++ch) for(int i=0;i<1200;++i) b.setSample(ch,i,0.2f);
    toggled.process(b);
    for(int ch=0;ch<2;++ch) for(int i=960;i<1200;++i) CHECK(b.getSample(ch,i)==0.2f);
    toggled.setEnabled(true); toggled.process(b);
    for(int ch=0;ch<2;++ch) for(int i=0;i<1200;++i) CHECK(std::isfinite(b.getSample(ch,i)));
}
void checkSpectralPartitioning()
{
    constexpr int length=24000;
    AdaptiveSpectralEngine whole, split; prepare(whole); prepare(split);
    juce::AudioBuffer<float> a(2,length), b(2,length);
    for(int ch=0;ch<2;++ch) for(int i=0;i<length;++i)
        a.setSample(ch,i,0.13f*std::sin(i*0.031f)+0.04f*std::sin(i*0.79f));
    b.makeCopyOf(a); whole.process(a);
    const int sizes[]{1,17,64,257,1024}; int j=0;
    for(int pos=0;pos<length;) {
        const int count=std::min(sizes[j++%5],length-pos);
        float* ptr[]{b.getWritePointer(0)+pos,b.getWritePointer(1)+pos};
        juce::AudioBuffer<float> part(ptr,2,count); split.process(part);pos+=count;
    }
    float worst=0;
    for(int ch=0;ch<2;++ch) for(int i=0;i<length;++i) worst=std::max(worst,std::abs(a.getSample(ch,i)-b.getSample(ch,i)));
    CHECK(worst<1.0e-6f);
    std::cout << "Spectral partition max difference: " << worst << "\n";
}
void checkSlowDelay()
{
    DuckingDelay delay; delay.prepare(48000); delay.setTempoAndDivision(40,4); delay.setFeedback(0);
    float l=0,r=0;
    for(int i=0;i<10000;++i) delay.processSample(0,0,l,r);
    int first=-1;
    for(int i=0;i<144020;++i) {
        delay.processSample(i==0?1.0f:0.0f,0,l,r);
        if(first<0 && std::abs(l)>1.0e-5f) first=i;
    }
    std::cout << "Half-note delay at 40 BPM: " << first << " samples\n";
    CHECK(first==144000);
}
void checkSmartEQ()
{
    for (double sr : {44100.0, 48000.0, 96000.0}) {
        const int length = static_cast<int>(sr * 2.0);
        SmartEQ whole, split, inverse, off;
        for (auto* e : {&whole, &split, &inverse, &off}) e->prepare(sr, 2);
        for (auto* e : {&whole, &split, &inverse}) e->setParameters(1, 3, 100);
        juce::AudioBuffer<float> a(2, length), b(2, length), c(2, length), d(2, length);
        for (int n = 0; n < length; ++n) {
            const float x = 0.2f * std::sin(static_cast<float>(n * 1000.0 * juce::MathConstants<double>::twoPi / sr));
            for (int ch = 0; ch < 2; ++ch) {
                a.setSample(ch,n,x); b.setSample(ch,n,x); d.setSample(ch,n,x);
                c.setSample(ch,n,ch == 0 ? x : -x);
            }
        }
        off.process(d);
        for (int n=0; n<length; ++n) CHECK(d.getSample(0,n) == a.getSample(0,n));
        whole.process(a); inverse.process(c);
        const int sizes[] {1, 17, 64, 257, 1024}; int j=0;
        for (int start=0; start<length;) {
            const int count = std::min(sizes[j++%5],length-start);
            float* ptr[] {b.getWritePointer(0)+start,b.getWritePointer(1)+start};
            juce::AudioBuffer<float> part(ptr,2,count); split.process(part); start+=count;
        }
        for(int n=0;n<length;++n) {
            CHECK(a.getSample(0,n) == b.getSample(0,n));
            CHECK(a.getSample(0,n) == a.getSample(1,n));
            CHECK(a.getSample(0,n) == c.getSample(0,n));
            CHECK(std::abs(c.getSample(0,n)+c.getSample(1,n)) < 1e-7f);
            CHECK(std::isfinite(a.getSample(0,n)));
        }
        float sum=0;
        for(size_t i=0;i<5;++i) { CHECK(whole.gainDb(i) <= 0); sum-=whole.gainDb(i); }
        CHECK(sum <= 3.0001f); CHECK(whole.gainDb(2) < -0.5f);
        const float reduction=20.0f*std::log10(a.getRMSLevel(0,length/2,length/2)/d.getRMSLevel(0,length/2,length/2));
        CHECK(reduction < -0.5f && reduction > -3.1f);
        std::cout << "Smart EQ " << sr << " Hz: 1 kHz attenuation " << reduction << " dB, shared budget " << sum << " dB\n";
        whole.setParameters(0,3,100);
        juce::AudioBuffer<float> silence(2, static_cast<int>(sr*5)); silence.clear(); whole.process(silence);
        for(size_t i=0;i<5;++i) CHECK(whole.gainDb(i)==0);
        whole.reset();
        for(size_t i=0;i<5;++i) CHECK(whole.gainDb(i)==0);
    }
    std::cout << "PASS: Smart EQ budget, attenuation, default identity, silence recovery, stereo polarity, block partitioning\n";
}
static float sine(double hz, int i, double sr, float amplitude = 1.0f)
{
    return amplitude * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * hz * i / sr));
}

/*  Amplitude of one exact frequency, by Goertzel over a Hann-windowed buffer.

    Measuring harmonics with a high-pass instead would not be comparable across
    sample rates: bilinear warping makes a biquad at a fixed cutoff far steeper
    near Nyquist at 44.1 kHz than at 96 kHz, so the same threshold would mean
    different things at each rate. Asking for one frequency at a time does not
    have that problem. The window keeps a tone that is not bin-aligned from
    leaking into the neighbouring harmonic.
*/
static float toneMagnitude(const juce::AudioBuffer<float>& buffer, double sr, double hz, int channel = 0)
{
    const int n = buffer.getNumSamples();
    const double w = juce::MathConstants<double>::twoPi * hz / sr;
    const double coeff = 2.0 * std::cos(w);
    double s1 = 0.0, s2 = 0.0, windowSum = 0.0;

    for (int i = 0; i < n; ++i) {
        const double hann = 0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * i / (n - 1));
        windowSum += hann;
        const double s0 = hann * buffer.getSample(channel, i) + coeff * s1 - s2;
        s2 = s1; s1 = s0;
    }

    const double real = s1 - s2 * std::cos(w);
    const double imag = s2 * std::sin(w);
    return static_cast<float>(2.0 * std::sqrt(real * real + imag * imag) / juce::jmax(1.0, windowSum));
}

void checkLimiter()
{
    for (double sr : {44100.0, 48000.0, 96000.0}) {
        constexpr int length = 24000;
        const float ceiling = juce::Decibels::decibelsToGain(-1.0f);

        // A silent lead-in into a loud tone is precisely the case look-ahead
        // exists for: without it the first cycle escapes above the ceiling.
        voxera::Limiter limiter; limiter.prepare(sr, 2);
        limiter.setEnabled(true); limiter.setCeilingDb(-1.0f);
        juce::AudioBuffer<float> loud(2, length);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                loud.setSample(ch, i, i < length / 3 ? 0.0f : sine(300.0, i, sr, 4.0f));
        limiter.process(loud);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i) {
                CHECK(std::isfinite(loud.getSample(ch, i)));
                CHECK(std::abs(loud.getSample(ch, i)) <= ceiling + 1.0e-6f);
            }

        // Disabled, the stage must be a pure delay of the reported latency.
        voxera::Limiter through; through.prepare(sr, 2); through.setEnabled(false);
        const int latency = through.getLatencySamples();
        juce::AudioBuffer<float> source(2, length), delayed(2, length);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i) source.setSample(ch, i, sine(220.0, i, sr, 0.3f));
        delayed.makeCopyOf(source); through.process(delayed);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i + latency < length; ++i)
                CHECK(delayed.getSample(ch, i + latency) == source.getSample(ch, i));

        // Chunked and whole-buffer processing must agree exactly.
        voxera::Limiter whole, split; whole.prepare(sr, 2); split.prepare(sr, 2);
        for (auto* l : {&whole, &split}) { l->setEnabled(true); l->setCeilingDb(-1.0f); }
        juce::AudioBuffer<float> a(2, length), b(2, length);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                a.setSample(ch, i, sine(180.0, i, sr, 1.8f) + sine(2700.0, i, sr, 0.5f));
        b.makeCopyOf(a); whole.process(a);
        const int sizes[]{1, 17, 64, 257, 1024}; int j = 0;
        for (int pos = 0; pos < length;) {
            const int count = std::min(sizes[j++ % 5], length - pos);
            float* ptr[]{b.getWritePointer(0) + pos, b.getWritePointer(1) + pos};
            juce::AudioBuffer<float> part(ptr, 2, count); split.process(part); pos += count;
        }
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                CHECK(a.getSample(ch, i) == b.getSample(ch, i));
    }
    std::cout << "PASS: limiter ceiling held, pure delay when off, block-size equivalence\n";
}

void checkExciter()
{
    for (double sr : {44100.0, 48000.0, 96000.0}) {
        constexpr int length = 16384;
        constexpr double fundamental = 4000.0;
        juce::AudioBuffer<float> dry(2, length);
        /*  A voice, not a bare tone: a low fundamental with some content in the
            excited band on top. The band alone would read as a sibilant to the
            guard below and be held back, which is correct behaviour and useless
            for measuring what the squaring produces.

            4 kHz sits inside the band, so squaring must put its whole output at
            8 kHz. Anything at 12 kHz would be a third harmonic, which a
            second-order stage cannot make — that is what separates this from a
            saturator, whose third harmonic would also alias at 44.1 kHz. The
            180 Hz part is below the split filter and never reaches the squarer.
        */
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                dry.setSample(ch, i, sine(180.0, i, sr, 0.30f) + sine(fundamental, i, sr, 0.25f));

        voxera::Exciter off; off.prepare(sr, 2); off.setAmount(0.0f);
        juce::AudioBuffer<float> silentPath(2, length); silentPath.makeCopyOf(dry);
        off.process(silentPath);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                CHECK(silentPath.getSample(ch, i) == dry.getSample(ch, i));

        voxera::Exciter on; on.prepare(sr, 2); on.setAmount(1.0f);
        juce::AudioBuffer<float> wet(2, length); wet.makeCopyOf(dry);
        on.process(wet);

        juce::AudioBuffer<float> generated(2, length);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i) {
                CHECK(std::isfinite(wet.getSample(ch, i)));
                generated.setSample(ch, i, wet.getSample(ch, i) - dry.getSample(ch, i));
            }

        const float second = toneMagnitude(generated, sr, 2.0 * fundamental);
        const float third = toneMagnitude(generated, sr, 3.0 * fundamental);
        CHECK(second > 1.0e-3f);
        CHECK(third < second * 0.02f);

        /*  And it has to hold back on a sibilant.

            The band this stage squares is the band an "s" occupies, and it runs
            after the de-esser, so anything it adds there is never taken back.
            A vowel puts most of its energy below the band; a sibilant puts most
            of it inside. Feeding one of each and comparing what comes out is
            what shows the guard is working.
        */
        /*  Both signals carry the same amount of band content, and differ only
            in what surrounds it. That is what isolates the guard: comparing a
            vowel against a sibilant outright would mostly measure how much
            band energy each happens to have, which is not the question.
        */
        const auto excite = [&](bool sibilant) {
            voxera::Exciter e; e.prepare(sr, 2); e.setAmount(1.0f);
            juce::AudioBuffer<float> b(2, length), reference(2, length);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < length; ++i) {
                    const float inBand = sine(6000.0, i, sr, 0.20f);
                    // Identical in the band; the vowel simply has a fundamental
                    // underneath it and the sibilant has nothing.
                    const float v = sibilant ? inBand : sine(180.0, i, sr, 0.35f) + inBand;
                    b.setSample(ch, i, v);
                    reference.setSample(ch, i, v);
                }
            e.process(b);
            double sum = 0.0;
            for (int i = length / 2; i < length; ++i) {
                const double d = b.getSample(0, i) - reference.getSample(0, i);
                sum += d * d;
            }
            return std::sqrt(sum / (length / 2));
        };

        const double onVowel = excite(false);
        const double onSibilant = excite(true);
        std::cout << "Exciter " << sr << "Hz: same band content adds " << onVowel
                  << " under a vowel, " << onSibilant << " on its own\n";
        CHECK(onSibilant < onVowel * 0.5);
        std::cout << "Exciter " << sr << "Hz: 2nd harmonic " << second
                  << ", 3rd " << third << " (" << (100.0f * third / second) << "%)\n";
    }
    std::cout << "PASS: exciter identity at zero, generates air, stays band-limited\n";
}

void checkPunch()
{
    const juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    constexpr int length = 4096;
    juce::AudioBuffer<float> dry(2, length);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i) dry.setSample(ch, i, sine(200.0, i, 48000.0, 0.15f));

    voxera::Punch off; off.prepare(spec); off.setAmount(0.0f);
    juce::AudioBuffer<float> untouched(2, length); untouched.makeCopyOf(dry);
    off.process(untouched);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i)
            CHECK(untouched.getSample(ch, i) == dry.getSample(ch, i));

    voxera::Punch on; on.prepare(spec); on.setAmount(1.0f);
    juce::AudioBuffer<float> blended(2, length); blended.makeCopyOf(dry);
    on.process(blended);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i) CHECK(std::isfinite(blended.getSample(ch, i)));
    CHECK(blended.getMagnitude(0, length) > dry.getMagnitude(0, length));
    std::cout << "PASS: punch identity at zero, adds level in parallel\n";
}

void checkGate()
{
    for (double sr : {44100.0, 48000.0, 96000.0}) {
        const int length = static_cast<int>(sr);   // one second
        voxera::Gate gate; gate.prepare(sr, 2);
        gate.setEnabled(true); gate.setThresholdDb(-55.0f);

        // A note followed by a quiet trailing consonant, then real silence. The
        // consonant must survive; only the silence may be removed.
        juce::AudioBuffer<float> b(2, length);
        const int noteEnd = length / 2;
        const int consonantEnd = noteEnd + static_cast<int>(sr * 0.06);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i) {
                const float level = i < noteEnd ? 0.3f : (i < consonantEnd ? 0.006f : 0.0f);
                b.setSample(ch, i, sine(400.0, i, sr, level));
            }
        gate.process(b);

        const int latency = gate.getLatencySamples();
        float consonant = 0.0f, tail = 0.0f;
        for (int i = noteEnd + latency; i < consonantEnd; ++i)
            consonant = juce::jmax(consonant, std::abs(b.getSample(0, i)));
        for (int i = length - static_cast<int>(sr * 0.05); i < length; ++i)
            tail = juce::jmax(tail, std::abs(b.getSample(0, i)));

        CHECK(consonant > 0.003f);            // hold kept the gate open for it
        CHECK(tail < 0.0005f);                // and the dead air is gone
        std::cout << "Gate " << sr << "Hz: consonant kept at " << consonant
                  << ", silence floor " << tail << '\n';

        // Disabled, it must be a pure delay of the reported latency.
        voxera::Gate through; through.prepare(sr, 2); through.setEnabled(false);
        juce::AudioBuffer<float> source(2, 4096), delayed(2, 4096);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4096; ++i) source.setSample(ch, i, sine(220.0, i, sr, 0.3f));
        delayed.makeCopyOf(source); through.process(delayed);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i + latency < 4096; ++i)
                CHECK(delayed.getSample(ch, i + latency) == source.getSample(ch, i));
    }
    std::cout << "PASS: gate keeps trailing consonants, removes silence, pure delay when off\n";
}

void checkSoftClip()
{
    constexpr int length = 8192;
    constexpr double sr = 48000.0;
    juce::AudioBuffer<float> dry(2, length);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i) dry.setSample(ch, i, sine(500.0, i, sr, 0.9f));

    voxera::SoftClip off; off.prepare(sr, 2); off.setAmount(0.0f);
    juce::AudioBuffer<float> untouched(2, length); untouched.makeCopyOf(dry);
    off.process(untouched);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i)
            CHECK(untouched.getSample(ch, i) == dry.getSample(ch, i));

    voxera::SoftClip on; on.prepare(sr, 2); on.setAmount(1.0f);
    juce::AudioBuffer<float> clipped(2, length); clipped.makeCopyOf(dry);
    on.process(clipped);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i) CHECK(std::isfinite(clipped.getSample(ch, i)));

    /*  What a clipper actually guarantees is a smaller crest factor: the body of
        the signal comes up while the peaks are rounded off, so the ratio between
        them shrinks. It does not guarantee the peak itself falls — normalising
        the curve so full scale maps to full scale means a signal below full
        scale is pushed up towards it, which is the whole point. What must hold
        is that it never goes past full scale.
    */
    double drySumSq = 0.0, wetSumSq = 0.0;
    for (int i = 0; i < length; ++i) {
        drySumSq += static_cast<double>(dry.getSample(0, i)) * dry.getSample(0, i);
        wetSumSq += static_cast<double>(clipped.getSample(0, i)) * clipped.getSample(0, i);
    }
    const double dryRms = std::sqrt(drySumSq / length);
    const double wetRms = std::sqrt(wetSumSq / length);
    const double dryPeak = dry.getMagnitude(0, length);
    const double wetPeak = clipped.getMagnitude(0, length);

    CHECK(wetRms > dryRms * 1.05);
    CHECK(wetPeak <= 1.0 + 1.0e-4);
    CHECK(wetPeak / wetRms < dryPeak / dryRms);

    // The curve is odd, so it may only produce odd harmonics. A second harmonic
    // would mean an asymmetry the shape does not have.
    const float second = toneMagnitude(clipped, sr, 1000.0);
    const float third = toneMagnitude(clipped, sr, 1500.0);
    CHECK(third > 1.0e-3f);
    CHECK(second < third * 0.02f);
    std::cout << "SoftClip: crest " << (dryPeak / dryRms) << " -> " << (wetPeak / wetRms)
              << ", 3rd harmonic " << third << ", 2nd " << second << '\n';
    std::cout << "PASS: soft clip identity at zero, lowers crest, stays under full scale, odd-only\n";
}

void checkOptical()
{
    constexpr double sr = 48000.0;
    constexpr int length = 48000;
    juce::AudioBuffer<float> dry(2, length);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i) dry.setSample(ch, i, sine(300.0, i, sr, 0.5f));

    voxera::Optical off; off.prepare(sr, 2); off.setAmount(0.0f);
    juce::AudioBuffer<float> light(2, length); light.makeCopyOf(dry);
    off.process(light);

    voxera::Optical on; on.prepare(sr, 2); on.setAmount(1.0f);
    juce::AudioBuffer<float> heavy(2, length); heavy.makeCopyOf(dry);
    on.process(heavy);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i) {
            CHECK(std::isfinite(light.getSample(ch, i)));
            CHECK(std::isfinite(heavy.getSample(ch, i)));
        }
    CHECK(on.getReductionDb() > off.getReductionDb());
    CHECK(on.getReductionDb() > 1.0f);
    std::cout << "Optical: reduction " << off.getReductionDb() << " dB at zero, "
              << on.getReductionDb() << " dB at full\n";
    std::cout << "PASS: optical stage engages with the control and stays finite\n";
}

void checkCharacter()
{
    constexpr double sr = 48000.0;
    constexpr int length = 8192;
    juce::AudioBuffer<float> dry(2, length);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i)
            dry.setSample(ch, i, sine(150.0, i, sr, 0.2f) + sine(6000.0, i, sr, 0.2f));

    // Neutral must be bit-exact, not merely close: it is the default, and every
    // session that never touches this control passes through it.
    voxera::Character plain; plain.prepare(sr, 2); plain.setType(voxera::Character::neutral);
    juce::AudioBuffer<float> untouched(2, length); untouched.makeCopyOf(dry);
    plain.process(untouched);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i)
            CHECK(untouched.getSample(ch, i) == dry.getSample(ch, i));
    CHECK(plain.formantOffsetSemitones() == 0.0f);

    const auto colour = [&](int type) {
        voxera::Character c; c.prepare(sr, 2); c.setType(type);
        juce::AudioBuffer<float> b(2, length); b.makeCopyOf(dry);
        // Two passes: the first lets the smoothed gains reach their target.
        c.process(b); b.makeCopyOf(dry); c.process(b);
        return std::make_pair(toneMagnitude(b, sr, 150.0), toneMagnitude(b, sr, 6000.0));
    };

    const auto neutral = colour(voxera::Character::neutral);
    const auto bright = colour(voxera::Character::bright);
    const auto dark = colour(voxera::Character::dark);
    const auto demon = colour(voxera::Character::demon);
    const auto robot = colour(voxera::Character::robot);

    CHECK(bright.second > neutral.second);   // top lifted
    CHECK(bright.first < neutral.first);     // weight removed
    CHECK(dark.second < neutral.second);
    CHECK(dark.first > neutral.first);
    CHECK(demon.first > neutral.first);      // heavier still
    CHECK(robot.first < dark.first);         // narrow band, no low end
    CHECK(robot.second < neutral.second);

    // Every character must move formants, and stay inside what the shifter takes.
    for (int type = 0; type < voxera::Character::numTypes; ++type) {
        voxera::Character c; c.prepare(sr, 2); c.setType(type);
        juce::AudioBuffer<float> b(2, length); b.makeCopyOf(dry);
        for (int pass = 0; pass < 40; ++pass) c.process(b);
        CHECK(std::abs(c.formantOffsetSemitones()) <= 12.0f);
    }
    std::cout << "PASS: voice characters tilt as named, neutral is exact, formants in range\n";
}

void checkUpward()
{
    constexpr double sr = 48000.0;
    const int length = static_cast<int>(sr);   // one second, well past the release

    // Level is what this stage keys on, so each case is one steady amplitude.
    const auto gainAt = [&](float amplitude, float amount) {
        voxera::Upward up; up.prepare(sr, 2); up.setAmount(amount);
        juce::AudioBuffer<float> b(2, length);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i) b.setSample(ch, i, sine(300.0, i, sr, amplitude));
        up.process(b);
        // Measure at the end, once the slow envelope has settled.
        float peak = 0.0f;
        for (int i = length - 4096; i < length; ++i) peak = juce::jmax(peak, std::abs(b.getSample(0, i)));
        return juce::Decibels::gainToDecibels(peak / amplitude);
    };

    // 0.0006 is about -64 dBFS: below the floor, so it is noise as far as this
    // stage is concerned. 0.02 is -34 dBFS, quiet detail worth raising. 0.5 is
    // -6 dBFS, already loud and none of its business.
    const float noise = gainAt(0.0006f, 1.0f);
    const float quiet = gainAt(0.02f, 1.0f);
    const float loud  = gainAt(0.5f, 1.0f);
    const float off   = gainAt(0.02f, 0.0f);

    CHECK(std::abs(off) < 0.01f);        // identity at zero
    CHECK(quiet > 3.0f);                 // quiet detail is genuinely raised
    CHECK(std::abs(loud) < 0.5f);        // loud material is left alone
    CHECK(noise < quiet * 0.5f);         // and the floor is not lifted with it
    CHECK(quiet <= 12.5f);               // never beyond the declared cap

    std::cout << "Upward: noise " << noise << " dB, quiet +" << quiet
              << " dB, loud " << loud << " dB\n";
    std::cout << "PASS: upward compression raises quiet detail, spares loud and noise\n";
}

void checkChopAndCrush()
{
    constexpr double sr = 48000.0;
    const int length = static_cast<int>(sr * 2.0);

    // --- Chop -----------------------------------------------------------
    const auto chopped = [&](float amount) {
        voxera::Chop c; c.prepare(sr, 2);
        c.setTempo(120.0); c.setDivision(1); c.setPattern(0);   // 1/16, alternate
        c.setAmount(amount); c.setPosition(0.0);
        juce::AudioBuffer<float> b(2, length);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i) b.setSample(ch, i, sine(440.0, i, sr, 0.4f));
        c.process(b);
        return b;
    };

    const auto quiet = chopped(0.0f);
    const auto cut = chopped(1.0f);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i) CHECK(std::isfinite(cut.getSample(ch, i)));

    // At zero the gate must never close, so nothing is touched at all.
    for (int i = 0; i < length; ++i) CHECK(std::abs(quiet.getSample(0, i)) > 0.0f || i < 4);

    /*  A 1/16 step at 120 BPM is 125 ms, and the alternate pattern opens every
        other one. So over two seconds the output has to contain both stretches
        at full level and stretches near silence; measuring the loudest and the
        quietest 20 ms window is enough to show the gate is working on the grid
        rather than just attenuating everything.
    */
    const int window = static_cast<int>(sr * 0.02);
    float loudest = 0.0f, quietest = 1.0f;
    for (int start = window; start + window < length; start += window) {
        float peak = 0.0f;
        for (int i = start; i < start + window; ++i) peak = juce::jmax(peak, std::abs(cut.getSample(0, i)));
        loudest = juce::jmax(loudest, peak);
        quietest = juce::jmin(quietest, peak);
    }
    CHECK(loudest > 0.3f);
    CHECK(quietest < 0.05f);
    std::cout << "Chop: loudest window " << loudest << ", quietest " << quietest << '\n';

    /*  The processor splits a host buffer into chunks and reads the playhead
        once per chunk, so the same position arrives several times for one
        block. Applying it each time would rewind the phase advanced during the
        previous chunk and replay the same window. Feeding four chunks with one
        position has to give the same result as one call of the same total
        length, which is what this asserts.
    */
    {
        const int span = 8192;
        voxera::Chop whole, split;
        for (auto* c : { &whole, &split }) {
            c->prepare(sr, 2); c->setTempo(120.0); c->setDivision(1);
            c->setPattern(0); c->setAmount(1.0f);
        }
        juce::AudioBuffer<float> a(2, span), b(2, span);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < span; ++i) { a.setSample(ch, i, 0.4f); b.setSample(ch, i, 0.4f); }

        whole.setPosition(4.0);
        whole.process(a);

        for (int part = 0; part < 4; ++part) {
            split.setPosition(4.0);   // the same position, as the host reports it
            float* ptr[]{ b.getWritePointer(0) + part * 2048, b.getWritePointer(1) + part * 2048 };
            juce::AudioBuffer<float> chunk(ptr, 2, 2048);
            split.process(chunk);
        }
        for (int i = 0; i < span; ++i)
            CHECK(std::abs(a.getSample(0, i) - b.getSample(0, i)) < 1.0e-6f);
    }

    // --- Crush ----------------------------------------------------------
    voxera::Crush off; off.prepare(sr, 2); off.setAmount(0.0f); off.setMix(0.0f);
    juce::AudioBuffer<float> dry(2, 4096), same(2, 4096);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 4096; ++i) dry.setSample(ch, i, sine(300.0, i, sr, 0.4f));
    same.makeCopyOf(dry); off.process(same);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 4096; ++i) CHECK(same.getSample(ch, i) == dry.getSample(ch, i));

    voxera::Crush on; on.prepare(sr, 2); on.setAmount(1.0f); on.setMix(1.0f);
    juce::AudioBuffer<float> wrecked(2, 4096); wrecked.makeCopyOf(dry);
    on.process(wrecked);
    // Quantising and holding must produce a staircase: distinct values, and far
    // fewer of them than the input had.
    std::vector<float> values;
    for (int i = 2048; i < 4096; ++i) values.push_back(wrecked.getSample(0, i));
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    for (int i = 0; i < 4096; ++i) CHECK(std::isfinite(wrecked.getSample(0, i)));
    CHECK(values.size() < 200);
    std::cout << "Crush: " << values.size() << " distinct levels in 2048 samples\n";
    std::cout << "PASS: chop gates on the grid and is exact at zero; crush quantises\n";
}

void checkModulationAndGlue()
{
    constexpr double sr = 48000.0;
    constexpr int length = 16384;

    juce::AudioBuffer<float> dry(2, length);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i)
            dry.setSample(ch, i, sine(300.0, i, sr, 0.3f) + sine(1100.0, i, sr, 0.2f));

    for (int type : { voxera::Modulation::flanger, voxera::Modulation::phaser }) {
        voxera::Modulation off; off.prepare(sr, 2); off.setType(voxera::Modulation::off);
        juce::AudioBuffer<float> untouched(2, length); untouched.makeCopyOf(dry);
        off.process(untouched);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                CHECK(untouched.getSample(ch, i) == dry.getSample(ch, i));

        voxera::Modulation m; m.prepare(sr, 2);
        m.setType(type); m.setRateHz(2.0f); m.setDepth(1.0f); m.setMix(1.0f);
        juce::AudioBuffer<float> wet(2, length); wet.makeCopyOf(dry);
        m.process(wet);

        double difference = 0.0, peak = 0.0;
        for (int i = 0; i < length; ++i) {
            difference += std::abs(wet.getSample(0, i) - dry.getSample(0, i));
            peak = juce::jmax(peak, static_cast<double>(std::abs(wet.getSample(0, i))));
            CHECK(std::isfinite(wet.getSample(0, i)));
            CHECK(std::isfinite(wet.getSample(1, i)));
        }
        CHECK(difference > 1.0);
        // Feedback below unity: it must resonate, not run away.
        CHECK(peak < 8.0);
        std::cout << "Modulation type " << type << ": peak " << peak << '\n';
    }

    // --- Glue -----------------------------------------------------------
    voxera::Glue none; none.prepare(sr, 2); none.setAmount(0.0f);
    juce::AudioBuffer<float> plain(2, length); plain.makeCopyOf(dry);
    none.process(plain);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i) CHECK(plain.getSample(ch, i) == dry.getSample(ch, i));

    voxera::Glue glued; glued.prepare(sr, 2); glued.setAmount(1.0f);
    juce::AudioBuffer<float> together(2, length); together.makeCopyOf(dry);
    glued.process(together);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i) CHECK(std::isfinite(together.getSample(ch, i)));

    /*  Glue has to stay gentle. If the last block differs from the input by more
        than a few dB it has stopped gluing and started compressing, which is the
        one thing this stage must not do.
    */
    double ratio = 0.0; int counted = 0;
    for (int i = length - 4096; i < length; ++i) {
        const float in = std::abs(dry.getSample(0, i));
        if (in > 0.05f) { ratio += std::abs(together.getSample(0, i)) / in; ++counted; }
    }
    ratio /= juce::jmax(1, counted);
    const double db = juce::Decibels::gainToDecibels(ratio);
    CHECK(std::abs(db) < 4.0);
    std::cout << "Glue: net " << db << " dB\n";
    std::cout << "PASS: modulation is exact when off and stable when on; glue stays gentle\n";
}

void checkDoubler()
{
    constexpr double sr = 48000.0;
    const int length = static_cast<int>(sr * 3.0);   // long enough for the slowest LFO

    const auto run = [&](float amount) {
        SpatialEngine s; s.prepare(sr, 2);
        s.setDouble(amount); s.setDelay(0.0f); s.setSpace(0.0f);
        s.setWidth(0.5f); s.setDuck(0.0f);
        juce::AudioBuffer<float> b(2, length);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                b.setSample(ch, i, sine(220.0, i, sr, 0.25f) + sine(660.0, i, sr, 0.12f));
        s.process(b);
        return b;
    };

    const auto dry = run(0.0f);
    const auto wide = run(1.0f);

    // Side energy is what "wide" means: identical channels have none.
    const auto sideRms = [](const juce::AudioBuffer<float>& b) {
        double sum = 0.0;
        for (int i = 0; i < b.getNumSamples(); ++i) {
            const double side = 0.5 * (b.getSample(0, i) - b.getSample(1, i));
            sum += side * side;
        }
        return std::sqrt(sum / b.getNumSamples());
    };
    const auto monoRms = [](const juce::AudioBuffer<float>& b) {
        double sum = 0.0;
        for (int i = 0; i < b.getNumSamples(); ++i) {
            const double mid = 0.5 * (b.getSample(0, i) + b.getSample(1, i));
            sum += mid * mid;
        }
        return std::sqrt(sum / b.getNumSamples());
    };

    const double drySide = sideRms(dry), wideSide = sideRms(wide);
    const double dryMono = monoRms(dry), wideMono = monoRms(wide);

    std::cout << "Doubler: side " << drySide << " -> " << wideSide
              << ", mono sum " << dryMono << " -> " << wideMono << '\n';

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i) CHECK(std::isfinite(wide.getSample(ch, i)));

    CHECK(wideSide > drySide * 4.0);   // it genuinely widens

    /*  The failure mode a doubler has to be checked for is mono collapse. Every
        voice is a delayed copy of the same signal, so summing to mono combs
        them against the original, and a badly chosen set of delays can cancel
        most of the level. Anyone listening on a phone speaker hears that and
        nothing else.
    */
    CHECK(wideMono > dryMono * 0.7);
    std::cout << "PASS: doubler widens without collapsing when summed to mono\n";
}

void checkWarmth()
{
    constexpr double sr = 48000.0;
    constexpr int length = 16384;
    constexpr double f0 = 500.0;

    const auto harmonics = [&](float warmth) {
        juce::dsp::ProcessSpec spec { sr, 512, 2 };
        Saturator sat; sat.prepare(spec);
        sat.setDriveDb(18.0f); sat.setMix(1.0f); sat.setWarmth(warmth);
        juce::AudioBuffer<float> b(2, length);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i) b.setSample(ch, i, sine(f0, i, sr, 0.5f));
        // Two passes so the smoothed drive and warmth have reached target.
        sat.process(b);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i) b.setSample(ch, i, sine(f0, i, sr, 0.5f));
        sat.process(b);
        return std::make_pair(toneMagnitude(b, sr, 2.0 * f0),   // even
                              toneMagnitude(b, sr, 3.0 * f0));  // odd
    };

    const auto plain = harmonics(0.0f);
    const auto warm = harmonics(1.0f);

    std::cout << "Saturator symmetric: 2nd " << plain.first << ", 3rd " << plain.second << '\n';
    std::cout << "Saturator warm:      2nd " << warm.first << ", 3rd " << warm.second << '\n';

    /*  tanh is odd, so with no bias the second harmonic must be absent — that
        is the limitation the warmth control exists to lift. With bias it has to
        appear, and substantially, while the odd harmonics carry on as before.
    */
    CHECK(plain.second > 1.0e-3f);              // the stage is distorting at all
    CHECK(plain.first < plain.second * 0.01f);  // and symmetrically
    CHECK(warm.first > plain.first * 20.0f);    // bias produced even harmonics
    CHECK(warm.first > warm.second * 0.1f);     // at a level that matters

    /*  Where the colour lands, which is what separates gear from distortion.

        A curve applied flat across the spectrum shapes a low fundamental and a
        sibilant one equally hard, and the results are mud and harshness. Real
        circuits colour the bottom far more than the top, so the stage tilts the
        signal into the curve and untilts it afterwards. Measuring a low tone
        and a high one through the same settings is what shows that is
        happening: the low one has to come out with substantially more harmonic
        content than the high one, or the tilt is not doing its job.
    */
    const auto distortionAt = [&](double hz) {
        juce::dsp::ProcessSpec spec { sr, 512, 2 };
        Saturator sat; sat.prepare(spec);
        sat.setDriveDb(18.0f); sat.setMix(1.0f); sat.setWarmth(0.6f);
        juce::AudioBuffer<float> b(2, length);
        for (int pass = 0; pass < 2; ++pass) {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < length; ++i) b.setSample(ch, i, sine(hz, i, sr, 0.5f));
            sat.process(b);
        }
        const float fundamental = toneMagnitude(b, sr, hz);
        const float second = toneMagnitude(b, sr, 2.0 * hz);
        const float third = toneMagnitude(b, sr, 3.0 * hz);
        return (second + third) / juce::jmax(1.0e-9f, fundamental);
    };

    const float lowColour = distortionAt(150.0);
    const float highColour = distortionAt(5000.0);
    std::cout << "Saturator colour: " << (100.0f * lowColour) << "% at 150 Hz, "
              << (100.0f * highColour) << "% at 5 kHz\n";
    CHECK(lowColour > highColour * 2.0f);

    std::cout << "PASS: warmth adds even harmonics, and the colour lands low rather than flat\n";
}

void checkVocalLock()
{
    constexpr double sr = 48000.0;

    // Settle the tracker on a singer whose voice sits around `hz`, singing a
    // melody of +/- 5 semitones around it.
    const auto settleOn = [&](float hz) {
        auto lock = std::make_unique<voxera::VocalLock>();
        lock->prepare(sr, 2);
        lock->setAmount(1.0f);
        for (int block = 0; block < 4000; ++block) {
            const float semitone = 5.0f * std::sin(block * 0.03f);
            lock->observePitch(hz * std::pow(2.0f, semitone / 12.0f), 0.9f);
        }
        return lock;
    };

    // A bass around 100 Hz and a soprano around 400 Hz.
    const auto bass = settleOn(100.0f);
    const auto soprano = settleOn(400.0f);

    std::cout << "VocalLock bass    (100 Hz): HPF " << bass->highPassHz()
              << " Hz, mud cut " << bass->mudHz() << " Hz\n";
    std::cout << "VocalLock soprano (400 Hz): HPF " << soprano->highPassHz()
              << " Hz, mud cut " << soprano->mudHz() << " Hz\n";

    // The whole point: both filters must land somewhere different for the two
    // voices. A fixed 250 Hz mud cut would be sitting on the soprano's
    // fundamental, and a fixed 80 Hz high-pass leaves her rumble untouched.
    CHECK(soprano->highPassHz() > bass->highPassHz() * 1.5f);
    CHECK(soprano->mudHz() > bass->mudHz() * 1.5f);
    // The mud cut has to stay above the fundamental it is protecting.
    CHECK(bass->mudHz() > 100.0f);
    CHECK(soprano->mudHz() > 400.0f);
    // And both stay inside the declared bounds.
    for (const auto* l : { bass.get(), soprano.get() }) {
        CHECK(l->highPassHz() >= 45.0f && l->highPassHz() <= 220.0f);
        CHECK(l->mudHz() >= 140.0f && l->mudHz() <= 700.0f);
    }

    // Unvoiced material must not drag the estimate down: consonants and breaths
    // report no fundamental, and averaging those in would sink both filters.
    const auto before = soprano->mudHz();
    for (int i = 0; i < 4000; ++i) soprano->observePitch(0.0f, 0.0f);
    for (int i = 0; i < 400; ++i) soprano->observePitch(30.0f, 0.95f);   // below range
    CHECK(std::abs(soprano->mudHz() - before) < 1.0f);

    // Identity at zero, whatever the tracker has decided.
    constexpr int length = 4096;
    juce::AudioBuffer<float> dry(2, length), through(2, length);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i) dry.setSample(ch, i, sine(220.0, i, sr, 0.3f));
    auto quiet = settleOn(220.0f);
    quiet->setAmount(0.0f);
    through.makeCopyOf(dry);
    quiet->process(through); through.makeCopyOf(dry); quiet->process(through);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i)
            CHECK(through.getSample(ch, i) == dry.getSample(ch, i));

    std::cout << "PASS: vocal lock follows the singer's register, ignores unvoiced, exact at zero\n";
}

void checkColourCompressor()
{
    constexpr double sr = 48000.0;
    const int length = static_cast<int>(sr * 2.0);

    struct Result { float reduction, harmonics, settle; };

    const auto measure = [&](int type) {
        voxera::ColourCompressor c;
        c.prepare(sr, 2);
        c.setType(type);
        c.setThresholdDb(-24.0f); c.setRatio(6.0f);
        c.setAttackMs(8.0f); c.setReleaseMs(120.0f);

        juce::AudioBuffer<float> b(2, length);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i) b.setSample(ch, i, sine(200.0, i, sr, 0.5f));
        c.process(b);

        const float fundamental = toneMagnitude(b, sr, 200.0);
        const float second = toneMagnitude(b, sr, 400.0);
        const float third = toneMagnitude(b, sr, 600.0);

        // How far the gain has settled from where it started tells whether the
        // release is programme dependent or a plain exponential.
        float early = 0.0f, late = 0.0f;
        for (int i = 4000; i < 5000; ++i) early = juce::jmax(early, std::abs(b.getSample(0, i)));
        for (int i = length - 1000; i < length; ++i) late = juce::jmax(late, std::abs(b.getSample(0, i)));

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i) CHECK(std::isfinite(b.getSample(ch, i)));

        return Result { c.getReductionDb(),
                        (second + third) / juce::jmax(1.0e-9f, fundamental),
                        late / juce::jmax(1.0e-9f, early) };
    };

    const auto pure = measure(voxera::ColourCompressor::clean);
    const auto fet = measure(voxera::ColourCompressor::fet);
    const auto vca = measure(voxera::ColourCompressor::vca);
    const auto tube = measure(voxera::ColourCompressor::variMu);

    std::cout << "Compressor Clean:   " << pure.reduction << " dB GR, harmonics "
              << (100.0f * pure.harmonics) << "%\n";
    std::cout << "Compressor FET:     " << fet.reduction << " dB GR, harmonics "
              << (100.0f * fet.harmonics) << "%\n";
    std::cout << "Compressor VCA:     " << vca.reduction << " dB GR, harmonics "
              << (100.0f * vca.harmonics) << "%\n";
    std::cout << "Compressor Vari-Mu: " << tube.reduction << " dB GR, harmonics "
              << (100.0f * tube.harmonics) << "%\n";

    /*  Clean has to stay clean: it is the reference the others are heard
        against, and a compressor that colours when set to Clean gives the user
        nowhere to stand.
    */
    CHECK(pure.harmonics < 0.005f);

    // Every coloured setting has to actually colour, or the choice is a label.
    CHECK(fet.harmonics > pure.harmonics * 10.0f);
    CHECK(tube.harmonics > pure.harmonics * 10.0f);
    // A valve is the even-harmonic one; a FET mostly is not. They must not be
    // the same voice wearing two names.
    CHECK(std::abs(tube.harmonics - fet.harmonics) > 0.002f);

    /*  Feedback designs converge on a reduction rather than being handed one,
        so at the same threshold and ratio they hold back less than the
        feedforward reference. That is the topology showing up in a number.
    */
    CHECK(fet.reduction < pure.reduction);
    CHECK(tube.reduction < pure.reduction);
    CHECK(vca.reduction > 0.5f);

    std::cout << "PASS: compressor characters differ in colour and in how the loop settles\n";
}

void checkAutoMix()
{
    using Profile = VoiceProfileEngine::Profile;
    const auto voice = [](float low, float mid, float high, float crest, float confidence) {
        Profile p; p.ready = true; p.avgRmsDb = -18.0f;
        p.lowMid = low; p.presence = mid; p.sibilance = high;
        p.crestDb = crest; p.pitchConfidence = confidence; p.pitchRangeSemitones = 12.0f;
        return p;
    };

    // An unready profile must leave the defaults alone: a capture that heard
    // nothing usable has no business rewriting the chain.
    CHECK(voxera::decide(Profile{}).punch == voxera::MixSettings{}.punch);

    // Balanced, controlled, clearly pitched.
    const auto neutral = voxera::decide(voice(0.55f, 0.32f, 0.13f, 8.0f, 0.9f));
    // Muddy: the same voice with the low band dominating.
    const auto muddy = voxera::decide(voice(0.80f, 0.15f, 0.05f, 8.0f, 0.9f));
    // Dull: almost nothing above 4 kHz.
    const auto dull = voxera::decide(voice(0.60f, 0.37f, 0.03f, 8.0f, 0.9f));
    // Sibilant: far too much.
    const auto essy = voxera::decide(voice(0.45f, 0.25f, 0.30f, 8.0f, 0.9f));
    // Wildly dynamic, and barely pitched at all.
    const auto loose = voxera::decide(voice(0.55f, 0.32f, 0.13f, 22.0f, 0.1f));

    CHECK(muddy.clean > neutral.clean);          // more high-pass work
    CHECK(muddy.bodyDb < neutral.bodyDb);        // and low end taken out
    CHECK(muddy.smartEQAmount > neutral.smartEQAmount);
    CHECK(dull.exciter > neutral.exciter);       // generate the missing top
    CHECK(dull.airDb > neutral.airDb);
    CHECK(essy.deEss > neutral.deEss);
    CHECK(essy.exciter <= neutral.exciter);      // never add top to a harsh take
    CHECK(loose.punch > neutral.punch);          // dynamics need parallel support
    CHECK(loose.compThreshold < neutral.compThreshold);
    CHECK(loose.tuneAmount < neutral.tuneAmount);  // do not hard-tune unpitched material

    // Every field must stay inside the range its parameter accepts.
    for (const auto& s : {neutral, muddy, dull, essy, loose}) {
        CHECK(s.clean >= 0.0f && s.clean <= 100.0f);
        CHECK(s.deEss >= 0.0f && s.deEss <= 100.0f);
        CHECK(s.punch >= 0.0f && s.punch <= 100.0f);
        CHECK(s.exciter >= 0.0f && s.exciter <= 100.0f);
        CHECK(s.smartEQAmount >= 0.0f && s.smartEQAmount <= 100.0f);
        CHECK(s.bodyDb >= -6.0f && s.bodyDb <= 6.0f);
        CHECK(s.presenceDb >= -6.0f && s.presenceDb <= 6.0f);
        CHECK(s.airDb >= -8.0f && s.airDb <= 8.0f);
        CHECK(s.compThreshold >= -48.0f && s.compThreshold <= 0.0f);
        CHECK(s.compRatio >= 1.0f && s.compRatio <= 20.0f);
        CHECK(s.satDrive >= 0.0f && s.satDrive <= 24.0f);
        CHECK(s.satMix >= 0.0f && s.satMix <= 100.0f);
        CHECK(s.tuneAmount >= 0.0f && s.tuneAmount <= 100.0f);
        CHECK(s.retune >= 0.0f && s.retune <= 100.0f);
        CHECK(s.humanize >= 0.0f && s.humanize <= 100.0f);
        CHECK(s.optical >= 0.0f && s.optical <= 100.0f);
        CHECK(s.clipAmount >= 0.0f && s.clipAmount <= 100.0f);
        CHECK(s.gateThresholdDb >= -80.0f && s.gateThresholdDb <= -20.0f);
        CHECK(s.density >= 0.0f && s.density <= 100.0f);
        CHECK(s.vocalLock >= 0.0f && s.vocalLock <= 100.0f);
        CHECK(s.satWarmth >= 0.0f && s.satWarmth <= 100.0f);
        // Never zero: without it the chain cannot make an even harmonic below
        // the exciter's band, whatever else it is doing.
        CHECK(s.satWarmth > 10.0f);
    }
    CHECK(loose.density > neutral.density);
    CHECK(muddy.vocalLock > neutral.vocalLock);
    CHECK(loose.optical > neutral.optical);
    std::cout << "PASS: auto-mix reacts to mud, dullness, sibilance and dynamics, stays in range\n";
}

/*  PSOLA: does it move the pitch, keep the level, and stay quiet between grains.

    The three ways this method fails are all audible and all measurable. It can
    fail to shift at all, which the detector catches. It can modulate the level
    where the windows join, which shows up as a moving RMS on a steady note. And
    it can click at every cut, which is the one that matters most and the one a
    frequency reading will not show — a buzz at the grain rate sits under the
    note without changing it, so it is measured here as energy appearing above
    where a pure tone has any business putting it.
*/
void checkPsola()
{
    for (double sr : {44100.0, 48000.0, 96000.0})
    {
        const float sourceHz = 180.0f;
        const int count = static_cast<int>(sr * 1.5);

        for (float semitones : {0.0f, 2.0f, -3.0f, 7.0f})
        {
            voxera::PsolaShifter psola;
            psola.prepare(sr, 1);
            psola.setPitch(sourceHz, 0.9f);
            psola.setShiftRatio(std::pow(2.0f, semitones / 12.0f));

            /*  Six harmonics at equal weight rather than a bare sine.

                A pure tone is the one signal this method is entitled to get
                the level wrong on: preserving the spectral envelope while
                moving the pitch means the output takes its amplitude from
                whatever the envelope holds at the new frequency, and a sine's
                envelope holds nothing anywhere but at the old one. Measuring
                against it would be measuring formant preservation working and
                calling it a level fault. A flat harmonic series has the same
                envelope wherever the note is moved to, so what is left to
                measure is the overlap-add, which is the thing under test.
            */
            juce::AudioBuffer<float> buffer(1, count);
            for (int i = 0; i < count; ++i) {
                float value = 0.0f;
                for (int h = 1; h <= 6; ++h)
                    value += std::sin(static_cast<float>(
                        2.0 * 3.141592653589793 * sourceHz * h * i / sr));
                buffer.setSample(0, i, 0.12f * value);
            }

            for (int at = 0; at < count; at += 128) {
                const int piece = std::min(128, count - at);
                juce::AudioBuffer<float> slice(buffer.getArrayOfWritePointers(), 1, at, piece);
                psola.process(slice);
            }

            // Frequency, read from the settled second half so the ramp-up of the
            // grain train is not included.
            YinPitchDetector yin; yin.prepare(sr);
            for (int i = count / 2; i < count; ++i) yin.pushSample(buffer.getSample(0, i));
            const float expected = sourceHz * std::pow(2.0f, semitones / 12.0f);
            const float cents = std::abs(1200.0f * std::log2(yin.getFrequencyHz() / expected));
            std::cout << "PSOLA " << sr << "Hz " << semitones << "st -> "
                      << yin.getFrequencyHz() << "Hz, error " << cents << " cents\n";
            // Measured under one cent everywhere. Held near that rather than at
            // a comfortable margin, because the failure this guards against —
            // rounding the mark spacing to whole samples — shows up as a few
            // cents of constant sharpness and nothing else, and a loose bound
            // would let it back in unnoticed.
            CHECK(yin.isVoiced() && cents < 3.0f);

            // Level, compared in two halves of the settled region. A window
            // overlap that does not sum to unity shows up here as drift.
            auto rms = [&](int from, int to) {
                double sum = 0.0;
                for (int i = from; i < to; ++i) sum += static_cast<double>(buffer.getSample(0, i))
                                                     * buffer.getSample(0, i);
                return std::sqrt(sum / std::max(1, to - from));
            };
            const double firstHalf = rms(count / 2, count * 3 / 4);
            const double secondHalf = rms(count * 3 / 4, count);
            const double drift = std::abs(20.0 * std::log10(std::max(1e-9, firstHalf)
                                                          / std::max(1e-9, secondHalf)));
            // Six sines at unrelated phases: the powers add, so the RMS is the
            // amplitude times the root of half the count.
            const double sourceRms = 0.12 * std::sqrt(6.0 / 2.0);
            const double offset = 20.0 * std::log10(std::max(1e-9, secondHalf) / sourceRms);
            std::cout << "  level drift " << drift << " dB, offset " << offset << " dB\n";
            CHECK(drift < 1.5 && std::abs(offset) < 4.0);
        }

        /*  Grain clicks. A cut in the wrong place is a step in the waveform, and
            a step is broadband — so with a pure tone in, everything above the
            fourth harmonic of the shifted note is either a click or nothing.
        */
        voxera::PsolaShifter psola;
        psola.prepare(sr, 1);
        psola.setPitch(sourceHz, 0.9f);
        psola.setShiftRatio(std::pow(2.0f, 4.0f / 12.0f));

        juce::AudioBuffer<float> buffer(1, count);
        for (int i = 0; i < count; ++i)
            buffer.setSample(0, i, 0.3f * std::sin(
                static_cast<float>(2.0 * 3.141592653589793 * sourceHz * i / sr)));
        for (int at = 0; at < count; at += 128) {
            const int piece = std::min(128, count - at);
            juce::AudioBuffer<float> slice(buffer.getArrayOfWritePointers(), 1, at, piece);
            psola.process(slice);
        }

        // A one-pole high pass repeated four times, well above any harmonic the
        // note itself contributes.
        const double cutoff = sourceHz * std::pow(2.0f, 4.0f / 12.0f) * 6.0;
        const double coeff = std::exp(-2.0 * 3.141592653589793 * cutoff / sr);
        double state[4] = {0, 0, 0, 0};
        double high = 0.0, total = 0.0;
        for (int i = count / 2; i < count; ++i) {
            double x = buffer.getSample(0, i);
            total += x * x;
            for (auto& s : state) { s = (1.0 - coeff) * x + coeff * s; x -= s; }
            high += x * x;
        }
        const double aboveDb = 10.0 * std::log10(std::max(1e-12, high) / std::max(1e-12, total));
        std::cout << "  energy above 6x note: " << aboveDb << " dB\n";
        CHECK(aboveDb < -30.0);
    }
    /*  Formant shifting: the note must not move, and the timbre must.

        Both halves matter and they fail differently. A formant control that
        drags the pitch with it is not a formant control at all — it is a
        second tuning knob fighting the first. And one that leaves the pitch
        alone but does nothing to the spectrum is worse than useless, because
        it sounds like it works: the level barely changes, so the only way to
        catch it is to measure where the energy sits.
    */
    const double sr = 48000.0;
    const float noteHz = 200.0f;
    const int count = static_cast<int>(sr * 1.5);

    float centroidLow = 0.0f, centroidHigh = 0.0f;
    for (int direction : {-1, 0, 1})
    {
        voxera::PsolaShifter psola;
        psola.prepare(sr, 1);
        psola.setPitch(noteHz, 0.9f);
        psola.setShiftRatio(1.0f);
        psola.setFormantRatio(std::pow(2.0f, static_cast<float>(direction) * 5.0f / 12.0f));

        /*  A source with an actual resonance in it — a bump centred on the
            fourth harmonic — rather than a plain harmonic series.

            A 1/h series is the one shape this test cannot use: scaling its
            envelope in frequency is the same as changing its gain, so a
            working formant shift and a broken one produce the same spectrum
            and the measurement can only ever pass. A bump has somewhere to
            move to.
        */
        juce::AudioBuffer<float> buffer(1, count);
        for (int i = 0; i < count; ++i) {
            float value = 0.0f;
            for (int h = 1; h <= 14; ++h) {
                const float offset = static_cast<float>(h) - 4.0f;
                value += std::exp(-offset * offset / 3.0f) * std::sin(static_cast<float>(
                    2.0 * 3.141592653589793 * noteHz * h * i / sr));
            }
            buffer.setSample(0, i, 0.25f * value);
        }
        for (int at = 0; at < count; at += 128) {
            const int piece = std::min(128, count - at);
            juce::AudioBuffer<float> slice(buffer.getArrayOfWritePointers(), 1, at, piece);
            psola.process(slice);
        }

        YinPitchDetector yin; yin.prepare(sr);
        for (int i = count / 2; i < count; ++i) yin.pushSample(buffer.getSample(0, i));
        const float cents = std::abs(1200.0f * std::log2(yin.getFrequencyHz() / noteHz));

        /*  The spectral centroid, measured one harmonic at a time by Goertzel.

            Counting zero crossings would be cheaper and would have been wrong:
            with a fundamental this strong the rate is pinned at twice the note
            whatever the harmonics above it are doing, so it reports the pitch
            and calls it brightness.
        */
        double weighted = 0.0, total = 0.0;
        for (int h = 1; h <= 14; ++h) {
            const double hz = noteHz * h;
            const double omega = 2.0 * 3.141592653589793 * hz / sr;
            const double coeff = 2.0 * std::cos(omega);
            double s1 = 0.0, s2 = 0.0;
            for (int i = count / 2; i < count; ++i) {
                const double s = buffer.getSample(0, i) + coeff * s1 - s2;
                s2 = s1; s1 = s;
            }
            const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
            weighted += power * hz;
            total += power;
        }
        const float rate = static_cast<float>(weighted / std::max(1e-12, total));

        std::cout << "PSOLA formant " << (direction * 5) << "st -> pitch "
                  << yin.getFrequencyHz() << "Hz (" << cents << " cents), brightness "
                  << rate << "\n";
        CHECK(yin.isVoiced() && cents < 3.0f);

        if (direction < 0) centroidLow = rate;
        if (direction > 0) centroidHigh = rate;
    }
    CHECK(centroidHigh > centroidLow * 1.3f);

    std::cout << "PASS: PSOLA pitch accuracy, level continuity, grain-join cleanliness,"
                 " formants move independently of the note\n";
}

/*  What the two engines cost, through the real pitch engine rather than the
    shifter on its own.

    Latency is checked the only way worth checking it: by sending an impulse
    through and finding where it comes out. A reported figure the host trusts
    and a real figure that differs is worse than either being large, because
    everything else in the session gets aligned to the wrong one.
*/
void checkPitchEngines()
{
    for (int which : {0, 1})
    {
        const char* name = which == 0 ? "RubberBand" : "PSOLA";

        PitchEngine engine;
        engine.prepare(48000, 512, 1);
        engine.setEngine(which);
        engine.setEnabled(false);

        juce::AudioBuffer<float> impulse(1, 32768);
        impulse.clear();
        impulse.setSample(0, 0, 0.5f);
        for (int at = 0; at < impulse.getNumSamples(); at += 512) {
            juce::AudioBuffer<float> slice(impulse.getArrayOfWritePointers(), 1, at, 512);
            engine.process(slice);
        }

        int peak = 0;
        for (int i = 1; i < impulse.getNumSamples(); ++i)
            if (std::abs(impulse.getSample(0, i)) > std::abs(impulse.getSample(0, peak))) peak = i;

        const int reported = engine.getLatencySamples();
        std::cout << name << " latency reported " << reported << " samples ("
                  << (1000.0 * reported / 48000.0) << " ms), impulse at " << peak << "\n";
        CHECK(std::abs(peak - reported) <= 2);

        // Cost, on a signal that keeps the engine doing real work throughout.
        PitchEngine timed;
        timed.prepare(48000, 512, 1);
        timed.setEngine(which);
        timed.setEnabled(true);
        timed.setAmount(1.0f);

        const int seconds = 10;
        juce::AudioBuffer<float> tone(1, 48000 * seconds);
        for (int i = 0; i < tone.getNumSamples(); ++i)
            tone.setSample(0, i, 0.25f * std::sin(
                static_cast<float>(2.0 * 3.141592653589793 * 187.0 * i / 48000.0)));

        const auto started = std::chrono::steady_clock::now();
        for (int at = 0; at + 512 <= tone.getNumSamples(); at += 512) {
            juce::AudioBuffer<float> slice(tone.getArrayOfWritePointers(), 1, at, 512);
            timed.process(slice);
        }
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();

        std::cout << "  " << name << " cost " << (100.0 * elapsed / seconds)
                  << "% of one core (mono, 48 kHz)\n";
    }
    std::cout << "PASS: both pitch engines report the latency they actually have\n";
}

int main() {
    checkPsola();
    checkPitchEngines();
    checkSmartEQ();
    checkBypass<AdaptiveSpectralEngine>(); checkBypass<SpatialEngine>();
    checkSpectralPartitioning(); checkSlowDelay();
    std::cout << "PASS: module bypass fades, spectral partitioning, tempo delay\n";
    checkLimiter(); checkExciter(); checkPunch();
    checkGate(); checkSoftClip(); checkOptical(); checkCharacter();
    checkUpward(); checkWarmth(); checkDoubler(); checkVocalLock();
    checkChopAndCrush(); checkModulationAndGlue();
    checkColourCompressor(); checkAutoMix();
    for (double sr : {44100.0, 48000.0, 96000.0}) {
        for (float hz : {80.0f, 110.0f, 220.0f, 440.0f, 880.0f}) {
            YinPitchDetector yin; yin.prepare(sr);
            for (int i = 0; i < static_cast<int>(sr * 0.20); ++i)
                yin.pushSample(0.2f * std::sin(static_cast<float>(2.0 * 3.141592653589793 * hz * i / sr)));
            const float cents = std::abs(1200.0f * std::log2(yin.getFrequencyHz() / hz));
            std::cout << "YIN " << sr << "Hz / " << hz << "Hz error " << cents << " cents\n";
            CHECK(yin.isVoiced() && cents < 15.0f);
            for (int i = 0; i < static_cast<int>(sr * 0.15); ++i) yin.pushSample(0.0f);
            CHECK(!yin.isVoiced() && yin.getFrequencyHz() == 0.0f);
        }
        AutoGain gain; gain.prepare(sr);
        juce::AudioBuffer<float> b(2, 511);
        for (int i = 0; i < b.getNumSamples(); ++i) b.setSample(0, i, 0.1f);
        b.copyFrom(1, 0, b, 0, 0, b.getNumSamples()); gain.process(b);
        for (int i = 0; i < b.getNumSamples(); ++i) CHECK(b.getSample(0, i) == b.getSample(1, i));
        for (int channels : {1, 2}) {
            Saturator sat; sat.prepare({sr, 64, static_cast<juce::uint32>(channels)}); sat.setMix(0.0f);
            juce::AudioBuffer<float> impulse(channels, 4096); impulse.clear();
            for (int ch = 0; ch < channels; ++ch) impulse.setSample(ch, 0, 0.5f);
            sat.process(impulse);
            const int latency = static_cast<int>(sat.getLatencySamples());
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < impulse.getNumSamples(); ++i)
                    CHECK(std::abs(impulse.getSample(ch, i) - (i == latency ? 0.5f : 0.0f)) < 1.0e-6f);
            sat.setMix(0.7f); sat.setDriveDb(18.0f);
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < impulse.getNumSamples(); ++i) impulse.setSample(ch, i, 0.1f * std::sin(i * 0.03f));
            sat.process(impulse);
            for (int i = 0; i < impulse.getNumSamples(); ++i) {
                CHECK(std::isfinite(impulse.getSample(0, i)));
                if (channels == 2) CHECK(impulse.getSample(0, i) == impulse.getSample(1, i));
            }
        }
    }
    VoiceProfileEngine profile; profile.prepare(48000); profile.startCapture();
    for (int i = 0; i < 384000; ++i) profile.pushSample(0.0f);
    CHECK(!profile.getProfile().ready);
    profile.startCapture();
    for (int i = 0; i < 384000; ++i) profile.pushSample(0.2f * std::sin(i * 0.031f));
    CHECK(profile.getProfile().ready);
    VoiceProfileEngine restored; restored.prepare(96000); restored.restoreProfile(profile.getProfile());
    CHECK(restored.getProfile().ready && restored.getProfile().avgRmsDb == profile.getProfile().avgRmsDb);
    ProfileTransfer transfer;
    std::thread writer([&] { for (int i = 0; i < 10000; ++i) {
        VoiceProfileEngine::Profile p; p.avgRmsDb = static_cast<float>(i); p.crestDb = static_cast<float>(-i); transfer.publish(p);
    }});
    for (int i = 0; i < 10000; ++i) { auto p = transfer.read(); CHECK(p.avgRmsDb == -60.0f || p.avgRmsDb == -p.crestDb); }
    writer.join();
    PitchEngine pitch; pitch.prepare(48000, 64, 1); pitch.setEnabled(false);
    juce::AudioBuffer<float> signal(1, 16384); signal.clear(); signal.setSample(0, 0, 0.5f);
    pitch.process(signal);
    int peak = 0; for (int i = 1; i < signal.getNumSamples(); ++i)
        if (std::abs(signal.getSample(0, i)) > std::abs(signal.getSample(0, peak))) peak = i;
    std::cout << "Pitch impulse peak " << peak << " reported " << pitch.getLatencySamples() << "\n";
    CHECK(std::abs(peak - pitch.getLatencySamples()) <= 1);
    std::cout << "PASS: YIN, linked auto-gain, mono/stereo saturation, oversize blocks, profile transfer, pitch latency\n";
}

#include "../Source/DSP/SmartEQ.h"
#include "../Source/DSP/Biquad.h"
#include "../Source/DSP/Limiter.h"
#include "../Source/DSP/Punch.h"
#include "../Source/DSP/Exciter.h"
#include "../Source/DSP/Gate.h"
#include "../Source/DSP/Optical.h"
#include "../Source/DSP/Upward.h"
#include "../Source/DSP/VocalLock.h"
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
        // 4 kHz sits inside the excited band, so squaring must put its whole
        // output at 8 kHz. Any energy at 12 kHz would be a third harmonic, which
        // a second-order stage cannot produce — that is what separates this from
        // a saturator, whose third harmonic would also alias at 44.1 kHz.
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i) dry.setSample(ch, i, sine(fundamental, i, sr, 0.25f));

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
    std::cout << "PASS: warmth turns a symmetric shaper into one with even harmonics\n";
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
    }
    CHECK(loose.density > neutral.density);
    CHECK(muddy.vocalLock > neutral.vocalLock);
    CHECK(loose.optical > neutral.optical);
    std::cout << "PASS: auto-mix reacts to mud, dullness, sibilance and dynamics, stays in range\n";
}

int main() {
    checkSmartEQ();
    checkBypass<AdaptiveSpectralEngine>(); checkBypass<SpatialEngine>();
    checkSpectralPartitioning(); checkSlowDelay();
    std::cout << "PASS: module bypass fades, spectral partitioning, tempo delay\n";
    checkLimiter(); checkExciter(); checkPunch();
    checkGate(); checkSoftClip(); checkOptical(); checkCharacter();
    checkUpward(); checkWarmth(); checkVocalLock(); checkAutoMix();
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

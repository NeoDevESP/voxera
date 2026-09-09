#pragma once
#include <JuceHeader.h>
#include "VoiceProfileEngine.h"

namespace voxera
{
/*  Auto-mix decision layer.

    `decide` is a pure function from the eight numbers the voice profiler
    measures to a setting for every stage in the chain. Keeping it pure and free
    of state is the point: it is the one seam where a trained model would go, and
    swapping this implementation for an inference call would require no change
    anywhere else. What blocks that swap today is not the code but the data —
    a network here needs a corpus of voices paired with settings a mixer judged
    correct, and no such corpus exists yet.

    The rules below are stated as deviations from a reference balance rather than
    as absolute thresholds. Absolute band energies depend on the singer, the mic
    and the gain staging; the ratios between bands are what actually says a voice
    is muddy or dull, and they survive all three.
*/
struct MixSettings
{
    float clean = 55.0f;
    float bodyDb = 0.0f;
    float presenceDb = 0.0f;
    float airDb = 0.0f;
    float deEss = 55.0f;
    float smartEQAmount = 0.0f;
    float vocalLock = 0.0f;

    float compThreshold = -18.0f;
    float compRatio = 3.0f;
    float optical = 0.0f;
    float density = 0.0f;
    float punch = 0.0f;
    float clipAmount = 0.0f;
    float gateThresholdDb = -55.0f;

    float satDrive = 4.0f;
    float satMix = 15.0f;
    float satWarmth = 0.0f;
    int compType = 1;   // 0 Clean, 1 FET, 2 VCA, 3 Vari-Mu
    float exciter = 0.0f;

    float tuneAmount = 100.0f;
    float retune = 65.0f;
    float humanize = 35.0f;
};

/*  Band balance of a vocal that needs no corrective EQ, as this profiler's
    one-pole band proxies measure it. These are shares of the three bands' own
    total, so they do not move with input level.
*/
namespace reference
{
    static constexpr float lowShare = 0.55f;
    static constexpr float midShare = 0.32f;
    static constexpr float highShare = 0.13f;
}

/*  What the analysis concluded about the take, before any of it is turned into
    parameter values.

    Keeping this separate is what lets the plugin say why it did something rather
    than only what it did. Each field is a plain judgement a mixer would make in
    words — this take is muddy, this one is dull — on a 0 to 1 scale, and every
    setting below is derived from these and nothing else.
*/
struct Diagnosis
{
    float mud = 0.0f;       // too much energy below the mids for this voice
    float hollow = 0.0f;    // scooped middle
    float dull = 0.0f;      // not enough top
    float harsh = 0.0f;     // too much top, or sibilant
    float dynamics = 0.0f;  // how far peaks sit above the body of the take
    float tonal = 0.0f;     // how reliably the detector found a pitch
    float range = 0.0f;     // how wide the melody was
    float averageDb = -60.0f;  // the take's own level, which the gate is placed against
    bool ready = false;
};

inline Diagnosis diagnose(const VoiceProfileEngine::Profile& profile)
{
    Diagnosis d;
    if (!profile.ready) return d;

    const auto unit = [](float value) { return juce::jlimit(0.0f, 1.0f, value); };

    const float total = profile.lowMid + profile.presence + profile.sibilance;
    if (total <= 1.0e-9f) return d;

    const float low = profile.lowMid / total;
    const float mid = profile.presence / total;
    const float high = profile.sibilance / total;

    // The divisors are the deviation at which each correction reaches full
    // strength, not a threshold: the response is proportional throughout.
    d.mud    = unit((low - reference::lowShare) / 0.20f);
    d.hollow = unit((reference::midShare - mid) / 0.15f);
    d.dull   = unit((reference::highShare - high) / 0.10f);
    d.harsh  = unit((high - reference::highShare) / 0.10f);

    // Crest factor is what separates a controlled take from one that needs the
    // compressor to do real work.
    d.dynamics = unit((profile.crestDb - 8.0f) / 12.0f);

    // Only commit to hard tuning on material the detector was confident about;
    // breathy or spoken passages are made worse by it.
    d.tonal = unit(profile.pitchConfidence);
    d.range = unit(profile.pitchRangeSemitones / 24.0f);
    d.averageDb = profile.avgRmsDb;

    d.ready = true;
    return d;
}

inline MixSettings decide(const Diagnosis& d)
{
    MixSettings settings;
    if (!d.ready) return settings;

    const float mud = d.mud, hollow = d.hollow, dull = d.dull, harsh = d.harsh;
    const float dynamics = d.dynamics, tonal = d.tonal, range = d.range;

    settings.clean         = 15.0f + 60.0f * mud;
    settings.bodyDb        = -5.0f * mud;
    settings.presenceDb    = 3.5f * hollow;
    settings.airDb         = 5.0f * dull - 2.0f * harsh;
    settings.deEss         = 30.0f + 55.0f * harsh;
    settings.smartEQAmount = 10.0f + 50.0f * mud;
    // Worth leaning on for a muddy take, since the whole point of that stage is
    // to put the cut where this singer's boxiness actually is.
    // Preserve natural body on clean takes; room/proximity correction follows measured mud.
    settings.vocalLock     = 20.0f + 70.0f * mud;

    settings.compThreshold = -12.0f - 12.0f * dynamics;
    settings.compRatio     = 1.8f + 1.7f * dynamics;
    settings.optical       = 10.0f + 40.0f * dynamics;
    // The wider the take's own range, the more of it is sitting below where a
    // listener can follow it under a track, and the more there is to raise.
    settings.density       = 10.0f + 40.0f * dynamics;
    settings.punch         = 10.0f + 40.0f * dynamics;
    settings.clipAmount    = 15.0f * dynamics;

    /*  The gate is placed relative to the voice rather than at a fixed level.
        Thirty-five dB below the take's own average sits under any sung or spoken
        content while still being above the room tone of an untreated space, and
        it tracks the singer's level instead of assuming a gain staging.
    */
    settings.gateThresholdDb = juce::jlimit(-80.0f, -20.0f, d.averageDb - 35.0f);

    settings.satDrive = 3.0f + 5.0f * dynamics;
    settings.satMix   = 10.0f + 15.0f * dynamics;
    /*  Even harmonics fill in a voice that reads as thin, which is what a
        scooped middle or a missing top sounds like. Never zero: this is the
        only stage in the chain that can produce them below the exciter's band,
        and a take with none at all sounds like what it is — a recording that
        never went through anything.
    */
    settings.satWarmth = 25.0f + 30.0f * hollow + 20.0f * dull;

    /*  Which gain element suits the take.

        A wide range needs something that catches transients before they are
        gone, which is what a FET does and a valve deliberately does not. A take
        that is already controlled has nothing left to catch, so the slower
        element earns its place instead: its programme-dependent release and
        even harmonics are audible as character rather than as work being done.
    */
    settings.compType = dynamics > 0.55f ? 1 : 3;
    // Generating top only helps a take that lacks it; on a bright voice this
    // stays near zero and the Air shelf handles the rest.
    settings.exciter  = 60.0f * dull;

    settings.tuneAmount = 55.0f + 45.0f * tonal;
    settings.retune     = 35.0f + 40.0f * tonal;
    // A wide melody needs the correction to let go between notes, or every
    // interval arrives sounding stepped.
    settings.humanize   = juce::jlimit(10.0f, 90.0f, 55.0f - 35.0f * tonal + 25.0f * range);

    return settings;
}

inline MixSettings decide(const VoiceProfileEngine::Profile& profile)
{
    return decide(diagnose(profile));
}

/*  Renders the reasoning in plain language.

    A stage that moves twenty controls on its own is worth nothing if the person
    using it cannot tell whether it was right. Reading back the judgement in the
    same words a mixer would use lets them disagree with it: if it says the take
    is dull and it plainly is not, they know to distrust the top end it added
    rather than wondering which of twenty knobs to undo.
*/
inline juce::String describe(const Diagnosis& d, const MixSettings& s)
{
    if (!d.ready)
        return "No usable take was captured. Play audio while the analysis runs.";

    const auto scale = [](float value) {
        if (value < 0.12f) return "none";
        if (value < 0.35f) return "slight";
        if (value < 0.65f) return "moderate";
        if (value < 0.85f) return "strong";
        return "extreme";
    };
    const auto one = [](float value) { return juce::String(value, 1); };

    juce::StringArray lines;
    lines.add("HEARD");
    lines.add("   muddy " + juce::String(scale(d.mud))
              + "   hollow " + scale(d.hollow)
              + "   dull " + scale(d.dull)
              + "   harsh " + scale(d.harsh));
    lines.add("   dynamics " + juce::String(scale(d.dynamics))
              + "   pitched " + scale(d.tonal)
              + "   level " + one(d.averageDb) + " dB");
    lines.add("");
    lines.add("DID");

    // Only the moves that are actually doing something are worth listing; a
    // report padded with settings at rest hides the ones that matter.
    if (d.mud > 0.12f)
        lines.add("   Clean " + one(s.clean) + " and body " + one(s.bodyDb)
                  + " dB: too much weight low down.");
    if (d.hollow > 0.12f)
        lines.add("   Presence +" + one(s.presenceDb) + " dB: the middle was scooped.");
    if (d.dull > 0.12f)
        lines.add("   Exciter " + one(s.exciter) + ", air +" + one(s.airDb)
                  + " dB: generating top the take did not have.");
    if (d.harsh > 0.12f)
        lines.add("   De-ess " + one(s.deEss) + ": sibilance above what this voice needs.");
    if (d.dynamics > 0.12f)
        lines.add("   Density " + one(s.density) + ", punch " + one(s.punch)
                  + ", optical " + one(s.optical) + ": wide range to even out.");
    lines.add("   Vocal Lock " + one(s.vocalLock) + ": corrective EQ tracks your register.");
    lines.add("   Gate at " + one(s.gateThresholdDb) + " dB, 35 below your own level.");
    if (d.tonal < 0.5f)
        lines.add("   Tune held to " + one(s.tuneAmount)
                  + ": the pitch was not clear enough to correct hard.");

    return lines.joinIntoString("\n");
}
}

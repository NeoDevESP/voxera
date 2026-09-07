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

    float compThreshold = -18.0f;
    float compRatio = 3.0f;
    float optical = 0.0f;
    float punch = 0.0f;
    float clipAmount = 0.0f;
    float gateThresholdDb = -55.0f;

    float satDrive = 4.0f;
    float satMix = 15.0f;
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

inline MixSettings decide(const VoiceProfileEngine::Profile& profile)
{
    MixSettings settings;
    if (!profile.ready) return settings;

    const auto unit = [](float value) { return juce::jlimit(0.0f, 1.0f, value); };

    const float total = profile.lowMid + profile.presence + profile.sibilance;
    if (total <= 1.0e-9f) return settings;

    const float low = profile.lowMid / total;
    const float mid = profile.presence / total;
    const float high = profile.sibilance / total;

    // Each of these reads as one word a mixer would use about a take. The
    // divisors are the deviation at which the correction reaches full strength.
    const float mud    = unit((low - reference::lowShare) / 0.20f);
    const float hollow = unit((reference::midShare - mid) / 0.15f);
    const float dull   = unit((reference::highShare - high) / 0.10f);
    const float harsh  = unit((high - reference::highShare) / 0.10f);

    // Crest factor is what separates a controlled take from one that needs the
    // compressor to do real work.
    const float dynamics = unit((profile.crestDb - 8.0f) / 12.0f);

    // Only commit to hard tuning on material the detector was confident about;
    // breathy or spoken passages are made worse by it.
    const float tonal = unit(profile.pitchConfidence);
    const float range = unit(profile.pitchRangeSemitones / 24.0f);

    settings.clean         = 30.0f + 55.0f * mud;
    settings.bodyDb        = -4.0f * mud;
    settings.presenceDb    = 3.5f * hollow;
    settings.airDb         = 5.0f * dull - 2.0f * harsh;
    settings.deEss         = 30.0f + 55.0f * harsh;
    settings.smartEQAmount = 20.0f + 40.0f * mud;

    settings.compThreshold = -12.0f - 12.0f * dynamics;
    settings.compRatio     = 2.5f + 3.0f * dynamics;
    settings.optical       = 25.0f + 40.0f * dynamics;
    settings.punch         = 20.0f + 50.0f * dynamics;
    settings.clipAmount    = 10.0f + 20.0f * dynamics;

    /*  The gate is placed relative to the voice rather than at a fixed level.
        Thirty-five dB below the take's own average sits under any sung or spoken
        content while still being above the room tone of an untreated space, and
        it tracks the singer's level instead of assuming a gain staging.
    */
    settings.gateThresholdDb = juce::jlimit(-80.0f, -20.0f, profile.avgRmsDb - 35.0f);

    settings.satDrive = 3.0f + 5.0f * dynamics;
    settings.satMix   = 10.0f + 15.0f * dynamics;
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
}

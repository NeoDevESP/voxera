#pragma once
#include <algorithm>
#include <cmath>

// Monophonic continuity heuristic, not a trained model or vibrato classifier.
// Isolated octave outliers are held; sustained octave changes are accepted.
class PitchIntelligence
{
public:
    struct Result {
        float correctedHz = 0.0f, transition = 0.0f, vibrato = 0.0f, protection = 0.0f;
    };
    void prepare(double controlRateHz) { rate = std::max(1.0, controlRateHz); reset(); }
    void reset() noexcept
    {
        lastHz = candidateHz = velocity = vibrato = 0.0f;
        candidateSeconds = unvoicedSeconds = 0.0;
    }
    Result process(float hz, float confidence) noexcept
    {
        const double dt = 1.0 / rate;
        if (!std::isfinite(hz) || !std::isfinite(confidence) || hz <= 0.0f || confidence < 0.45f)
        {
            unvoicedSeconds += dt;
            if (unvoicedSeconds >= 0.080) {
                lastHz = candidateHz = velocity = vibrato = 0.0f;
                candidateSeconds = 0.0;
            }
            return {};
        }
        unvoicedSeconds = 0.0;
        float fixed = hz;
        if (lastHz > 0.0f)
        {
            const float delta = cents(hz, lastHz);
            if (std::abs(std::abs(delta) - 1200.0f) < 160.0f)
            {
                if (candidateHz > 0.0f && std::abs(cents(hz, candidateHz)) < 100.0f)
                    candidateSeconds += dt;
                else {
                    candidateHz = hz;
                    candidateSeconds = dt;
                }
                const double hold = confidence >= 0.9f ? 0.060 : 0.100;
                if (candidateSeconds < hold)
                    fixed = delta > 0.0f ? hz * 0.5f : hz * 2.0f;
                else { candidateHz = 0.0f; candidateSeconds = 0.0; }
            }
            else { candidateHz = 0.0f; candidateSeconds = 0.0; }
        }
        Result r;
        r.correctedHz = fixed;
        if (lastHz > 0.0f)
        {
            // Normalise movement to a 10 ms reference, independent of sample rate.
            const float movement = std::abs(cents(fixed, lastHz)) * static_cast<float>(0.010 / dt);
            const float av = static_cast<float>(std::exp(-dt / 0.030));
            velocity = av * velocity + (1.0f - av) * movement;
            r.transition = std::clamp((velocity - 12.0f) / 65.0f, 0.0f, 1.0f);
            const float candidate = std::clamp((movement - 3.0f) / 22.0f, 0.0f, 1.0f) * (1.0f - r.transition);
            const float ab = static_cast<float>(std::exp(-dt / 0.100));
            vibrato = ab * vibrato + (1.0f - ab) * candidate;
            r.vibrato = vibrato;
        }
        r.protection = std::clamp(0.45f * r.transition + 0.35f * r.vibrato
            + 0.20f * std::clamp((0.78f - confidence) / 0.33f, 0.0f, 1.0f), 0.0f, 1.0f);
        lastHz = fixed;
        return r;
    }
private:
    static float cents(float a, float b) noexcept { return 1200.0f * std::log2(a / b); }
    double rate = 100.0, candidateSeconds = 0.0, unvoicedSeconds = 0.0;
    float lastHz = 0.0f, candidateHz = 0.0f, velocity = 0.0f, vibrato = 0.0f;
};

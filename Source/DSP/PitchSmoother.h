#pragma once
#include <algorithm>
#include <cmath>

// Smooth in log-frequency (semitones), with time in seconds rather than blocks.
// Updating the time constant never resets the current value.
class PitchSmoother
{
public:
    void reset() noexcept { current = 0.0f; }
    float process(float targetSemitones, double elapsedSeconds, float timeMs) noexcept
    {
        const float a = static_cast<float>(std::exp(-std::max(0.0, elapsedSeconds)
                                                   / std::max(0.001, timeMs * 0.001)));
        current = targetSemitones + a * (current - targetSemitones);
        return current;
    }
    float getSemitones() const noexcept { return current; }
private:
    float current = 0.0f;
};

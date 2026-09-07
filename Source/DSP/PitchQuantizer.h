#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

class PitchQuantizer
{
public:
    enum class ScaleType
    {
        chromatic = 0,
        major,
        naturalMinor,
        harmonicMinor,
        dorian
    };

    void reset()
    {
        heldTargetMidi = -1000.0f;
    }

    void setRoot(int midiPitchClass) noexcept
    {
        const int newRoot = ((midiPitchClass % 12) + 12) % 12;

        if (newRoot != root)
        {
            root = newRoot;
            heldTargetMidi = -1000.0f;
        }
    }

    void setScale(ScaleType newScale) noexcept
    {
        if (newScale != scale)
        {
            scale = newScale;
            heldTargetMidi = -1000.0f;
        }
    }

    float quantizeHz(float frequencyHz,
                     float confidence,
                     float hysteresisSemitones = 0.18f) noexcept
    {
        if (frequencyHz <= 0.0f || confidence < 0.55f)
            return frequencyHz;

        const float midi =
            69.0f + 12.0f * std::log2(frequencyHz / 440.0f);

        const float candidate = nearestAllowedMidi(midi);

        if (heldTargetMidi < -100.0f)
        {
            heldTargetMidi = candidate;
        }
        else if (candidate != heldTargetMidi)
        {
            const float oldDistance = std::abs(midi - heldTargetMidi);
            const float newDistance = std::abs(midi - candidate);

            // New target must win by a margin. This reduces chatter around
            // the exact midpoint between two allowed notes.
            if (newDistance + hysteresisSemitones < oldDistance)
                heldTargetMidi = candidate;
        }

        return 440.0f * std::pow(2.0f, (heldTargetMidi - 69.0f) / 12.0f);
    }

    float getHeldTargetMidi() const noexcept { return heldTargetMidi; }

private:
    bool pitchClassAllowed(int pc) const noexcept
    {
        pc = ((pc - root) % 12 + 12) % 12;

        switch (scale)
        {
            case ScaleType::chromatic:
                return true;

            case ScaleType::major:
            {
                constexpr std::array<int, 7> notes { 0, 2, 4, 5, 7, 9, 11 };
                for (int n : notes) if (pc == n) return true;
                return false;
            }

            case ScaleType::naturalMinor:
            {
                constexpr std::array<int, 7> notes { 0, 2, 3, 5, 7, 8, 10 };
                for (int n : notes) if (pc == n) return true;
                return false;
            }

            case ScaleType::harmonicMinor:
            {
                constexpr std::array<int, 7> notes { 0, 2, 3, 5, 7, 8, 11 };
                for (int n : notes) if (pc == n) return true;
                return false;
            }

            case ScaleType::dorian:
            {
                constexpr std::array<int, 7> notes { 0, 2, 3, 5, 7, 9, 10 };
                for (int n : notes) if (pc == n) return true;
                return false;
            }
        }

        return true;
    }

    float nearestAllowedMidi(float midi) const noexcept
    {
        const int centre = static_cast<int>(std::lround(midi));

        float best = static_cast<float>(centre);
        float bestDistance = 1000.0f;

        for (int offset = -12; offset <= 12; ++offset)
        {
            const int note = centre + offset;
            const int pc = ((note % 12) + 12) % 12;

            if (!pitchClassAllowed(pc))
                continue;

            const float distance = std::abs(midi - static_cast<float>(note));

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = static_cast<float>(note);
            }
        }

        return best;
    }

    int root = 0;
    ScaleType scale = ScaleType::chromatic;
    float heldTargetMidi = -1000.0f;
};

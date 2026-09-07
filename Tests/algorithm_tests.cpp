#include "../Source/DSP/PitchIntelligence.h"
#include "../Source/DSP/PitchSmoother.h"
#include "Checks.h"
#include <cmath>
#include <iostream>
int main() {
    for (double rate : {50.0, 93.75, 187.5}) {
        PitchIntelligence tracker; tracker.prepare(rate);
        for (int i = 0; i < 20; ++i) tracker.process(220.0f, 0.98f);
        CHECK(std::abs(tracker.process(440.0f, 0.98f).correctedHz - 220.0f) < 0.1f);
        CHECK(std::abs(tracker.process(220.0f, 0.98f).correctedHz - 220.0f) < 0.1f);
        for (int i = 0; i < static_cast<int>(std::ceil(rate * 0.15)); ++i) tracker.process(440.0f, 0.98f);
        CHECK(std::abs(tracker.process(440.0f, 0.98f).correctedHz - 440.0f) < 0.1f);
        for (int i = 0; i < static_cast<int>(std::ceil(rate * 0.1)); ++i) tracker.process(0.0f, 0.0f);
        CHECK(std::abs(tracker.process(110.0f, 0.98f).correctedHz - 110.0f) < 0.1f);
    }
    float previous = -1.0f;
    for (int steps : {10, 100, 1000}) {
        PitchSmoother smoother;
        float result = 0;
        for (int i = 0; i < steps; ++i) result = smoother.process(2.0f, 0.1 / steps, 100.0f);
        CHECK(std::abs(result - 2.0f * (1.0f - std::exp(-1.0f))) < 0.0001f);
        if (previous >= 0) CHECK(std::abs(result - previous) < 0.0001f);
        previous = result;
        const float next = smoother.process(2.0f, 0.001, 200.0f);
        CHECK(next > result && next - result < 0.01f);
    }
    std::cout << "PASS: octave outlier, sustained octave, phrase reset, sample-rate-independent smoothing\n";
}

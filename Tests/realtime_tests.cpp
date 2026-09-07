#include "../Source/DSP/RealtimeUtilities.h"
#include "Checks.h"
#include <iostream>
struct Buffer {
    float samples[2][128];
    Buffer() { for (auto& ch : samples) for (auto& s : ch) s = 1.0f; }
    int getNumSamples() const { return 128; }
    int getNumChannels() const { return 2; }
    float* getWritePointer(int ch) { return samples[ch]; }
};
struct Ramp {
    int steps = 0;
    float getNextValue() { return static_cast<float>(++steps) / 128.0f; }
};
int main() {
    Buffer buffer; Ramp ramp;
    voxera::applyLinkedGain(buffer, ramp);
    CHECK(ramp.steps == 128);
    for (int i = 0; i < 128; ++i) {
        CHECK(buffer.samples[0][i] == buffer.samples[1][i]);
        CHECK(buffer.samples[0][i] == static_cast<float>(i + 1) / 128.0f);
    }
    for (int latency : {0, 1, 7, 32, 512}) {
        voxera::IntegerDelay delay;
        delay.prepare(2, latency);
        for (int i = 0; i < 2048; ++i) {
            CHECK(delay.process(0, i == 0 ? 1.0f : 0.0f) == (i == latency ? 1.0f : 0.0f));
            CHECK(delay.process(1, i == 5 ? 0.5f : 0.0f) == (i == latency + 5 ? 0.5f : 0.0f));
            delay.advance();
        }
        delay.reset();
        for (int i = 0; i < 1024; ++i) {
            CHECK(delay.process(0, 0.0f) == 0.0f);
            CHECK(delay.process(1, 0.0f) == 0.0f);
            delay.advance();
        }
    }
    std::cout << "PASS: stereo ramp, exact delay, channel isolation, reset\n";
}

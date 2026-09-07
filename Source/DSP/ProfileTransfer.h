#pragma once
#include "VoiceProfileEngine.h"
#include <atomic>
#include <thread>

// Fixed-size exchange. Audio only calls try* once and never spins or allocates.
// Non-audio serialization threads may wait for the short value copy.
class ProfileTransfer
{
public:
    using Profile = VoiceProfileEngine::Profile;
    bool tryPublish(const Profile& p) noexcept {
        if (guard.test_and_set(std::memory_order_acquire)) return false;
        value = p; dirty = true;
        guard.clear(std::memory_order_release); return true;
    }
    bool tryRead(Profile& p, bool consume = false) noexcept {
        if (guard.test_and_set(std::memory_order_acquire)) return false;
        const bool available = !consume || dirty;
        if (available) p = value;
        if (consume) dirty = false;
        guard.clear(std::memory_order_release); return available;
    }
    void publish(const Profile& p) {
        while (!tryPublish(p)) std::this_thread::yield();
    }
    Profile read() {
        Profile p;
        while (!tryRead(p)) std::this_thread::yield();
        return p;
    }
private:
    std::atomic_flag guard = ATOMIC_FLAG_INIT;
    Profile value;
    bool dirty = false;
};

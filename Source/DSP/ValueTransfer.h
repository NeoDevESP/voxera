#pragma once
#include <atomic>
#include <thread>

// Audio uses only try operations; all storage is allocated with the processor.
template<class T> class ValueTransfer
{
public:
    bool tryPublish(const T& next) noexcept {
        if (guard.test_and_set(std::memory_order_acquire)) return false;
        value = next; dirty = true;
        guard.clear(std::memory_order_release); return true;
    }
    bool tryRead(T& next, bool consume = false) noexcept {
        if (guard.test_and_set(std::memory_order_acquire)) return false;
        const bool available = !consume || dirty;
        if (available) next = value;
        if (consume) dirty = false;
        guard.clear(std::memory_order_release); return available;
    }
    void publish(const T& next) { while (!tryPublish(next)) std::this_thread::yield(); }
    T read() { T next {}; while (!tryRead(next)) std::this_thread::yield(); return next; }
private:
    std::atomic_flag guard = ATOMIC_FLAG_INIT;
    T value {};
    bool dirty = false;
};

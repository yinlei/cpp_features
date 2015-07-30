#pragma once
#include <atomic>

struct LFLock
{
    volatile std::atomic_flag lck = ATOMIC_FLAG_INIT;

    inline void lock()
    {
        while (std::atomic_flag_test_and_set_explicit(&lck, std::memory_order_acquire));
    }

    inline bool try_lock()
    {
        return !std::atomic_flag_test_and_set_explicit(&lck, std::memory_order_acquire);
    }
    
    inline void unlock()
    {
        std::atomic_flag_clear_explicit(&lck, std::memory_order_release);
    }
};


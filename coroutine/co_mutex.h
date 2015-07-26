#pragma once
#include <mutex>

class CoMutex
{
    static uint64_t s_id;
    std::mutex native_;
    uint64_t id_;

public:
    CoMutex();

    void lock();
    bool try_lock();
    void unlock();
};

typedef CoMutex co_mutex;

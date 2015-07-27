#pragma once
#include <stdint.h>

class CoMutex
{
    static uint64_t s_id;
    uint64_t id_;

public:
    CoMutex();
    ~CoMutex();

    void lock();
    void unlock();
};

typedef CoMutex co_mutex;

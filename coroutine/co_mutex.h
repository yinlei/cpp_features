#pragma once
#include <stdint.h>

/// 协程锁
//  可在协程外创建、try_lock、unlock, 但必须在协程内lock !
class CoMutex
{
    static uint64_t s_id;
    uint64_t id_;

public:
    CoMutex();
    ~CoMutex();

    void lock();
    bool try_lock();
    void unlock();
};

typedef CoMutex co_mutex;

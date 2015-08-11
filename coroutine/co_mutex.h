#pragma once
#include <stdint.h>
#include "block_object.h"

namespace co
{

/// 协程锁
class CoMutex
{
    BlockObject block_;

public:
    CoMutex();

    void lock();
    bool try_lock();
    bool is_lock();
    void unlock();
};

typedef CoMutex co_mutex;

} //namespace co

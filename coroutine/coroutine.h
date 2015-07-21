#pragma once
#include "scheduler.h"

struct __go
{
    template <typename Arg>
    inline void operator-(Arg const& arg)
    {
        Scheduler::getInstance().CreateTask(arg);
    }
};

#define go __go()-
#define yield do { Scheduler::getInstance().Yield(); } while (0)

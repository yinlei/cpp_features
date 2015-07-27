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

// (uint32_t type, uint64_t id)
#define co_wait(type, id) do { Scheduler::getInstance().UserBlockWait(type, id); } while (0)
#define co_wakeup(type, id) do { Scheduler::getInstance().UserBlockWakeup(type, id); } while (0)

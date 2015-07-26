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

// co_block(uint32_t type, uint64_t id, bool yield_immediately = true);
#define co_block(...) do { Scheduler::getInstance().UserBlockSwitch(__VA_ARGS__); } while (0)
#define co_cancel_block() do { Scheduler::getInstance().UserBlockCancel(); } while (0)
#define co_weak(type, id) do { Scheduler::getInstance().UserBlockWeak(type, id); } while (0)

#pragma once
#include "scheduler.h"
#include "channel.h"

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

// coroutine sleep, never blocks current thread.
#define co_sleep(milliseconds) do { Scheduler::getInstance().SleepSwitch(milliseconds); } while (0)

// co_mutex

// co_channel
template <typename T>
using co_chan = Channel<T>;

// co_timer_add will returns timer_id; The timer_id type is uint64_t.
template <typename Arg, typename F>
inline TimerId co_timer_add(Arg const& duration_or_timepoint, F const& callback) {
    return Scheduler::getInstance().ExpireAt(duration_or_timepoint, callback);
}

// co_timer_cancel will returns boolean type;
//   if cancel successfully it returns true,
//   else it returns false;
inline bool co_timer_cancel(TimerId timer_id) {
    return Scheduler::getInstance().CancelTimer(timer_id);
}

// co_timer_block_cancel will returns boolean type;
//   if cancel successfully it returns true,
//   else it returns false;
//
// This function will block wait timer occurred done, if cancel error.
inline bool co_timer_block_cancel(TimerId timer_id) {
    return Scheduler::getInstance().BlockCancelTimer(timer_id);
}


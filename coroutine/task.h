#pragma once
#include <stddef.h>
#include <ucontext.h>
#include <functional>
#include "ts_queue.h"

enum class TaskState
{
    runnable,
    io_block,    // write, writev, read, ...
    sync_block,  // mutex, pthread_lock, ...
    done,
};

typedef std::function<void()> TaskF;

struct Task
    : public TSQueueHook
{
    uint64_t id_;
    TaskState state_;
    ucontext_t ctx_;
    TaskF fn_;
    char* stack_;

    explicit Task(TaskF const& fn, int stack_size);
    ~Task();

    static uint64_t s_id;
};



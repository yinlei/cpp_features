#pragma once
#include <stddef.h>
#include <ucontext.h>
#include <functional>
#include "ts_queue.h"

enum class TaskState
{
    runnable,
    io_block,    // write, writev, read, select, poll, ...
    sys_block,  // mutex, pthread_lock, ...
    user_block,  // mutex, pthread_lock, ...
    done,
    fatal,
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
    std::string debug_info_;
    int wait_fd_;
    int64_t user_wait_type_;
    uint64_t user_wait_id_;

    explicit Task(TaskF const& fn, int stack_size);
    ~Task();

    void SetDebugInfo(std::string const& info);
    const char* DebugInfo();

    static uint64_t s_id;
};



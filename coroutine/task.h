#pragma once
#include <stddef.h>
#include <ucontext.h>
#include <functional>

enum class TaskState
{
    runnable,
    syscall,
    lock,
    done,
};

typedef std::function<void()> TaskF;
struct Task
{
    uint64_t id_;
    TaskState state_;
    ucontext_t ctx_;
    TaskF fn_;
    char* stack_;

    explicit Task(TaskF const& fn);
    ~Task();

    static uint64_t s_id;
};



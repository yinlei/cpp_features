#pragma once
#include <stddef.h>
#include <ucontext.h>
#include <functional>
#include <boost/intrusive/list_hook.hpp>

enum class TaskState
{
    runnable,
    io_block,    // write, writev, read, ...
    sync_block,  // mutex, pthread_lock, ...
    done,
};

typedef std::function<void()> TaskF;

using namespace boost::intrusive;

struct Task
    : public list_base_hook<link_mode<auto_unlink>>
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



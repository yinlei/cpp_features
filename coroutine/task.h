#pragma once
#include <stddef.h>
#include <ucontext.h>
#include <functional>
#include <exception>
#include <vector>
#include <list>
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

struct FdStruct
{
    int fd;
    uint32_t event; // epoll event flags.
};

class BlockObject;
struct Task
    : public TSQueueHook
{
    uint64_t id_;
    TaskState state_;
    ucontext_t ctx_;
    TaskF fn_;
    char* stack_;
    std::string debug_info_;

    std::exception_ptr eptr_;           // 保存exception的指针
    std::atomic<uint32_t> ref_count_;   // 引用计数
    std::vector<FdStruct> wait_fds_;    // io_block等待的fd列表
    int64_t user_wait_type_;            // user_block等待的类型
    uint64_t user_wait_id_;             // user_block等待的id
    BlockObject* block_;                // sys_block等待的block对象

    explicit Task(TaskF const& fn, int stack_size);
    ~Task();

    void SetDebugInfo(std::string const& info);
    const char* DebugInfo();

    static uint64_t s_id;

    void IncrementRef() { ++ref_count_; }
    void DecrementRef() {
        if (--ref_count_ == 0) {
            std::unique_lock<LFLock> lock(s_delete_list_lock);
            s_delete_list.push_back(this);
        }
    }

    typedef std::list<Task*> DeleteList;
    static DeleteList s_delete_list;
    static LFLock s_delete_list_lock;

    static void SwapDeleteList(DeleteList &output);
};



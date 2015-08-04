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

struct FdStruct;
struct Task;

struct EpollPtr
{
    FdStruct* fdst;
    Task* tk;
    uint32_t revent;    // 结果event
};

struct FdStruct
{
    int fd;
    uint32_t event;     // epoll event flags.
    EpollPtr epoll_ptr; // 传递入epoll的指针

    FdStruct() : fd(-1), event(0), epoll_ptr{NULL, NULL, 0} {
        epoll_ptr.fdst = this;
    }
};

class BlockObject;
struct Task
    : public TSQueueHook
{
    uint64_t id_;
    TaskState state_;
    uint64_t yield_count_;
    ucontext_t ctx_;
    TaskF fn_;
    char* stack_;
    std::string debug_info_;
    std::exception_ptr eptr_;           // 保存exception的指针
    std::atomic<uint32_t> ref_count_;   // 引用计数

    std::vector<FdStruct> wait_fds_;    // io_block等待的fd列表
    uint32_t wait_successful_;          // io_block成功等待到的fd数量(用于poll和select)
    LFLock io_block_lock_;              // 当等待的fd多余1个时, 用此锁sync添加到epoll和从epoll删除的操作, 以防在epoll中残留fd, 导致Task无法释放.
    TimerId io_block_timer_;

    int64_t user_wait_type_;            // user_block等待的类型
    uint64_t user_wait_id_;             // user_block等待的id
    BlockObject* block_;                // sys_block等待的block对象

    explicit Task(TaskF const& fn, int stack_size);
    ~Task();

    void SetDebugInfo(std::string const& info);
    const char* DebugInfo();

    static uint64_t s_id;
    static std::atomic<uint64_t> s_task_count;

    void IncrementRef() { ++ref_count_; }
    void DecrementRef() {
        if (--ref_count_ == 0) {
            std::unique_lock<LFLock> lock(s_delete_list_lock);
            s_delete_list.push_back(this);
        }
    }
    static uint64_t GetTaskCount();

    // Task引用计数归0时不要立即释放, 以防epoll_wait取到残余数据时访问野指针.
    typedef std::list<Task*> DeleteList;
    static DeleteList s_delete_list;
    static LFLock s_delete_list_lock;

    static void SwapDeleteList(DeleteList &output);
    static std::size_t GetDeletedTaskCount();
};



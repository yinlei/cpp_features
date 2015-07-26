#pragma once
#include <ucontext.h>
#include <boost/noncopyable.hpp>
#include "task.h"

#define DebugPrint(type, fmt, ...) \
    do { \
        if (g_Scheduler.GetOptions().debug & type) { \
            printf("co_dbg ---- " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

static const uint64_t dbg_all = 0xffffffffffffffffULL;
static const uint64_t dbg_hook = 0x1;
static const uint64_t dbg_yield = 0x1 << 1;
static const uint64_t dbg_scheduler = 0x1 << 2;
static const uint64_t dbg_co_switch = 0x1 << 3;
static const uint64_t dbg_ioblock = 0x1 << 4;
static const uint64_t dbg_task = 0x1 << 5;

struct CoroutineOptions
{
    uint64_t debug = 0;
    uint32_t stack_size = 128 * 1024;
    uint32_t chunk_count = 128;     // Run每次最多从run队列中pop出1/chunk_count * task_count个task.
    uint32_t max_chunk_size = 128;  // Run每次最多从run队列中pop出max_chunk_size个task.
};

struct ThreadLocalInfo
{
    Task* current_task;
    ucontext_t scheduler;
};

class Scheduler : boost::noncopyable
{
    public:
        typedef TSQueue<Task> TaskList;

        static Scheduler& getInstance();

        CoroutineOptions& GetOptions();

        void CreateTask(TaskF const& fn);

        bool IsCoroutine();

        bool IsEmpty();

        void Yield();

        uint32_t Run();
        
        void RunLoop();

        uint32_t TaskCount();

        uint32_t RunnableTaskCount();

        uint64_t GetCurrentTaskID();

    public:
        bool IOBlockSwitch(int fd, uint32_t event);

    private:
        Scheduler();
        ~Scheduler();

        void AddTask(Task* tk);

        ThreadLocalInfo& GetLocalInfo();

        // list of task.
        TaskList run_tasks_;
        TaskList wait_tasks_;
        int epoll_fd_;
        std::atomic<uint32_t> task_count_;
        std::atomic<uint32_t> runnable_task_count_;
};

#define g_Scheduler Scheduler::getInstance()


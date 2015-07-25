#pragma once
#include <ucontext.h>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include "task.h"

#define DebugPrint(fmt, ...) \
    do{ if (g_Scheduler.GetOptions().debug) { printf(fmt "\n", ##__VA_ARGS__); } }while(0)

struct CoroutineOptions
{
    bool debug = false;
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


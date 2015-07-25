#pragma once
#include <ucontext.h>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include "task.h"

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

    private:
        Scheduler();
        ~Scheduler();

        void AddTask(Task* tk);

        ThreadLocalInfo& GetLocalInfo();

        // list of task.
        TaskList run_task_;
        TaskList wait_task_;
        int epoll_fd;
        std::atomic<uint32_t> task_count_;
        std::atomic<uint32_t> runnale_task_count_;
};

#define g_Scheduler Scheduler::getInstance()


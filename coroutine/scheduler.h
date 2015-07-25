#pragma once
#include <ucontext.h>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include "task.h"

struct CoroutineOptions
{
    uint32_t stack_size;
    bool debug;

    CoroutineOptions();
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
};

#define g_Scheduler Scheduler::getInstance()

#pragma once
#include <ucontext.h>
#include <boost/noncopyable.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/thread/mutex.hpp>
#include "task.h"

struct CoroutineOptions
{
    uint32_t stack_size;

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
        typedef list<Task, constant_time_size<false>> TaskList;

        static Scheduler& getInstance();

        ThreadLocalInfo& GetLocalInfo();

        CoroutineOptions& GetOptions();

        void CreateTask(TaskF const& fn);

        bool IsCoroutine();

        bool IsEmpty();

        void Yield();

        void Run();

    private:
        Scheduler();
        ~Scheduler();

        void AddTask(Task* tk);

        // list of task.
        TaskList task_lists_[3];
        TaskList *run_task_;
        TaskList *run2_task_;
        TaskList *wait_task_;
        boost::mutex run2_mutex;
};

#define g_Scheduler Scheduler::getInstance()

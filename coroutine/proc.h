#pragma once
#include <list>
#include <memory>
#include "task.h"

struct Proc
{
    typedef std::list<Task*> TaskList;

    TaskList run_list_;
    TaskList wait_list_;

    ucontext_t proc_ctx_;
    Task* running_task_;

    ~Proc();

    void AddTask(Task* tk);

    void Yield();
    
    void DoSchedule();
};
typedef std::shared_ptr<Proc> ProcPtr;

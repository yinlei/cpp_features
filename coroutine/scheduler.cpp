#include "scheduler.h"
#include <ucontext.h>
#include <assert.h>

Scheduler& Scheduler::getInstance()
{
    static Scheduler obj;
    return obj;
}

Scheduler::~Scheduler()
{
    assert(true);
}

void Scheduler::CreateTask(std::function<void()> const& fn)
{
    if (threads_.empty())
        NewThread();

    Task* tk = new Task(fn);
    threads_[0]->proc_->AddTask(tk);
}

void Scheduler::Yield()
{
    CoreThread* ct = CoreThread::s_core_thread;
    assert(ct);
    ct->Yield();
}

void Scheduler::NewThread()
{
    CoreThreadPtr th(new CoreThread);
    threads_.push_back(th);
}


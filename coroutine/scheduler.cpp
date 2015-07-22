#include "scheduler.h"
#include <ucontext.h>
#include <assert.h>

CoroutineOptions::CoroutineOptions()
{
    stack_size = 128 * 1024;
}

Scheduler& Scheduler::getInstance()
{
    static Scheduler obj;
    return obj;
}

Scheduler::Scheduler()
{
    run_task_ = &task_lists_[0];
    run2_task_ = &task_lists_[0];
    wait_task_ = &task_lists_[0];
}

Scheduler::~Scheduler()
{
}

ThreadLocalInfo& Scheduler::GetLocalInfo()
{
    static thread_local ThreadLocalInfo info;
    return info;
}

CoroutineOptions& Scheduler::GetOptions()
{
    static CoroutineOptions options;
    return options;
}

void Scheduler::CreateTask(TaskF const& fn)
{
    Task* tk = new Task(fn, GetOptions().stack_size);
    AddTask(tk);
}

bool Scheduler::IsCoroutine()
{
    return !!GetLocalInfo().current_task;
}

bool Scheduler::IsEmpty()
{
    return task_lists_[0].empty() && task_lists_[1].empty() && task_lists_[2].empty();
}

void Scheduler::Yield()
{
    Task* tk = GetLocalInfo().current_task;
    if (!tk) return ;

    swapcontext(&tk->ctx_, &GetLocalInfo().scheduler);
}

void Scheduler::Run()
{
    ThreadLocalInfo& info = GetLocalInfo();
    info.current_task = NULL;

    TaskList::iterator it = run_task_->begin();
    while (it != run_task_->end())
    {
        Task* tk = &*it;
        info.current_task = tk;
        swapcontext(&info.scheduler, &tk->ctx_);
        info.current_task = NULL;

        switch (tk->state_) {
            case TaskState::runnable:
                ++it;
                break;

            case TaskState::io_block:
            case TaskState::sync_block:
                wait_task_->push_back(*tk);
                it = run_task_->erase(it);
                break;

            case TaskState::done:
            default:
                it = run_task_->erase(it);
                delete tk;
                break;
        }
    }

    {
        boost::mutex::scoped_lock lck(run2_mutex);
        std::swap(run_task_, run2_task_);
    }
}

void Scheduler::AddTask(Task* tk)
{
    boost::mutex::scoped_lock lck(run2_mutex);
    run2_task_->push_back(*tk);
}

#include "proc.h"
#include <assert.h>
#include <iostream>

Proc::~Proc()
{
    for (auto tk: run_list_)
        delete tk;
    for (auto tk: wait_list_)
        delete tk;
}

void Proc::AddTask(Task* tk)
{
    wait_list_.push_back(tk);
}

void Proc::Yield()
{
    assert(running_task_);
    swapcontext(&running_task_->ctx_, &proc_ctx_);
}

void Proc::DoSchedule()
{
    auto it = run_list_.begin();
    while (it != run_list_.end())
    {
        Task* tk = *it;
        if (tk->state_ == TaskState::runnable) {
            running_task_ = tk;
            swapcontext(&proc_ctx_, &tk->ctx_);
            running_task_ = NULL;
        }

        switch (tk->state_) {
            case TaskState::runnable:
                ++it;
                break;

            case TaskState::syscall:
            case TaskState::lock:
                wait_list_.push_back(tk);
                it = run_list_.erase(it);
                break;

            case TaskState::done:
                delete tk;
                it = run_list_.erase(it);
                break;

            default:
                ++it;
                break;
        }
    }

    it = wait_list_.begin();
    while (it != wait_list_.end())
    {
        Task* tk = *it;
        if (tk->state_ == TaskState::runnable) {
            run_list_.push_front(tk);
            it = wait_list_.erase(it);
        } else {
            ++it;
        }
    }
}


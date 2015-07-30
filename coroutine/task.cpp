#include "task.h"
#include <iostream>
#include "scheduler.h"
#include <string.h>

uint64_t Task::s_id = 0;

static void C_func(Task* self)
{
    try {
        (self->fn_)();
    } catch (...) {
        switch (g_Scheduler.GetOptions().exception_handle) {
            case eCoExHandle::immedaitely_throw:
                throw ;
                break;

            case eCoExHandle::delay_rethrow:
                self->eptr_ = std::current_exception();
                break;

            default:
            case eCoExHandle::debugger_only:
                DebugPrint(dbg_exception, "task(%s) has uncaught exception.", self->DebugInfo());
                break;
        }
    }

    self->state_ = TaskState::done;
    Scheduler::getInstance().Yield();
}

Task::Task(TaskF const& fn, int stack_size)
    : id_(++s_id), state_(TaskState::runnable), fn_(fn),
    ref_count_{1}, wait_fd_(-1), block_(NULL)
{
    stack_ = new char[stack_size];
    if (!stack_) {
        state_ = TaskState::fatal;
        fprintf(stderr, "task(%s) init, new stack error\n", DebugInfo());
        return ;
    }

    if (-1 == getcontext(&ctx_)) {
        state_ = TaskState::fatal;
        fprintf(stderr, "task(%s) init, getcontext error:%s\n",
                DebugInfo(), strerror(errno));
        return ;
    }

    ctx_.uc_stack.ss_sp = stack_;
    ctx_.uc_stack.ss_size = stack_size;
    ctx_.uc_link = NULL;
    makecontext(&ctx_, (void(*)(void))&C_func, 1, this);
}

Task::~Task()
{
    delete []stack_;
}

void Task::SetDebugInfo(std::string const& info)
{
    debug_info_ = info + "(" + std::to_string(id_) + ")";
}

const char* Task::DebugInfo()
{
    if (debug_info_.empty())
        debug_info_ = std::to_string(id_);

    return debug_info_.c_str();
}

void Task::IncrementRef()
{
    ++ref_count_;
}

void Task::DecrementRef()
{
    if (--ref_count_ == 0)
        delete this;
}



#include "task.h"
#include <iostream>
#include "scheduler.h"
#include <string.h>

uint64_t Task::s_id = 0;

static void C_func(Task* self)
{
    (self->fn_)();
    self->state_ = TaskState::done;
    Scheduler::getInstance().Yield();
}

Task::Task(TaskF const& fn, int stack_size)
    : id_(++s_id), state_(TaskState::runnable), fn_(fn), wait_fd_(-1)
{
    stack_ = new char[stack_size];
    if (!stack_) {
        state_ = TaskState::fatal;
        fprintf(stderr, "task(%llu) init, new stack error\n", id_);
        return ;
    }

    if (-1 == getcontext(&ctx_)) {
        state_ = TaskState::fatal;
        fprintf(stderr, "task(%llu) init, getcontext error:%s\n",
                id_, strerror(errno));
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


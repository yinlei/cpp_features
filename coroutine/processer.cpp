#include "processer.h"
#include "scheduler.h"
#include "error.h"
#include "assert.h"

namespace co {

std::atomic<uint32_t> Processer::s_id_{0};

Processer::Processer(uint32_t stack_size)
    : id_(++s_id_)
{
    shared_stack_cap_ = stack_size;
    shared_stack_ = new char[shared_stack_cap_];
}
Processer::~Processer()
{
    if (shared_stack_) {
        delete[] shared_stack_;
        shared_stack_ = NULL;
    }
}

void Processer::AddTaskRunnable(Task *tk)
{
    DebugPrint(dbg_scheduler, "task(%s) add into proc(%u)", tk->DebugInfo(), id_);
    if (tk->state_ == TaskState::init) {
        assert(!tk->proc_);
        tk->AddIntoProcesser(this, shared_stack_, shared_stack_cap_);
        if (tk->state_ == TaskState::fatal) {
            // 创建失败
            delete tk;
            throw std::system_error(errno, std::system_category());
        }
        ++ task_count_;
    }

    assert(tk->proc_ == this);
    tk->state_ = TaskState::runnable;
    runnable_list_.push(tk);
}

uint32_t Processer::Run(ThreadLocalInfo &info, uint32_t &done_count)
{
    info.current_task = NULL;

    uint32_t do_count = runnable_list_.size();;
    done_count = 0;

    DebugPrint(dbg_scheduler, "Run [Proc(%d) do_count:%u] --------------------------", id_, do_count);

    uint32_t c = 0;
    for (; c < do_count; ++c)
    {
        Task* tk = runnable_list_.pop();
        info.current_task = tk;
        tk->state_ = TaskState::runnable;
        DebugPrint(dbg_switch, "enter task(%s)", tk->DebugInfo());
        RestoreStack(tk);
        int ret = swapcontext(&info.scheduler, &tk->ctx_);
        if (ret) {
            fprintf(stderr, "swapcontext error:%s\n", strerror(errno));
            runnable_list_.push(tk);
            ThrowError(eCoErrorCode::ec_swapcontext_failed);
        }
        DebugPrint(dbg_switch, "leave task(%s) state=%d", tk->DebugInfo(), tk->state_);
        info.current_task = NULL;

        switch (tk->state_) {
            case TaskState::runnable:
                runnable_list_.push(tk);
                break;

            case TaskState::io_block:
                g_Scheduler.io_wait_.SchedulerSwitch(tk);
                break;

            case TaskState::sleep:
                g_Scheduler.sleep_wait_.SchedulerSwitch(tk);
                break;

            case TaskState::sys_block:
            case TaskState::user_block:
                {
                    if (tk->block_) {
                        if (!tk->block_->AddWaitTask(tk))
                            runnable_list_.push(tk);
                        tk->block_ = NULL;
                    } else {
                        std::unique_lock<LFLock> lock(g_Scheduler.user_wait_lock_);
                        auto &zone = g_Scheduler.user_wait_tasks_[tk->user_wait_type_];
                        auto &wait_pair = zone[tk->user_wait_id_];
                        auto &task_queue = wait_pair.second;
                        if (wait_pair.first) {
                            --wait_pair.first;
                            tk->state_ = TaskState::runnable;
                        } else {
                            task_queue.push(tk);
                        }
                        g_Scheduler.ClearWaitPairWithoutLock(tk->user_wait_type_,
                                tk->user_wait_id_, zone, wait_pair);
                    }
                }
                break;

            case TaskState::done:
            default:
                --task_count_;
                ++done_count;
                DebugPrint(dbg_task, "task(%s) done.", tk->DebugInfo());
                if (tk->eptr_) {
                    std::exception_ptr ep = tk->eptr_;
                    tk->DecrementRef();
                    std::rethrow_exception(ep);
                } else
                    tk->DecrementRef();
                break;
        }
    }

    return c;
}

void Processer::Yield(ThreadLocalInfo &info)
{
    Task *tk = info.current_task;
    if (!tk) return ;

    DebugPrint(dbg_yield, "yield task(%s) state=%d", tk->DebugInfo(), tk->state_);
    ++tk->yield_count_;
    SaveStack(tk);
    int ret = swapcontext(&tk->ctx_, &info.scheduler);
    if (ret) {
        fprintf(stderr, "swapcontext error:%s\n", strerror(errno));
        ThrowError(eCoErrorCode::ec_yield_failed);
    }
}

uint32_t Processer::GetTaskCount()
{
    return task_count_;
}

void Processer::SaveStack(Task *tk)
{
    char dummy = 0;
    char *top = shared_stack_ + shared_stack_cap_;
    uint32_t current_stack_size = top - &dummy;
    DebugPrint(dbg_scheduler, "task(%s) in proc(%u) save_stack size=%u", tk->DebugInfo(), id_, current_stack_size);
    assert(current_stack_size <= shared_stack_cap_);
    if (tk->stack_capacity_ < current_stack_size) {
        tk->stack_ = (char*)realloc(tk->stack_, current_stack_size);
        tk->stack_capacity_ = current_stack_size;
    }
    tk->stack_size_ = current_stack_size;
    memcpy(tk->stack_, &dummy, tk->stack_size_);
}

void Processer::RestoreStack(Task *tk)
{
    DebugPrint(dbg_scheduler, "task(%s) in proc(%u) restore_stack size=%u", tk->DebugInfo(), id_, tk->stack_size_);
    memcpy(shared_stack_ + shared_stack_cap_ - tk->stack_size_, tk->stack_, tk->stack_size_);
}

} //namespace co

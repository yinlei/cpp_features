#include "scheduler.h"
#include <ucontext.h>
#include <assert.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <string.h>
#include <system_error>

Scheduler& Scheduler::getInstance()
{
    static Scheduler obj;
    return obj;
}

extern void coroutine_hook_init();
Scheduler::Scheduler()
{
    epoll_fd_ = epoll_create(1024);
    if (epoll_fd_ == -1) {
        perror("CoroutineScheduler init failed. epoll create error:");
        assert(false);
    }

    coroutine_hook_init();
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
    if (tk->state_ == TaskState::fatal) {
        // 创建失败
        delete tk;
        throw std::system_error(errno, std::system_category());
        return ;
    }

    ++task_count_;
    DebugPrint(dbg_task, "task(%s) created.", tk->DebugInfo());
    AddTask(tk);
}

bool Scheduler::IsCoroutine()
{
    return !!GetLocalInfo().current_task;
}

bool Scheduler::IsEmpty()
{
    return task_count_ == 0;
}

void Scheduler::Yield()
{
    Task* tk = GetLocalInfo().current_task;
    if (!tk) return ;

    DebugPrint(dbg_yield, "yield task(%s) state=%d", tk->DebugInfo(), tk->state_);
    int ret = swapcontext(&tk->ctx_, &GetLocalInfo().scheduler);
    if (ret) perror("swapcontext error:");
    assert(ret == 0);
}

uint32_t Scheduler::Run()
{
    ThreadLocalInfo& info = GetLocalInfo();
    info.current_task = NULL;
    uint32_t do_max_count = runnable_task_count_;
    uint32_t do_count = 0;

    DebugPrint(dbg_scheduler, "Run [max_count=%u]--------------------------", do_max_count);

    // 每次Run执行的协程数量不能多于当前runnable协程数量
    // 以防wait状态的协程得不到执行。
    while (do_count < do_max_count)
    {
        uint32_t cnt = std::max((uint32_t)1, std::min(
                    do_max_count / GetOptions().chunk_count,
                    GetOptions().max_chunk_size));
        DebugPrint(dbg_scheduler, "want pop %u tasks.", cnt);
        SList<Task> slist = run_tasks_.pop(cnt);
        DebugPrint(dbg_scheduler, "really pop %u tasks.", cnt);
        if (slist.empty()) break;

        SList<Task>::iterator it = slist.begin();
        while (it != slist.end())
        {
            Task* tk = &*it;
            info.current_task = tk;
            tk->state_ = TaskState::runnable;
            DebugPrint(dbg_co_switch, "enter task(%s)", tk->DebugInfo());
            int ret = swapcontext(&info.scheduler, &tk->ctx_);
            if (ret) perror("swapcontext error:");
            assert(ret == 0);
            ++do_count;
            DebugPrint(dbg_co_switch, "exit task(%s) state=%d", tk->DebugInfo(), tk->state_);
            info.current_task = NULL;

            switch (tk->state_) {
                case TaskState::runnable:
                    ++it;
                    break;

                case TaskState::io_block:
                    --runnable_task_count_;
                    it = slist.erase(it);
                    wait_tasks_.push(tk);
                    break;

                case TaskState::sys_block:
                case TaskState::user_block:
                    {
                        --runnable_task_count_;
                        it = slist.erase(it);
                        std::lock_guard<LFLock> lock(user_wait_lock_);
                        user_wait_tasks_[tk->user_wait_type_][tk->user_wait_id_].push(tk);
                    }
                    break;

                case TaskState::done:
                default:
                    --task_count_;
                    --runnable_task_count_;
                    it = slist.erase(it);
                    DebugPrint(dbg_task, "task(%s) released.", tk->DebugInfo());
                    delete tk;
                    break;
            }
        }
        DebugPrint(dbg_scheduler, "push %u task return to runnable list", (uint32_t)slist.size());
        run_tasks_.push(slist);
    }

    static thread_local epoll_event evs[1024];
    int n = epoll_wait(epoll_fd_, evs, 1024, 1);
    DebugPrint(dbg_scheduler, "do_count=%u, do epoll event, n = %d", do_count, n);
    for (int i = 0; i < n; ++i)
    {
        Task* tk = (Task*)evs[i].data.ptr;
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, tk->wait_fd_, NULL);
        DebugPrint(dbg_ioblock, "task(%s) weak. wait_fd=%d", tk->DebugInfo(), tk->wait_fd_);
        tk->wait_fd_ = -1;
        wait_tasks_.erase(tk);
        AddTask(tk);
    }

    return do_count;
}

void Scheduler::RunLoop()
{
    for (;;) Run();
}

void Scheduler::AddTask(Task* tk)
{
    DebugPrint(dbg_scheduler, "Add task(%s) to runnable list.", tk->DebugInfo());
    run_tasks_.push(tk);
    ++runnable_task_count_;
}

uint32_t Scheduler::TaskCount()
{
    return task_count_;
}

uint32_t Scheduler::RunnableTaskCount()
{
    return runnable_task_count_;
}

uint64_t Scheduler::GetCurrentTaskID()
{
    Task* tk = GetLocalInfo().current_task;
    return tk ? tk->id_ : 0;
}

void Scheduler::SetCurrentTaskDebugInfo(std::string const& info)
{
    Task* tk = GetLocalInfo().current_task;
    if (!tk) return ;
    tk->SetDebugInfo(info);
}

const char* Scheduler::GetCurrentTaskDebugInfo()
{
    Task* tk = GetLocalInfo().current_task;
    return tk ? tk->DebugInfo() : "";
}

bool Scheduler::IOBlockSwitch(int fd, uint32_t event)
{
    // TODO: 支持同一个fd被多个协程等待
    if (!IsCoroutine()) return false;
    Task* tk = GetLocalInfo().current_task;
    epoll_event ev = {event, {(void*)tk}};
    if (-1 == epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev)) {
        fprintf(stderr, "add into epoll error:%d,%s\n", errno, strerror(errno));
        return false;
    }

    tk->wait_fd_ = fd;
    tk->state_ = TaskState::io_block;
    DebugPrint(dbg_ioblock, "task(%s) io_block. wait_fd=%d", tk->DebugInfo(), tk->wait_fd_);
    Yield();
    return true;
}

bool Scheduler::UserBlockSwitch(uint32_t type, uint64_t wait_id, bool yield_immediately)
{
    return SysBlockSwitch((int64_t)type, wait_id, yield_immediately);
}

uint32_t Scheduler::UserBlockWeak(uint32_t type, uint64_t wait_id, uint32_t weak_count)
{
    return SysBlockWeak((int64_t)type, wait_id, weak_count);
}

void Scheduler::UserBlockCancel()
{
    SysBlockCancel();
}

bool Scheduler::SysBlockSwitch(int64_t type, uint64_t wait_id, bool yield_immediately)
{
    if (!IsCoroutine()) return false;
    Task* tk = GetLocalInfo().current_task;
    tk->user_wait_type_ = type;
    tk->user_wait_id_ = wait_id;
    tk->state_ = type < 0 ? TaskState::sys_block : TaskState::user_block;
    DebugPrint(dbg_userblock, "task(%s) %s. wait_type=%lld, wait_id=%llu, yield_immediately=%s",
            tk->DebugInfo(), type < 0 ? "sys_block" : "user_block", (long long int)tk->user_wait_type_, (long long unsigned)tk->user_wait_id_,
            yield_immediately ? "true" : "false");
    if (yield_immediately)
        Yield();
    return true;
}

uint32_t Scheduler::SysBlockWeak(int64_t type, uint64_t wait_id, uint32_t weak_count)
{
    std::unique_lock<LFLock> locker(user_wait_lock_);
    auto it = user_wait_tasks_.find(type);
    if (it == user_wait_tasks_.end()) return 0;

    auto it2 = it->second.find(wait_id);
    if (it2 == it->second.end()) return 0;

    auto& task_queue = it2->second;
    SList<Task> tasks = task_queue.pop(weak_count);
    if (task_queue.empty()) {
        if (it->second.size() > 1) {
            it->second.erase(wait_id);
        } else {
            user_wait_tasks_.erase(type);
        }
    }
    locker.unlock();
    std::size_t c = 0;
    for (auto &task: tasks)
    {
        ++c;
        Task *tk = &task;
        DebugPrint(dbg_userblock, "%s weak task(%s). wait_type=%lld, wait_id=%llu",
                type < 0 ? "sys_block" : "user_block", tk->DebugInfo(), (long long int)type, (long long unsigned)wait_id);
        AddTask(tk);
    }
    DebugPrint(dbg_userblock, "%s weak %u tasks. wait_type=%lld, wait_id=%llu",
            type < 0 ? "sys_block" : "user_block", (unsigned)c, (long long int)type, (long long unsigned)wait_id);
    return c;
}

void Scheduler::SysBlockCancel()
{
    if (!IsCoroutine()) return ;
    Task* tk = GetLocalInfo().current_task;
    if (tk->state_ != TaskState::sys_block && tk->state_ != TaskState::io_block) return ;
    DebugPrint(dbg_userblock, "task(%s) cancel %s. wait_type=%lld, wait_id=%llu",
            tk->DebugInfo(), tk->user_wait_type_ < 0 ? "sys_block" : "user_block",
            (long long int)tk->user_wait_type_, (long long unsigned)tk->user_wait_id_);
    tk->user_wait_type_ = (int64_t)SysBlockType::sysblock_none;
    tk->user_wait_id_ = 0;
    tk->state_ = TaskState::runnable;
}


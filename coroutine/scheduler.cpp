#include "scheduler.h"
#include <ucontext.h>
#include "error.h"
#include <stdio.h>
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
        fprintf(stderr,
                "CoroutineScheduler init failed. epoll create error:%s\n",
                strerror(errno));
        exit(1);
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
    AddTaskRunnable(tk);
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
    if (ret) {
        fprintf(stderr, "swapcontext error:%s\n", strerror(errno));
        ThrowError(eCoErrorCode::ec_yield_failed);
    }
}

uint32_t Scheduler::Run()
{
    uint32_t do_count = DoRunnable();

    // epoll
    DoEpoll();

    // timer
    DoTimer();

    return do_count;
}

// Run函数的一部分, 处理runnable状态的协程
uint32_t Scheduler::DoRunnable()
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
            DebugPrint(dbg_switch, "enter task(%s)", tk->DebugInfo());
            int ret = swapcontext(&info.scheduler, &tk->ctx_);
            if (ret) {
                fprintf(stderr, "swapcontext error:%s\n", strerror(errno));
                run_tasks_.push(slist);
                ThrowError(eCoErrorCode::ec_swapcontext_failed);
            }
            ++do_count;
            DebugPrint(dbg_switch, "leave task(%s) state=%d", tk->DebugInfo(), tk->state_);
            info.current_task = NULL;

            switch (tk->state_) {
                case TaskState::runnable:
                    ++it;
                    break;

                case TaskState::io_block:
                    --runnable_task_count_;
                    it = slist.erase(it);
                    wait_tasks_.push(tk);
                    if (!__IOBlockSwitch(tk)) {
                        if (wait_tasks_.erase(tk))
                            AddTaskRunnable(tk);
                    }
                    break;

                case TaskState::sys_block:
                case TaskState::user_block:
                    {
                        if (tk->block_) {
                            it = slist.erase(it);
                            if (tk->block_->AddWaitTask(tk))
                                --runnable_task_count_;
                            else
                                run_tasks_.push(tk);
                            tk->block_ = NULL;
                        } else {
                            std::unique_lock<LFLock> lock(user_wait_lock_);
                            auto &zone = user_wait_tasks_[tk->user_wait_type_];
                            auto &wait_pair = zone[tk->user_wait_id_];
                            auto &task_queue = wait_pair.second;
                            if (wait_pair.first) {
                                --wait_pair.first;
                                tk->state_ = TaskState::runnable;
                                ++it;
                            } else {
                                --runnable_task_count_;
                                it = slist.erase(it);
                                task_queue.push(tk);
                            }
                            ClearWaitPairWithoutLock(tk->user_wait_type_,
                                    tk->user_wait_id_, zone, wait_pair);
                        }
                    }
                    break;

                case TaskState::done:
                default:
                    --task_count_;
                    --runnable_task_count_;
                    it = slist.erase(it);
                    DebugPrint(dbg_task, "task(%s) released.", tk->DebugInfo());
                    if (tk->eptr_) {
                        std::exception_ptr ep = tk->eptr_;
                        run_tasks_.push(slist);
                        tk->DecrementRef();
                        std::rethrow_exception(ep);
                    } else
                        tk->DecrementRef();
                    break;
            }
        }
        DebugPrint(dbg_scheduler, "push %u task return to runnable list", (uint32_t)slist.size());
        run_tasks_.push(slist);
    }

    return do_count;
}

// Run函数的一部分, 处理epoll相关
void Scheduler::DoEpoll()
{
    std::unique_lock<LFLock> epoll_lock(epoll_lock_, std::defer_lock);
    if (!epoll_lock.try_lock())
        return ;

    static epoll_event evs[1024];
    int n = epoll_wait(epoll_fd_, evs, 1024, 1);
    DebugPrint(dbg_scheduler, "do epoll event, n = %d", n);
    std::list<Task*> cancel_tasks;
    for (int i = 0; i < n; ++i)
    {
        Task* tk = (Task*)evs[i].data.ptr;
        ++tk->wait_successful_;
        // 将tk暂存, 最后再执行__IOBlockCancel, 是为了poll和select可以得到正确的计数。
        // 以防Task被加入runnable列表后，被其他线程执行
        cancel_tasks.push_back(tk);     
    }

    for (auto &tk : cancel_tasks)
        __IOBlockCancel(tk);

    {
        cancel_tasks.clear();
        std::unique_lock<LFLock> lock(io_cancel_tasks_lock_);
        cancel_tasks.swap(io_cancel_tasks_);
    }

    for (auto &tk : cancel_tasks)
        __IOBlockCancel(tk);

    // 由于epoll_wait的结果中会残留一些未计数的Task*,
    //     epoll的性质决定了这些Task无法计数, 
    //     所以这个析构的操作一定要在epoll_lock的保护中做
    Task::DeleteList delete_list;
    Task::SwapDeleteList(delete_list);
    for (auto &tk : delete_list)
        delete tk;
}

// Run函数的一部分, 处理定时器
void Scheduler::DoTimer()
{
    std::vector<std::shared_ptr<CoTimer>> timers;
    timer_mgr_.GetExpired(timers, 128);
    for (auto &sp_timer : timers)
    {
        DebugPrint(dbg_timer, "enter timer callback %llu", (long long unsigned)sp_timer->GetId());
        (*sp_timer)();
        DebugPrint(dbg_timer, "leave timer callback %llu", (long long unsigned)sp_timer->GetId());
    }
}

void Scheduler::RunLoop()
{
    for (;;) Run();
}

void Scheduler::AddTaskRunnable(Task* tk)
{
    DebugPrint(dbg_scheduler, "Add task(%s) to runnable list.", tk->DebugInfo());
    tk->state_ = TaskState::runnable;
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

Task* Scheduler::GetCurrentTask()
{
    return GetLocalInfo().current_task;
}

void Scheduler::IOBlockSwitch(int fd, uint32_t event)
{
    FdStruct fdst[1] = {{fd, event}};
    IOBlockSwitch(fdst);
}

void Scheduler::IOBlockCancel(Task* tk)
{
    std::unique_lock<LFLock> lock(io_cancel_tasks_lock_);
    io_cancel_tasks_.push_back(tk);
}

bool Scheduler::__IOBlockSwitch(Task* tk)
{
    bool ok = false;
    for (auto &fdst : tk->wait_fds_)
    {
        epoll_event ev = {fdst.event, {(void*)tk}};
        if (-1 == epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fdst.fd, &ev)) {
            fprintf(stderr, "add into epoll error:%d,%s\n", errno, strerror(errno));
            // 某个fd添加失败, 回滚
            for (auto &fdst : tk->wait_fds_)
            {
                epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fdst.fd, NULL);
                tk->DecrementRef();
                DebugPrint(dbg_ioblock, "task(%s) delete io_block. fd=%d", tk->DebugInfo(), fdst.fd);
            }
            ok = false;
            break;
        }

        ok = true;
        tk->IncrementRef();     // epoll use ref.
        DebugPrint(dbg_ioblock, "task(%s) io_block. fd=%d, ev=%d",
                tk->DebugInfo(), fdst.fd, fdst.event);
    }

    return ok;
}

void Scheduler::__IOBlockCancel(Task* tk)
{
    if (wait_tasks_.erase(tk)) { // sync between timer and epoll.
        DebugPrint(dbg_ioblock, "task(%s) io_block wakeup.", tk->DebugInfo());

        // 清理所有fd
        for (auto &fdst: tk->wait_fds_)
        {
            if (0 == epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fdst.fd, NULL)) {   // sync 1
                DebugPrint(dbg_ioblock, "task(%s) io_block clear fd=%d", tk->DebugInfo(), fdst.fd);
                // 减引用计数的条件：谁成功从epoll中删除了一个fd，谁才能减引用计数。
                tk->DecrementRef(); // epoll use ref.
            }
        }

        AddTaskRunnable(tk);
    }
}

bool Scheduler::UserBlockWait(uint32_t type, uint64_t wait_id)
{
    return BlockWait((int64_t)type, wait_id);
}

bool Scheduler::TryUserBlockWait(uint32_t type, uint64_t wait_id)
{
    return TryBlockWait((int64_t)type, wait_id);
}

uint32_t Scheduler::UserBlockWakeup(uint32_t type, uint64_t wait_id, uint32_t wakeup_count)
{
    return BlockWakeup((int64_t)type, wait_id, wakeup_count);
}

uint64_t Scheduler::ExpireAt(CoTimerMgr::TimePoint const& time_point,
        CoTimer::fn_t const& fn)
{
    uint64_t id = timer_mgr_.ExpireAt(time_point, fn);
    DebugPrint(dbg_timer, "add timer %llu", (long long unsigned)id);
    return id;
}

bool Scheduler::CancelTimer(uint64_t timer_id)
{
    bool ok = timer_mgr_.Cancel(timer_id);
    DebugPrint(dbg_timer, "cancel timer %llu %s", (long long unsigned)timer_id,
            ok ? "success" : "failed");
    return ok;
}

bool Scheduler::BlockWait(int64_t type, uint64_t wait_id)
{
    if (!IsCoroutine()) return false;
    Task* tk = GetLocalInfo().current_task;
    tk->user_wait_type_ = type;
    tk->user_wait_id_ = wait_id;
    tk->state_ = type < 0 ? TaskState::sys_block : TaskState::user_block;
    DebugPrint(dbg_wait, "task(%s) %s. wait_type=%lld, wait_id=%llu",
            tk->DebugInfo(), type < 0 ? "sys_block" : "user_block",
            (long long int)tk->user_wait_type_, (long long unsigned)tk->user_wait_id_);
    Yield();
    return true;
}

bool Scheduler::TryBlockWait(int64_t type, uint64_t wait_id)
{
    std::unique_lock<LFLock> locker(user_wait_lock_);
    auto it = user_wait_tasks_.find(type);
    if (user_wait_tasks_.end() == it) return false;

    auto &zone = it->second;
    auto it2 = zone.find(wait_id);
    if (zone.end() == it2) return false;

    auto &wait_pair = it2->second;
    if (wait_pair.first > 0) {
        --wait_pair.first;
        ClearWaitPairWithoutLock(type, wait_id, zone, wait_pair);
        return true;
    }

    return false;
}

uint32_t Scheduler::BlockWakeup(int64_t type, uint64_t wait_id, uint32_t wakeup_count)
{
    std::unique_lock<LFLock> locker(user_wait_lock_);
    auto &zone = user_wait_tasks_[type];
    auto &wait_pair = zone[wait_id];
    auto &task_queue = wait_pair.second;
    SList<Task> tasks = task_queue.pop(wakeup_count);
    std::size_t c = tasks.size();
    if (c < wakeup_count) // 允许提前设置唤醒标志, 以便多线程同步。
        wait_pair.first += wakeup_count - c;
    ClearWaitPairWithoutLock(type, wait_id, zone, wait_pair);
    uint32_t domain_wakeup = wait_pair.first;
    locker.unlock();

    for (auto &task: tasks)
    {
        ++c;
        Task *tk = &task;
        DebugPrint(dbg_wait, "%s wakeup task(%s). wait_type=%lld, wait_id=%llu",
                type < 0 ? "sys_block" : "user_block", tk->DebugInfo(), (long long int)type, (long long unsigned)wait_id);
        AddTaskRunnable(tk);
    }

    DebugPrint(dbg_wait, "%s wakeup %u tasks, domain wakeup=%u. wait_type=%lld, wait_id=%llu",
            type < 0 ? "sys_block" : "user_block", (unsigned)c, domain_wakeup, (long long int)type, (long long unsigned)wait_id);
    return c;
}

void Scheduler::ClearWaitPairWithoutLock(int64_t type,
        uint64_t wait_id, WaitZone& zone, WaitPair& wait_pair)
{
    if (wait_pair.second.empty() && wait_pair.first == 0) {
        if (zone.size() > 1) {
            zone.erase(wait_id);
        } else {
            user_wait_tasks_.erase(type);
        }
    }
}


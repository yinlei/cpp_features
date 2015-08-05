#include "io_wait.h"
#include "scheduler.h"
#include <sys/poll.h>

IoWait::IoWait()
{
    epoll_fd_ = epoll_create(1024);
    if (epoll_fd_ == -1) {
        fprintf(stderr,
                "CoroutineScheduler init failed. epoll create error:%s\n",
                strerror(errno));
        exit(1);
    }
}

void IoWait::CoSwitch(std::vector<FdStruct> & fdsts, int timeout_ms)
{
    // TODO: 支持同一个fd被多个协程等待
    Task* tk = g_Scheduler.GetCurrentTask();
    if (!tk) return ;

    uint32_t id = ++tk->io_block_id_;
    tk->state_ = TaskState::io_block;
    tk->wait_fds_.clear();
    tk->wait_successful_ = 0;
    tk->io_block_timeout_ = timeout_ms;
    tk->io_block_timer_.reset();
    for (auto &fdst : fdsts) {
        fdst.epoll_ptr.tk = tk;
        fdst.epoll_ptr.io_block_id = id;
        tk->wait_fds_.push_back(fdst);
    }

    g_Scheduler.Yield();
}

bool IoWait::SchedulerSwitch(Task* tk)
{
    bool ok = false;
    std::unique_lock<LFLock> lock(tk->io_block_lock_, std::defer_lock);
    if (tk->wait_fds_.size() > 1)
        lock.lock();

    wait_tasks_.push(tk);
    for (auto &fdst : tk->wait_fds_)
    {
        epoll_event ev = {fdst.event, {(void*)&fdst.epoll_ptr}};
        tk->IncrementRef();     // 先将引用计数加一, 以防另一个线程立刻epoll_wait成功被执行完线程.
        if (-1 == epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fdst.fd, &ev)) {
            tk->DecrementRef(); // 添加失败时, 回退刚刚增加的引用计数.
            if (errno == EEXIST) {
                fprintf(stderr, "task(%s) add fd(%d) into epoll error %d:%s\n",
                        tk->DebugInfo(), fdst.fd, errno, strerror(errno));
                DebugPrint(dbg_ioblock, "task(%s) add fd(%d) into epoll error %d:%s\n",
                        tk->DebugInfo(), fdst.fd, errno, strerror(errno));
                // 某个fd添加失败, 回滚
                for (auto &fdst : tk->wait_fds_)
                {
                    if (0 == epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fdst.fd, NULL)) {
                        DebugPrint(dbg_ioblock, "task(%s) rollback io_block. fd=%d",
                                tk->DebugInfo(), fdst.fd);
                        // 减引用计数的条件：谁成功从epoll中删除了一个fd，谁才能减引用计数。
                        tk->DecrementRef();
                    }
                }
                ok = false;
                break;
            }

            // 其他原因添加失败, 忽略即可.(模拟poll逻辑)
            continue;
        }

        ok = true;
        DebugPrint(dbg_ioblock, "task(%s) io_block. fd=%d, ev=%d",
                tk->DebugInfo(), fdst.fd, fdst.event);
    }

    if (!ok)
        wait_tasks_.pop();
    else if (tk->io_block_timeout_ != -1) {
        // set timer.
        tk->IncrementRef();
        tk->io_block_timer_ = timer_mgr_.ExpireAt(std::chrono::milliseconds(tk->io_block_timeout_),
                [=]{ 
                    this->Cancel(tk);
                    tk->DecrementRef();
                });
    }

    return ok;
}

void IoWait::Cancel(Task *tk)
{
    if (wait_tasks_.erase(tk)) { // sync between timer and epoll.
        DebugPrint(dbg_ioblock, "task(%s) io_block wakeup.", tk->DebugInfo());

        std::unique_lock<LFLock> lock(tk->io_block_lock_, std::defer_lock);
        if (tk->wait_fds_.size() > 1)
            lock.lock();

        // 清理所有fd
        for (auto &fdst: tk->wait_fds_)
        {
            if (0 == epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fdst.fd, NULL)) {   // sync 1
                DebugPrint(dbg_ioblock, "task(%s) io_block clear fd=%d", tk->DebugInfo(), fdst.fd);
                // 减引用计数的条件：谁成功从epoll中删除了一个fd，谁才能减引用计数。
                tk->DecrementRef(); // epoll use ref.
            }
        }

        g_Scheduler.AddTaskRunnable(tk);
    }
}

int IoWait::WaitLoop()
{
    int c = 0;
    for (;;) {
        std::list<CoTimerPtr> timers;
        timer_mgr_.GetExpired(timers, 128);
        if (timers.empty())
            break;

        c += timers.size();
        // 此处暂存callback而不是Task*，是为了block_cancel能够真实有效。
        std::unique_lock<LFLock> lock(timeout_list_lock_);
        timeout_list_.merge(std::move(timers));
    }

    std::unique_lock<LFLock> lock(epoll_lock_, std::defer_lock);
    if (!lock.try_lock())
        return c ? c : -1;

    static epoll_event evs[1024];
retry:
    int n = epoll_wait(epoll_fd_, evs, 1024, 0);
    if (n == -1 && errno == EAGAIN)
            goto retry;

    DebugPrint(dbg_scheduler, "do epoll event, n = %d", n);
    for (int i = 0; i < n; ++i)
    {
        EpollPtr* ep = (EpollPtr*)evs[i].data.ptr;
        ep->revent = evs[i].events;
        Task* tk = ep->tk;
        ++tk->wait_successful_;
        // 将tk暂存, 最后再执行Cancel, 是为了poll和select可以得到正确的计数。
        // 以防Task被加入runnable列表后，被其他线程执行
        epollwait_tasks_.insert(tk);     
    }

    for (auto &tk : epollwait_tasks_)
        Cancel(tk);

    {
        std::list<CoTimerPtr> timeout_list;
        std::unique_lock<LFLock> lock(timeout_list_lock_);
        timeout_list_.swap(timeout_list);
    }

    for (auto &cb : timeout_list_)
        (*cb)();

    // 由于epoll_wait的结果中会残留一些未计数的Task*,
    //     epoll的性质决定了这些Task无法计数, 
    //     所以这个析构的操作一定要在epoll_lock的保护中做
    Task::DeleteList delete_list;
    Task::SwapDeleteList(delete_list);
    for (auto &tk : delete_list) {
        DebugPrint(dbg_task, "task(%s) delete.", tk->DebugInfo());
        delete tk;
    }

    return n + c;
}


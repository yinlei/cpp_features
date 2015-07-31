// Inline scheduler member functions.

template <typename Fdsts>
bool Scheduler::IOBlockSwitch(Fdsts const& fdsts)
{
    // TODO: 支持同一个fd被多个协程等待
    if (!IsCoroutine()) return false;
    Task* tk = GetLocalInfo().current_task;

    tk->wait_fds_.clear();
    bool ok = false;
    for (auto &fdst : fdsts)
    {
        epoll_event ev = {fdst.event, {(void*)tk}};
        if (-1 == epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fdst.fd, &ev)) {
            fprintf(stderr, "add into epoll error:%d,%s\n", errno, strerror(errno));
            // 回滚
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
        tk->wait_fds_.push_back(fdst);
        tk->state_ = TaskState::io_block;
        tk->IncrementRef();     // epoll use ref.
        DebugPrint(dbg_ioblock, "task(%s) io_block. fd=%d, ev=%d",
                tk->DebugInfo(), fdst.fd, fdst.event);
    }

    if (ok) {
        Yield();
        return true;
    } else {
        return false;
    }
}

template <typename Duration>
uint64_t Scheduler::ExpireAt(Duration const& duration, CoTimer::fn_t const& fn)
{
    return ExpireAt(CoTimerMgr::Now() + duration, fn);
}

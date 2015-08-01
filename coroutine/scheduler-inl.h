// Inline scheduler member functions.

template <typename Fdsts>
void Scheduler::IOBlockSwitch(Fdsts & fdsts)
{
    // TODO: 支持同一个fd被多个协程等待
    if (!IsCoroutine()) return ;
    Task* tk = GetLocalInfo().current_task;
    tk->state_ = TaskState::io_block;
    tk->wait_fds_.clear();
    tk->wait_successful_ = 0;
    for (auto &fdst : fdsts) {
        fdst.epoll_ptr.tk = tk;
        tk->wait_fds_.push_back(fdst);
    }

    Yield();
}

template <typename Duration>
uint64_t Scheduler::ExpireAt(Duration const& duration, CoTimer::fn_t const& fn)
{
    return ExpireAt(CoTimerMgr::Now() + duration, fn);
}

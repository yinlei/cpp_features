// Inline scheduler member functions.

template <typename Duration>
TimerId Scheduler::ExpireAt(Duration const& duration, CoTimer::fn_t const& fn)
{
    return this->ExpireAt(CoTimerMgr::Now() + duration, fn);
}

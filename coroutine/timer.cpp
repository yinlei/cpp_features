#include "timer.h"
#include <mutex>

std::atomic<uint64_t> CoTimer::s_id{0};

CoTimer::CoTimer(fn_t const& fn)
    : id_(++s_id), fn_(fn)
{}

uint64_t CoTimer::GetId()
{
    return id_;
}

void CoTimer::operator()() const
{
    fn_();
}

CoTimerMgr::CoTimerMgr()
{}

uint64_t CoTimerMgr::ExpireAt(TimePoint const& time_point, CoTimer::fn_t const& fn)
{
    std::unique_lock<LFLock> lock(lock_);
    std::shared_ptr<CoTimer> sptr(new CoTimer(fn));
    sptr->next_time_point_ = time_point;
    timers_[sptr->GetId()] = sptr;
    deadlines_.insert(std::make_pair(time_point, sptr));
    return sptr->GetId();
}

bool CoTimerMgr::Cancel(uint64_t timer_id)
{
    std::unique_lock<LFLock> lock(lock_);
    Timers::iterator timers_it = timers_.find(timer_id);
    if (timers_.end() == timers_it) return false;

    std::shared_ptr<CoTimer> sp_timer = timers_it->second;
    timers_.erase(timers_it);

    auto range = deadlines_.equal_range(sp_timer->next_time_point_);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == sp_timer) {
            deadlines_.erase(it);
            break;
        }
    }

    return true;
}

uint32_t CoTimerMgr::GetExpired(std::vector<std::shared_ptr<CoTimer>> &result, uint32_t n)
{
    std::unique_lock<LFLock> lock(lock_);
    TimePoint now = Now();
    auto it = deadlines_.begin();
    for (; it != deadlines_.end() && n > 0; --n, ++it)
    {
        if (it->first > now) {
            break;
        }

        result.push_back(it->second);
        timers_.erase(it->second->GetId());
    }

    deadlines_.erase(deadlines_.begin(), it);
    return result.size();
}

CoTimerMgr::TimePoint CoTimerMgr::Now()
{
    return TimePoint::clock::now();
}


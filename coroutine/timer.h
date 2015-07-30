#include <map>
#include <functional>
#include <chrono>
#include <memory>
#include <vector>
#include "spinlock.h"

class CoTimer
{
public:
    typedef std::function<void()> fn_t;
    typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;

    explicit CoTimer(fn_t const& fn);
    uint64_t GetId();
    void operator()() const;

private:
    uint64_t id_;
    static std::atomic<uint64_t> s_id;
    fn_t fn_;
    TimePoint next_time_point_;

    friend class CoTimerMgr;
};

// 定时器管理
class CoTimerMgr
{
public:
    typedef std::map<uint64_t, std::shared_ptr<CoTimer>> Timers;
    typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;
    typedef std::multimap<TimePoint, std::shared_ptr<CoTimer>> DeadLines;

    CoTimerMgr();

    uint64_t ExpireAt(TimePoint const& time_point, CoTimer::fn_t const& fn);

    template <typename Duration>
    uint64_t ExpireAt(Duration const& duration, CoTimer::fn_t const& fn)
    {
        return ExpireAt(Now() + duration, fn);
    }

    bool Cancel(uint64_t timer_id);

    uint32_t GetExpired(std::vector<std::shared_ptr<CoTimer>> &result, uint32_t n = 1);

    static TimePoint Now();

private:
    Timers timers_;
    DeadLines deadlines_;
    LFLock lock_;
};


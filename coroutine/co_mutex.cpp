#include "co_mutex.h"
#include "scheduler.h"

uint64_t CoMutex::s_id = 0;

CoMutex::CoMutex()
    : id_(++s_id)
{}

void CoMutex::lock()
{
    if (!g_Scheduler.IsCoroutine())
        native_.lock();
    else {
        if (try_lock()) return ;

        g_Scheduler.SysBlockSwitch(
                (int64_t)SysBlockType::sysblock_co_mutex, id_, false);

        if (try_lock()) {
            g_Scheduler.SysBlockCancel();
            return ;
        }

        g_Scheduler.Yield();
    }
}

bool CoMutex::try_lock()
{
    return native_.try_lock();
}

void CoMutex::unlock()
{
    return native_.unlock();
}

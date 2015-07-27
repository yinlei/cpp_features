#include "co_mutex.h"
#include "scheduler.h"
#include <assert.h>

uint64_t CoMutex::s_id = 0;

CoMutex::CoMutex()
    : id_(++s_id)
{
    g_Scheduler.BlockWakeup((int64_t)SysBlockType::sysblock_co_mutex, id_);
}

CoMutex::~CoMutex()
{
    g_Scheduler.BlockWait((int64_t)SysBlockType::sysblock_co_mutex, id_);
}

void CoMutex::lock()
{
    assert(g_Scheduler.IsCoroutine());
    g_Scheduler.BlockWait((int64_t)SysBlockType::sysblock_co_mutex, id_);
}

bool CoMutex::try_lock()
{
    return g_Scheduler.TryBlockWait((int64_t)SysBlockType::sysblock_co_mutex, id_);
}

void CoMutex::unlock()
{
    g_Scheduler.BlockWakeup((int64_t)SysBlockType::sysblock_co_mutex, id_);
}


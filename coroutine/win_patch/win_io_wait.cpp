#include "win_io_wait.h"

namespace co {

IoWait::IoWait() {}

void IoWait::CoSwitch(std::vector<FdStruct> && fdsts, int timeout_ms)
{

}

void IoWait::SchedulerSwitch(Task* tk)
{

}

int IoWait::WaitLoop()
{
    return 0;
}

void IoWait::Cancel(Task *tk, uint32_t id)
{

}

} //namespace co
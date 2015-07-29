#pragma once
#include "ts_queue.h"
#include "task.h"

class BlockObject
{
protected:
    friend class Scheduler;
    uint32_t wakeup_;
    uint32_t max_wakeup_;
    TSQueue<Task, false> wait_queue_;
    LFLock lock_;

public:
    BlockObject();
    explicit BlockObject(uint32_t max_wakeup);
    ~BlockObject();

    void CoBlockWait();

    bool TryBlockWait();

    bool Wakeup();

    bool IsWakeup();

private:
    bool AddWaitTask(Task* tk);
};

#pragma once
#include <memory>
#include <functional>
#include <pthread.h>
#include <thread>
#include "proc.h"

struct CoreThread
{
    ProcPtr proc_;
    std::unique_ptr<std::thread> th_;
    bool exit_;
//    pthread_t th_;

    CoreThread();
    ~CoreThread();

    void RunProc();

    void Yield();

    static thread_local CoreThread* s_core_thread;
};

typedef std::shared_ptr<CoreThread> CoreThreadPtr;

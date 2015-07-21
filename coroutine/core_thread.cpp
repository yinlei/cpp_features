#include "core_thread.h"
#include <unistd.h>
#include <assert.h>
#include <iostream>
#include <exception>

thread_local CoreThread* CoreThread::s_core_thread = NULL;

//static void* ThreadFunc(void* arg)
//{
//    CoreThread* self = (CoreThread*)arg;
//    self->RunProc();
//    return NULL;
//}

CoreThread::CoreThread()
{
    exit_ = false;
    proc_.reset(new Proc);
    th_.reset(new std::thread([this]{this->RunProc();}));
//    pthread_create(&th_, NULL, &ThreadFunc, this);
}

CoreThread::~CoreThread()
{
    exit_ = true;
    th_->join();
//    pthread_join(th_, NULL);
}

void CoreThread::RunProc()
{
    s_core_thread = this;

    while (!exit_) {
        if (!proc_) {
            sleep(1);
            continue;
        }

        proc_->DoSchedule();
    }
}

void CoreThread::Yield()
{
    assert(proc_);
    proc_->Yield();
}

#pragma once
#include "proc.h"
#include "core_thread.h"
#include "task.h"
#include <functional>
#include <vector>
#include <boost/noncopyable.hpp>

class Scheduler : boost::noncopyable
{
    public:
        typedef std::vector<CoreThreadPtr> CoreThreadList;
        CoreThreadList threads_;

//        typedef std::vector<ProcPtr> ProcList;
//        ProcList procs_;

        static Scheduler& getInstance();

        void Init(uint8_t proc_count);

        void CreateTask(std::function<void()> const& fn);

        void Yield();

//        ProcPtr GetProc();
//        ProcPtr PutProc();

    private:
        void NewThread();
        ~Scheduler();
};

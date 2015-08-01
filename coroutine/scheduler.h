#pragma once
#include <ucontext.h>
#include <boost/noncopyable.hpp>
#include <unordered_map>
#include <list>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include "task.h"
#include "block_object.h"
#include "co_mutex.h"
#include "timer.h"

#define DebugPrint(type, fmt, ...) \
    do { \
        if (g_Scheduler.GetOptions().debug & type) { \
            printf("co_dbg ---- " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

///---- debugger flags
static const uint64_t dbg_none = 0;
static const uint64_t dbg_all = 0xffffffffffffffffULL;
static const uint64_t dbg_hook = 0x1;
static const uint64_t dbg_yield = 0x1 << 1;
static const uint64_t dbg_scheduler = 0x1 << 2;
static const uint64_t dbg_task = 0x1 << 3;
static const uint64_t dbg_switch = 0x1 << 4;
static const uint64_t dbg_ioblock = 0x1 << 5;
static const uint64_t dbg_wait = 0x1 << 6;
static const uint64_t dbg_exception = 0x1 << 7;
static const uint64_t dbg_syncblock = 0x1 << 8;
static const uint64_t dbg_timer = 0x1 << 9;
static const uint64_t dbg_sleep = 0x1 << 10;
///-------------------

enum class eCoExHandle : uint8_t
{
    immedaitely_throw,
    delay_rethrow,
    debugger_only,
};

///---- 配置选项
struct CoroutineOptions
{
    uint64_t debug = 0;             // 调试选项, 例如: dbg_switch 或 dbg_hook|dbg_task|dbg_wait
    eCoExHandle exception_handle = eCoExHandle::immedaitely_throw;
    uint32_t stack_size = 128 * 1024; // 协程栈大小, 只会影响在此值设置之后新创建的协程.
    uint32_t chunk_count = 128;     // Run每次最多从run队列中pop出1/chunk_count * task_count个task.
    uint32_t max_chunk_size = 128;  // Run每次最多从run队列中pop出max_chunk_size个task.
};
///-------------------

struct ThreadLocalInfo
{
    Task* current_task;
    ucontext_t scheduler;
};

class Scheduler : boost::noncopyable
{
    public:
        typedef TSQueue<Task> TaskList;  // 线程安全的协程队列
        typedef std::pair<uint32_t, TSQueue<Task, false>> WaitPair;
        typedef std::unordered_map<uint64_t, WaitPair> WaitZone;
        typedef std::unordered_map<int64_t, WaitZone> WaitTable;

        static Scheduler& getInstance();

        // 获取配置选项
        CoroutineOptions& GetOptions();

        // 创建一个协程
        void CreateTask(TaskF const& fn);

        // 当前是否处于协程中
        bool IsCoroutine();

        // 是否没有协程可执行
        bool IsEmpty();

        // 当前协程让出执行权
        void Yield();

        // 调度器调度函数, 内部执行协程、调度协程
        uint32_t Run();

        // 循环Run直到没有协程为止
        void RunUntilNoTask();
        
        // 无限循环执行Run
        void RunLoop();

        // 当前协程总数量
        uint32_t TaskCount();

        // 当前处于可执行状态的协程总数量
        uint32_t RunnableTaskCount();

        // 当前协程ID, ID从1开始（不再协程中则返回0）
        uint64_t GetCurrentTaskID();

        // 当前协程切换的次数
        uint64_t GetCurrentTaskYieldCount();

        // 设置当前协程调试信息, 打印调试信息时将回显
        void SetCurrentTaskDebugInfo(std::string const& info);

        // 获取当前协程的调试信息, 返回的内容包括用户自定义的信息和协程ID
        const char* GetCurrentTaskDebugInfo();

    public:
        /// 调用阻塞式网络IO时, 将当前协程加入等待队列中, socket加入epoll中.
        void IOBlockSwitch(int fd, uint32_t event);

        template <typename Fdsts>
        void IOBlockSwitch(Fdsts & fdsts);

        void IOBlockCancel(Task* tk);

        /// ------------------------------------------------------------------------
        // @{ 以计数的方式模拟实现的协程同步方式. 
        //    初始计数为0, Wait减少计数, Wakeup增加计数.
        //    UserBlockWait将阻塞式（yield）地等待计数大于0, 等待成功后将计数减一,
        //        并将协程切换回可执行状态. 如果不在协程中调用, 则返回false, 且不做任何事.
        //    TryBlockWait检查当前计数, 如果计数等于0, 则返回false; 否则计数减一并返回true.
        //    UserBlockWakeup检查当前等待队列, 将等待队列中的前面最多wakeup_count个
        //        协程唤醒（设置为可执行状态）, 累加剩余计数（wakeup_count减去唤醒的协程数量）
        //
        // 用户自定义的阻塞切换, type范围限定为: [0, 0xffffffff]
        bool UserBlockWait(uint32_t type, uint64_t wait_id);
        bool TryUserBlockWait(uint32_t type, uint64_t wait_id);
        uint32_t UserBlockWakeup(uint32_t type, uint64_t wait_id, uint32_t wakeup_count = 1);
        // }@
        /// ------------------------------------------------------------------------
        
        /// ------------------------------------------------------------------------
        // @{ 定时器
        uint64_t ExpireAt(CoTimerMgr::TimePoint const& time_point, CoTimer::fn_t const& fn);

        template <typename Duration>
        uint64_t ExpireAt(Duration const& duration, CoTimer::fn_t const& fn);

        bool CancelTimer(uint64_t timer_id);
        // }@
        /// ------------------------------------------------------------------------
    
    public:
        Task* GetCurrentTask();

    private:
        Scheduler();
        ~Scheduler();

        // 将一个协程加入可执行队列中
        void AddTaskRunnable(Task* tk);

        // 将一个协程等待的fd全部加入epoll中
        bool __IOBlockSwitch(Task* tk);

        // 取消一个协程的IOBlock
        void __IOBlockCancel(Task* tk);

        /// ------------------------------------------------------------------------
        // 协程框架定义的阻塞切换, type范围不可与用户自定义范围重叠, 指定为:[-xxxxx, -1]
        // 如果不在协程中调用, 则返回false, 且不做任何事.
        bool BlockWait(int64_t type, uint64_t wait_id);

        // 尝试等待某个事件发生, 功能等同于try_lock, 可在协程外调用.
        bool TryBlockWait(int64_t type, uint64_t wait_id);

        // 唤醒对某个时间等待的协程.
        uint32_t BlockWakeup(int64_t type, uint64_t wait_id, uint32_t wakeup_count = 1);
        // }@
        /// ------------------------------------------------------------------------

        // 清理没有等待也没有被等待的WaitPair.
        void ClearWaitPairWithoutLock(int64_t type, uint64_t wait_id, WaitZone& zone, WaitPair& wait_pair);

    private:
        // Run函数的一部分, 处理runnable状态的协程
        uint32_t DoRunnable();

        // Run函数的一部分, 处理epoll相关
        int DoEpoll();

        // Run函数的一部分, 处理定时器
        uint32_t DoTimer();

        // 获取线程局部信息
        ThreadLocalInfo& GetLocalInfo();

        // List of task.
        TaskList run_tasks_;
        TaskList wait_tasks_;

        // User define wait tasks table.
        WaitTable user_wait_tasks_;
        LFLock user_wait_lock_;

        // Timer manager.
        CoTimerMgr timer_mgr_;

        // epoll, io block waiter.
        int epoll_fd_;
        LFLock epoll_lock_;
        LFLock io_cancel_tasks_lock_;
        std::list<Task*> io_cancel_tasks_;

        // total task.
        std::atomic<uint32_t> task_count_;
        std::atomic<uint32_t> runnable_task_count_;

    friend class CoMutex;
    friend class BlockObject;
};

#define g_Scheduler Scheduler::getInstance()

#include "scheduler-inl.h"


#include "scheduler.h"
#include <ucontext.h>
#include <assert.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <system_error>

Scheduler& Scheduler::getInstance()
{
    static Scheduler obj;
    return obj;
}

extern void coroutine_hook_init();
Scheduler::Scheduler()
{
    epoll_fd_ = epoll_create(1024);
    if (epoll_fd_ == -1) {
        perror("CoroutineScheduler init failed. epoll create error:");
        assert(false);
    }

    coroutine_hook_init();
}

Scheduler::~Scheduler()
{
}

ThreadLocalInfo& Scheduler::GetLocalInfo()
{
    static thread_local ThreadLocalInfo info;
    return info;
}

CoroutineOptions& Scheduler::GetOptions()
{
    static CoroutineOptions options;
    return options;
}

void Scheduler::CreateTask(TaskF const& fn)
{
    Task* tk = new Task(fn, GetOptions().stack_size);
    if (tk->state_ == TaskState::fatal) {
        // 创建失败
        delete tk;
        throw std::system_error(errno, std::system_category());
        return ;
    }

    ++task_count_;
    AddTask(tk);
}

bool Scheduler::IsCoroutine()
{
    return !!GetLocalInfo().current_task;
}

bool Scheduler::IsEmpty()
{
    return task_count_ == 0;
}

void Scheduler::Yield()
{
    Task* tk = GetLocalInfo().current_task;
    if (!tk) return ;

    DebugPrint("yield task(%llu) state=%d", tk->id_, tk->state_);
    int ret = swapcontext(&tk->ctx_, &GetLocalInfo().scheduler);
    if (ret) perror("swapcontext error:");
    assert(ret == 0);
}

uint32_t Scheduler::Run()
{
    ThreadLocalInfo& info = GetLocalInfo();
    info.current_task = NULL;
    uint32_t do_max_count = runnable_task_count_;
    uint32_t do_count = 0;

    DebugPrint("Run [max_count=%u]--------------------------", do_max_count);

    // 每次Run执行的协程数量不能多于当前runnable协程数量
    // 以防wait状态的协程得不到执行。
    while (do_count < do_max_count)
    {
        uint32_t cnt = std::max((uint32_t)1, std::min(
                    do_max_count / GetOptions().chunk_count,
                    GetOptions().max_chunk_size));
        DebugPrint("want pop %u tasks.", cnt);
        SList<Task> slist = run_tasks_.pop(cnt);
        DebugPrint("really pop %u tasks.", cnt);
        if (slist.empty()) break;

        SList<Task>::iterator it = slist.begin();
        while (it != slist.end())
        {
            Task* tk = &*it;
            info.current_task = tk;
            DebugPrint("enter task(%llu)", tk->id_);
            int ret = swapcontext(&info.scheduler, &tk->ctx_);
            if (ret) perror("swapcontext error:");
            assert(ret == 0);
            ++do_count;
            DebugPrint("exit task(%llu) state=%d", tk->id_, tk->state_);
            info.current_task = NULL;

            switch (tk->state_) {
                case TaskState::runnable:
                    ++it;
                    break;

                case TaskState::io_block:
                case TaskState::sync_block:
                    --runnable_task_count_;
                    it = slist.erase(it);
                    wait_tasks_.push(tk);
                    break;

                case TaskState::done:
                default:
                    --task_count_;
                    --runnable_task_count_;
                    it = slist.erase(it);
                    delete tk;
                    break;
            }
        }
        DebugPrint("push %d task return to runnable list", slist.size());
        run_tasks_.push(slist);
    }

    static thread_local epoll_event evs[1024];
    int n = epoll_wait(epoll_fd_, evs, 1024, 1);
    DebugPrint("do_count=%u, do epoll event, n = %d", do_count, n);
    for (int i = 0; i < n; ++i)
    {
        Task* tk = (Task*)evs[i].data.ptr;
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, tk->wait_fd_, NULL);
        tk->wait_fd_ = -1;
        wait_tasks_.erase(tk);
        AddTask(tk);
    }

    return do_count;
}

void Scheduler::RunLoop()
{
    for (;;) Run();
}

void Scheduler::AddTask(Task* tk)
{
    DebugPrint("Add task(%llu) to runnable list.", tk->id_);
    run_tasks_.push(tk);
    ++runnable_task_count_;
}

uint32_t Scheduler::TaskCount()
{
    return task_count_;
}

uint32_t Scheduler::RunnableTaskCount()
{
    return runnable_task_count_;
}

uint64_t Scheduler::GetCurrentTaskID()
{
    Task* tk = GetLocalInfo().current_task;
    return tk ? tk->id_ : 0;
}

bool Scheduler::IOBlockSwitch(int fd, uint32_t event)
{
    // TODO: 同一个fd在不同线程同时进行read和write的处理.
    if (!IsCoroutine()) return false;
    Task* tk = GetLocalInfo().current_task;
    epoll_event ev = {event, tk};
    if (-1 == epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev)) {
        fprintf(stderr, "add into epoll error:%d,%s\n", errno, strerror(errno));
        return false;
    }

    tk->wait_fd_ = fd;
    tk->state_ = TaskState::io_block;
    DebugPrint("task(%llu) io_block. wait_fd=%d", tk->id_, tk->wait_fd_);
    Yield();
    return true;
}



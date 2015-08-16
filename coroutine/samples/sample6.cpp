/************************************************
 * coroutine sample6
************************************************
 * 在并行编程中，你经常会使用到一些锁用来同步代码。
 * coroutine目前还没有支持OS原生的锁的自动yield，
 * 不过coroutine提供了一个协程锁co_mutex，可以达到
 * 同样的效果，并且在等待协程锁时可以自动yield，不
 * 浪费性能。
 *
 * co_mutex提供了与标准库的mutex相同的操作接口，可
 * 以轻松搭配标准库提供的lock_guard uniqe_lock等工
 * 具。
 *
 * 和标准库的mutex一样，使用时要注意生命周期的控制，
 * 不要在已经析构的co_mutex上进行操作
************************************************/
#include <mutex>
#include "coroutine.h"


int main()
{
    co_mutex cm;
    int value = 0;
    // 为展示co_mutex自动切换协程的功能，先锁住mutex
    cm.lock();

    go [&]{
        for (int i = 0; i < 3; ++i) {
            std::lock_guard<co_mutex> lock(cm);
            printf("%d\n", value++);
        }
    };

    go [&]{
        cm.unlock();
        for (int i = 0; i < 3; ++i) {
            std::lock_guard<co_mutex> lock(cm);
            printf("%d\n", value++);
        }
    };

    co_sched.RunUntilNoTask();
    return 0;
}


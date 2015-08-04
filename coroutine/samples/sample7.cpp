/************************************************
 * coroutine sample7
************************************************
 * coroutine库原生提供了一个线程安全的定时器
 * 协程内外均可使用.
 * 加入定时器的回调函数会在调度器Run时被触发.
************************************************/
#include <chrono>
#include "coroutine.h"

int main()
{
    bool is_exit = false;

    // co_timer_add接受两个参数
    // 第一个参数可以是std::chrono中的时间长度，也可以是时间点。
    // 第二个参数是定时器回调函数
    // 返回一个uint64_t类型的ID, 通过这个ID可以撤销还未执行的定时函数
    TimerId id1 = co_timer_add(std::chrono::seconds(1), [&]{
            printf("Timer Callback.\n");
            });

    // co_timer_cancel接口可以撤销还未执行的定时函数
    // 它只接受一个参数，就是co_timer_add返回的ID。
    // 它返回bool类型的结果，如果撤销成功，返回true；
    //     如果未来得及撤销，返回false, 此时不保证回调函数已执行完毕。
    //     如果需要保证回调函数不再撤销失败以后被执行, 需要使用co_timer_block_cancel接口
    bool cancelled = co_timer_cancel(id1);
    printf("cancelled:%s\n", cancelled ? "true" : "false");

    co_timer_add(std::chrono::seconds(2), [&]{
            printf("Timer Callback.\n");
            is_exit = true;
            });

    while (!is_exit)
        g_Scheduler.Run();
    return 0;
}


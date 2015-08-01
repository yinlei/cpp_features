/************************************************
 * coroutine sample2
*************************************************/
#include "coroutine.h"
#include <stdio.h>

int main()
{
    // 在协程中使用yield关键字, 可以主动让出调度器执行权限,
    // 让调度器有机会去执行其他协程,
    // 并将当前协程加到可执行协程列表的尾部。
    // 类似于操作系统提供的sleep(0)的功能。
    //
    // 在下面的例子中, 由于yield的存在, 两个协程会交错执行,
    // 输出结果为:
    //   1
    //   3
    //   2
    //   4
    //
    // 注意：1.在协程外使用yield不会有任何效果，也不会出错。
    //       2.如果程序中还需要使用thread库，
    //         头文件包含语句#include "coroutine.h"，要放到
    //         标准库或boost库的thread.hpp的include之后，
    //         以防由于macro的特性导致编译错误。
    //       3.不要忘记yield语句后面的分号";", 如果忘记，也
    //         没有太大关系，编译器一定会不太友好地提醒你。
    //
    go []{
        printf("1\n");
        yield;
        printf("2\n");
    };

    go []{
        printf("3\n");
        yield;
        printf("4\n");
    };

    g_Scheduler.RunUntilNoTask();
    return 0;
}


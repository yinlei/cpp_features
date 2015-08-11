/***********************************************
 * coroutine sample4
************************************************
 * coroutine支持异常安全, 对于协程中的抛出未捕获
 * 的异常提供以下几种处理方式:
 *   1.立即在协程栈上抛出异常, 此举会导致进程直接崩溃, 但是可以生成带有堆栈的coredump
 *     设置方法：
 *       g_Scheduler.GetOptions().exception_handle = co::eCoExHandle::immedaitely_throw;
 *   2.结束当前协程, 使其堆栈回滚, 将异常暂存至调度器Run时抛出.
 *     设置方法：
 *       g_Scheduler.GetOptions().exception_handle = co::eCoExHandle::delay_rethrow;
 *   3.结束当前协程, 吃掉异常, 仅打印一些日志信息.
 *     设置方法：
 *       g_Scheduler.GetOptions().exception_handle = co::eCoExHandle::debugger_only;
 *
 * 显示日志信息需要打开exception相关的调试信息:
 *       g_Scheduler.GetOptions().debug |= dbg_exception;
 *
 * 日志信息默认显示在标准输出上，允许用户重定向
************************************************/
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "coroutine.h"

int main()
{
    g_Scheduler.GetOptions().exception_handle = co::eCoExHandle::delay_rethrow;
    go []{ throw 1; };
    try {
        g_Scheduler.RunUntilNoTask();
    } catch (int v) {
        printf("caught delay throw exception:%d\n", v);
    }

    g_Scheduler.GetOptions().debug |= co::dbg_exception;
    g_Scheduler.GetOptions().exception_handle = co::eCoExHandle::debugger_only;
    go []{
        // 为了使打印的日志信息更加容易辨识，还可以给当前协程附加一些调试信息。
        g_Scheduler.SetCurrentTaskDebugInfo("throw_ex");

        // 重定向日志信息输出至文件
        g_Scheduler.GetOptions().debug_output = fopen("log", "a+");

        throw std::exception();
    };
    g_Scheduler.RunUntilNoTask();
    return 0;
}


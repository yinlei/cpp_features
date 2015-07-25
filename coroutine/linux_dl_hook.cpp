#include <dlfcn.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "scheduler.h"

typedef ssize_t(*write_t)(int, const void *, size_t);
static write_t write_f = (write_t)dlsym(RTLD_NEXT, "write");

ssize_t write(int fd, const void *buf, size_t count)
{
    DebugPrint("hook write. %s coroutine.", g_Scheduler.IsCoroutine() ? "In" : "Not in");

    if (!g_Scheduler.IsCoroutine()) {
        return write_f(fd, buf, count);
    } else {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags & O_NONBLOCK)
            return write_f(fd, buf, count);

        if (-1 == fcntl(fd, F_SETFL, flags | O_NONBLOCK))
            return write_f(fd, buf, count);

        // add into epoll, and switch other context.
        if (!g_Scheduler.IOBlockSwitch(fd, EPOLLOUT)) {
            fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
            return write_f(fd, buf, count);
        }
        
        DebugPrint("continue task(%llu) write. fd=%d", g_Scheduler.GetCurrentTaskID(), fd);
        ssize_t s = write_f(fd, buf, count);
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        return s;
    }
}


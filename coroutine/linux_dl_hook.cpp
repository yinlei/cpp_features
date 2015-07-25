#include <dlfcn.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "scheduler.h"

typedef int(*connect_t)(int, const struct sockaddr *, socklen_t);
static connect_t connect_f = (connect_t)dlsym(RTLD_NEXT, "connect");

typedef ssize_t(*read_t)(int, void *, size_t);
static read_t read_f = (read_t)dlsym(RTLD_NEXT, "read");

typedef ssize_t(*readv_t)(int, const struct iovec *, int);
static readv_t readv_f = (readv_t)dlsym(RTLD_NEXT, "readv");

typedef ssize_t(*write_t)(int, const void *, size_t);
static write_t write_f = (write_t)dlsym(RTLD_NEXT, "write");

typedef ssize_t(*writev_t)(int, const struct iovec *, int);
static writev_t writev_f = (writev_t)dlsym(RTLD_NEXT, "writev");

extern "C" {

int connect(int fd, const struct sockaddr *addr, socklen_t addrlen)
{
    DebugPrint("hook connect. %s coroutine.", g_Scheduler.IsCoroutine() ? "In" : "Not in");

    if (!g_Scheduler.IsCoroutine()) {
        return connect_f(fd, addr, addrlen);
    } else {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags & O_NONBLOCK)
            return connect_f(fd, addr, addrlen);

        if (-1 == fcntl(fd, F_SETFL, flags | O_NONBLOCK))
            return connect_f(fd, addr, addrlen);

        int n = connect_f(fd, addr, addrlen);
        int e = errno;
        if (n == 0) {
            fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
            DebugPrint("continue task(%llu) connect completed immediately. fd=%d",
                    g_Scheduler.GetCurrentTaskID(), fd);
            return 0;
        } else if (n != -1 || errno != EINPROGRESS) {
            fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
            errno = e;
            return n;
        } else if (!g_Scheduler.IOBlockSwitch(fd, EPOLLOUT)) {
            // add into epoll, and switch other context.
            fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
            errno = e;
            return n;
        }
        
        DebugPrint("continue task(%llu) connect. fd=%d", g_Scheduler.GetCurrentTaskID(), fd);
        int error = 0;  
        socklen_t len = sizeof(int);  
        if (0 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
            if (0 == error) {
                fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
                DebugPrint("continue task(%llu) connect success async. fd=%d",
                        g_Scheduler.GetCurrentTaskID(), fd);
                return 0;
            } else {
                fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
                errno = error;
                return -1;
            }
        }

        e = errno;      // errno set by getsockopt.
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        errno = e;
        return -1;
    }
}

ssize_t read(int fd, void *buf, size_t count)
{
    DebugPrint("hook read. %s coroutine.", g_Scheduler.IsCoroutine() ? "In" : "Not in");

    if (!g_Scheduler.IsCoroutine()) {
        return read_f(fd, buf, count);
    } else {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags & O_NONBLOCK)
            return read_f(fd, buf, count);

        if (-1 == fcntl(fd, F_SETFL, flags | O_NONBLOCK))
            return read_f(fd, buf, count);

        // add into epoll, and switch other context.
        if (!g_Scheduler.IOBlockSwitch(fd, EPOLLIN)) {
            fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
            return read_f(fd, buf, count);
        }
        
        DebugPrint("continue task(%llu) read. fd=%d", g_Scheduler.GetCurrentTaskID(), fd);
        ssize_t s = read_f(fd, buf, count);
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        return s;
    }
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    DebugPrint("hook readv. %s coroutine.", g_Scheduler.IsCoroutine() ? "In" : "Not in");

    if (!g_Scheduler.IsCoroutine()) {
        return readv_f(fd, iov, iovcnt);
    } else {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags & O_NONBLOCK)
            return readv_f(fd, iov, iovcnt);

        if (-1 == fcntl(fd, F_SETFL, flags | O_NONBLOCK))
            return readv_f(fd, iov, iovcnt);

        // add into epoll, and switch other context.
        if (!g_Scheduler.IOBlockSwitch(fd, EPOLLIN)) {
            fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
            return readv_f(fd, iov, iovcnt);
        }
        
        DebugPrint("continue task(%llu) readv. fd=%d", g_Scheduler.GetCurrentTaskID(), fd);
        ssize_t s = readv_f(fd, iov, iovcnt);
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        return s;
    }
}

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

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    DebugPrint("hook writev. %s coroutine.", g_Scheduler.IsCoroutine() ? "In" : "Not in");

    if (!g_Scheduler.IsCoroutine()) {
        return writev_f(fd, iov, iovcnt);
    } else {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags & O_NONBLOCK)
            return writev_f(fd, iov, iovcnt);

        if (-1 == fcntl(fd, F_SETFL, flags | O_NONBLOCK))
            return writev_f(fd, iov, iovcnt);

        // add into epoll, and switch other context.
        if (!g_Scheduler.IOBlockSwitch(fd, EPOLLOUT)) {
            fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
            return writev_f(fd, iov, iovcnt);
        }
        
        DebugPrint("continue task(%llu) writev. fd=%d", g_Scheduler.GetCurrentTaskID(), fd);
        ssize_t s = writev_f(fd, iov, iovcnt);
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        return s;
    }
}

}

void coroutine_hook_init() {}

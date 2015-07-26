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

template <typename OriginF, typename ... Args>
ssize_t read_write_mode(int fd, OriginF fn, const char* hook_fn_name, uint32_t event, Args && ... args)
{
    DebugPrint("hook %s. %s coroutine.", hook_fn_name, g_Scheduler.IsCoroutine() ? "In" : "Not in");

    if (!g_Scheduler.IsCoroutine()) {
        return fn(fd, std::forward<Args>(args)...);
    } else {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags & O_NONBLOCK)
            return fn(fd, std::forward<Args>(args)...);

        if (-1 == fcntl(fd, F_SETFL, flags | O_NONBLOCK))
            return fn(fd, std::forward<Args>(args)...);

        ssize_t n = fn(fd, std::forward<Args>(args)...);
        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // add into epoll, and switch other context.
            if (!g_Scheduler.IOBlockSwitch(fd, event)) {
                fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
                return fn(fd, std::forward<Args>(args)...);
            }
            
            DebugPrint("continue task(%llu) %s. fd=%d", g_Scheduler.GetCurrentTaskID(), hook_fn_name, fd);
            n = fn(fd, std::forward<Args>(args)...);
        }

        int e = errno;
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        errno = e;
        return n;
    }
}

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
    return read_write_mode(fd, read_f, "read", EPOLLIN, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    return read_write_mode(fd, readv_f, "readv", EPOLLIN, iov, iovcnt);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return read_write_mode(fd, write_f, "write", EPOLLOUT, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    return read_write_mode(fd, writev_f, "writev", EPOLLOUT, iov, iovcnt);
}

}

void coroutine_hook_init() {}

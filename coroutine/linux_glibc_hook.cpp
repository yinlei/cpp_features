#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "scheduler.h"
#include <assert.h>

template <typename OriginF, typename ... Args>
ssize_t read_write_mode(int fd, OriginF fn, const char* hook_fn_name, uint32_t event, int timeout_so, Args && ... args)
{
    DebugPrint(dbg_hook, "hook %s. %s coroutine.", hook_fn_name, g_Scheduler.IsCoroutine() ? "In" : "Not in");

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
            // get timeout option.
            Task* tk = g_Scheduler.GetCurrentTask();
            uint64_t timer_id = 0;
            struct timeval timeout;
            socklen_t timeout_blen = sizeof(timeout);
            if (0 == getsockopt(fd, SOL_SOCKET, timeout_so, &timeout, &timeout_blen)) {
                if (timeout.tv_sec > 0 || timeout.tv_usec > 0) {
                    std::chrono::microseconds duration{timeout.tv_sec * 1000 * 1000 +
                        timeout.tv_usec};
                    tk->IncrementRef(); // timer use ref.
                    timer_id = g_Scheduler.ExpireAt(duration, [=]{
                            g_Scheduler.IOBlockCancel(tk);
                            tk->DecrementRef(); // timer use ref.
                            });
                }
            }

            // add into epoll, and switch other context.
            g_Scheduler.IOBlockSwitch(fd, event);
            if (timer_id && g_Scheduler.CancelTimer(timer_id))
                tk->DecrementRef(); // timer use ref.

            if (tk->wait_successful_ == 0) {
                fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
                return fn(fd, std::forward<Args>(args)...);
            }

            DebugPrint(dbg_hook, "continue task(%s) %s. fd=%d", g_Scheduler.GetCurrentTaskDebugInfo(), hook_fn_name, fd);
            n = fn(fd, std::forward<Args>(args)...);
        } else {
            DebugPrint(dbg_hook, "task(%s) syscall(%s) completed immediately. fd=%d",
                    g_Scheduler.GetCurrentTaskDebugInfo(), hook_fn_name, fd);
        }

        int e = errno;
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        errno = e;
        return n;
    }
}

extern "C" {

typedef int(*connect_t)(int, const struct sockaddr *, socklen_t);
static connect_t connect_f = NULL;

typedef ssize_t(*read_t)(int, void *, size_t);
static read_t read_f = NULL;

typedef ssize_t(*readv_t)(int, const struct iovec *, int);
static readv_t readv_f = NULL;

typedef ssize_t(*write_t)(int, const void *, size_t);
static write_t write_f = NULL;

typedef ssize_t(*writev_t)(int, const struct iovec *, int);
static writev_t writev_f = NULL;

typedef int(*poll_t)(struct pollfd *fds, nfds_t nfds, int timeout);
static poll_t poll_f = NULL;

int connect(int fd, const struct sockaddr *addr, socklen_t addrlen)
{
    DebugPrint(dbg_hook, "hook connect. %s coroutine.", g_Scheduler.IsCoroutine() ? "In" : "Not in");

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
            DebugPrint(dbg_hook, "continue task(%s) connect completed immediately. fd=%d",
                    g_Scheduler.GetCurrentTaskDebugInfo(), fd);
            return 0;
        } else if (n != -1 || errno != EINPROGRESS) {
            fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
            errno = e;
            return n;
        } else {
            // add into epoll, and switch other context.
            g_Scheduler.IOBlockSwitch(fd, EPOLLOUT);
        }

        Task* tk = g_Scheduler.GetCurrentTask();
        if (tk->wait_successful_ == 0) {
            // 添加到epoll中失败了
            fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
            errno = e;
            return n;
        }

        DebugPrint(dbg_hook, "continue task(%s) connect. fd=%d", g_Scheduler.GetCurrentTaskDebugInfo(), fd);
        int error = 0;
        socklen_t len = sizeof(int);
        if (0 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
            if (0 == error) {
                fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
                DebugPrint(dbg_hook, "continue task(%s) connect success async. fd=%d",
                        g_Scheduler.GetCurrentTaskDebugInfo(), fd);
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
    return read_write_mode(fd, read_f, "read", EPOLLIN, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    return read_write_mode(fd, readv_f, "readv", EPOLLIN, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return read_write_mode(fd, write_f, "write", EPOLLOUT, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    return read_write_mode(fd, writev_f, "writev", EPOLLOUT, SO_SNDTIMEO, iov, iovcnt);
}

static uint32_t PollEvent2Epoll(short events)
{
    uint32_t e = 0;
    if (events & POLLIN)   e |= EPOLLIN;
    if (events & POLLOUT)  e |= EPOLLOUT;
    if (events & POLLHUP)  e |= EPOLLHUP;
    if (events & POLLERR)  e |= EPOLLERR;
    return e;
}

static short EpollEvent2Poll(uint32_t events)
{
    short e = 0;
    if (events & EPOLLIN)  e |= POLLIN;
    if (events & EPOLLOUT) e |= POLLOUT;
    if (events & EPOLLHUP) e |= POLLHUP;
    if (events & EPOLLERR) e |= POLLERR;
    return e;
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    DebugPrint(dbg_hook, "hook poll. %s coroutine.", g_Scheduler.IsCoroutine() ? "In" : "Not in");

    if (!g_Scheduler.IsCoroutine())
        return poll_f(fds, nfds, timeout);

    if (timeout == 0)
        return poll_f(fds, nfds, timeout);

    Task* tk = g_Scheduler.GetCurrentTask();
    uint32_t timer_id = 0;
    if (timeout != -1) {
        std::chrono::milliseconds duration{timeout};
        tk->IncrementRef(); // timer use ref.
        timer_id = g_Scheduler.ExpireAt(duration, [=]{
                g_Scheduler.IOBlockCancel(tk);
                tk->DecrementRef(); // timer use ref.
                });
    }

    std::vector<FdStruct> fdsts;
    for (nfds_t i = 0; i < nfds; ++i) {
        fdsts.emplace_back();
        fdsts.back().fd = fds[i].fd;
        fdsts.back().event = PollEvent2Epoll(fds[i].events);
    }

    // add into epoll, and switch other context.
    g_Scheduler.IOBlockSwitch(fdsts);
    bool is_timeout = false; // 是否超时
    if (timer_id) {
        is_timeout = true;
        if (g_Scheduler.CancelTimer(timer_id)) {
            tk->DecrementRef(); // timer use ref.
            is_timeout = false;
        }
    }

    if (tk->wait_successful_ == 0) {
        if (is_timeout)
            return 0;
        else
            return poll_f(fds, nfds, 0);
    }

    int n = 0;
    for (int i = 0; i < (int)tk->wait_fds_.size(); ++i)
    {
        fds[i].revents = EpollEvent2Poll(tk->wait_fds_[i].epoll_ptr.revent);
        if (fds[i].revents) ++n;
    }

    assert(n == (int)tk->wait_successful_);

    return n;
}

#if !defined(CO_DYNAMIC_LINK)
extern int __connect(int fd, const struct sockaddr *addr, socklen_t addrlen);
extern ssize_t __read(int fd, void *buf, size_t count);
extern ssize_t __readv(int fd, const struct iovec *iov, int iovcnt);
extern ssize_t __write(int fd, const void *buf, size_t count);
extern ssize_t __writev(int fd, const struct iovec *iov, int iovcnt);
extern int __poll(struct pollfd *fds, nfds_t nfds, int timeout);
#endif
}

void coroutine_hook_init()
{
#if defined(CO_DYNAMIC_LINK)
    connect_f = (connect_t)dlsym(RTLD_NEXT, "connect");
    read_f = (read_t)dlsym(RTLD_NEXT, "read");
    readv_f = (readv_t)dlsym(RTLD_NEXT, "readv");
    write_f = (write_t)dlsym(RTLD_NEXT, "write");
    writev_f = (writev_t)dlsym(RTLD_NEXT, "writev");
    poll_f = (poll_t)dlsym(RTLD_NEXT, "poll");
#else
    connect_f = &__connect;
    read_f = &__read;
    readv_f = &__readv;
    write_f = &__write;
    writev_f = &__writev;
    poll_f = &__poll;
#endif

    if (!connect_f || !read_f || !write_f || !readv_f || !writev_f) {
        fprintf(stderr, "Hook syscall failed. Please don't remove libc.a when static-link.\n");
        exit(1);
    }
}


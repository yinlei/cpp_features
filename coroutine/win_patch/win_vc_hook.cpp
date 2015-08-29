#include <scheduler.h>
#include <mhook.h>

namespace co {

    typedef int (*ioctlsocket_t)(
        _In_    SOCKET s,
        _In_    long   cmd,
        _Inout_ u_long *argp
        );
    static ioctlsocket_t ioctlsocket_f = (ioctlsocket_t)GetProcAddress(GetModuleHandle(L"Ws2_32.dll"), "ioctlsocket");

    static int hook_ioctlsocket(
        _In_    SOCKET s,
        _In_    long   cmd,
        _Inout_ u_long *argp
        )
    {
        int ret = ioctlsocket_f(s, cmd, argp);
        if (ret == 0 && cmd == FIONBIO) {
            int v = *argp;
            setsockopt(s, SOL_SOCKET, SO_GROUP_PRIORITY, (const char*)&v, sizeof(v));
        }
        return ret;
    }

    bool SetNonblocking(SOCKET s, bool is_nonblocking)
    {
        u_long v = is_nonblocking ? 1 : 0;
        return ioctlsocket(s, FIONBIO, &v) == 0;
    }

    bool IsNonblocking(SOCKET s)
    {
        int v = 0;
        int vlen = sizeof(v);
        if (0 != getsockopt(s, SOL_SOCKET, SO_GROUP_PRIORITY, (char*)&v, &vlen)) {
            if (WSAENOTSOCK == WSAGetLastError())
                return true;
        }
        return !!v;
    }

    typedef int (*select_t)(
        _In_    int                  nfds,
        _Inout_ fd_set               *readfds,
        _Inout_ fd_set               *writefds,
        _Inout_ fd_set               *exceptfds,
        _In_    const struct timeval *timeout
        );
    static select_t select_f = (select_t)GetProcAddress(GetModuleHandle(L"Ws2_32.dll"), "select");

    static inline int safe_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds)
    {
        //Task *tk = g_Scheduler.GetCurrentTask();
        //DebugPrint(dbg_hook, "task(%s) safe_select(nfds=%d, rfds=%p, wfds=%p, efds=%p).",
        //    tk ? tk->DebugInfo() : "nil", (int)nfds, readfds, writefds, exceptfds);
        static const struct timeval zero_tmv { 0, 0 };
        fd_set *rfds = NULL, *wfds = NULL, *efds = NULL;
        fd_set fds[3];
        if (readfds) {
            fds[0] = *readfds;
            rfds = &fds[0];
        }
        if (writefds) {
            fds[1] = *writefds;
            wfds = &fds[1];
        }
        if (exceptfds) {
            fds[2] = *exceptfds;
            efds = &fds[2];
        }
        int ret = select_f(nfds, rfds, wfds, efds, &zero_tmv);
        if (ret <= 0) return ret;

        if (readfds) *readfds = fds[0];
        if (writefds) *writefds = fds[1];
        if (exceptfds) *exceptfds = fds[2];
        return ret;
    }

    static int hook_select(
        _In_    int                  nfds,
        _Inout_ fd_set               *readfds,
        _Inout_ fd_set               *writefds,
        _Inout_ fd_set               *exceptfds,
        _In_    const struct timeval *timeout
        )
    {
        static const struct timeval zero_tmv{0, 0};
        int timeout_ms = timeout ? (timeout->tv_sec * 1000 + timeout->tv_usec / 1000) : -1;
        Task *tk = g_Scheduler.GetCurrentTask();
        DebugPrint(dbg_hook, "task(%s) Hook select(nfds=%d, rfds=%p, wfds=%p, efds=%p, timeout=%d).", 
            tk ? tk->DebugInfo() : "nil", (int)nfds, readfds, writefds, exceptfds, timeout_ms);

        if (!tk || !timeout_ms)
            return select_f(nfds, readfds, writefds, exceptfds, timeout);

        // async select
        int ret = safe_select(nfds, readfds, writefds, exceptfds);
        if (ret) return ret;
        
        ULONGLONG start_time = GetTickCount64();
        int delta_time = 1;
        while (-1 == timeout_ms || GetTickCount64() - start_time < timeout_ms)
        {
            ret = safe_select(nfds, readfds, writefds, exceptfds);
            if (ret > 0) return ret;

            if (exceptfds) {
                // 因为windows的select, 仅在事先监听时才能捕获到error, 因此此处需要手动check
                fd_set e_result;
                FD_ZERO(&e_result);
                for (u_int i = 0; i < exceptfds->fd_count; ++i)
                {
                    SOCKET fd = exceptfds->fd_array[i];
                    int err = 0;
                    int errlen = sizeof(err);
                    getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
                    if (err) {
                        FD_SET(fd, &e_result);
                    }
                }

                if (e_result.fd_count > 0) {
                    // Some errors were happened.
                    if (readfds) FD_ZERO(readfds);
                    if (writefds) FD_ZERO(writefds);
                    *exceptfds = e_result;
                    return e_result.fd_count;
                }
            }

            g_Scheduler.SleepSwitch(delta_time);
            if (delta_time < 16)
                delta_time <<= 2;
        }

        if (readfds) FD_ZERO(readfds);
        if (writefds) FD_ZERO(writefds);
        if (exceptfds) FD_ZERO(exceptfds);
        return 0;
    }

    typedef int (*connect_t)(
        _In_ SOCKET                s,
        _In_ const struct sockaddr *name,
        _In_ int                   namelen
        );
    static connect_t connect_f = (connect_t)GetProcAddress(GetModuleHandle(L"Ws2_32.dll"), "connect");

    static int hook_connect(
        _In_ SOCKET                s,
        _In_ const struct sockaddr *name,
        _In_ int                   namelen
        )
    {
        Task *tk = g_Scheduler.GetCurrentTask();
        bool is_nonblocking = IsNonblocking(s);
        DebugPrint(dbg_hook, "task(%s) Hook connect(s=%d)(nonblocking:%d).", tk ? tk->DebugInfo() : "nil", (int)s, (int)is_nonblocking);
        if (!tk || is_nonblocking)
            return connect_f(s, name, namelen);

        // async connect
        if (!SetNonblocking(s, true))
            return connect_f(s, name, namelen);
        
        int ret = connect_f(s, name, namelen);
        if (ret == 0) return 0;

        int err = WSAGetLastError();
        if (WSAEWOULDBLOCK != err && WSAEINPROGRESS != err)
            return ret;

        fd_set wfds = {}, efds = {};
        FD_ZERO(&wfds);
        FD_ZERO(&efds);
        FD_SET(s, &wfds);
        FD_SET(s, &efds);
        select(1, NULL, &wfds, &efds, NULL);
        if (!FD_ISSET(s, &efds) && FD_ISSET(s, &wfds)) {
            SetNonblocking(s, false);
            return 0;
        }

        err = 0;
        int errlen = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
        if (err) {
            SetNonblocking(s, false);
            WSASetLastError(err);
            return SOCKET_ERROR;
        }

        SetNonblocking(s, false);
        return 0;
    }

    enum e_mode_hook_flags
    {
        e_overlapped = 0x1,
        e_no_timeout = 0x1 << 1,
    };

    template <typename R, typename OriginF, typename ... Args>
    static R read_mode_hook(OriginF fn, const char* fn_name, int flags, SOCKET s, Args && ... args)
    {
        Task *tk = g_Scheduler.GetCurrentTask();
        bool is_nonblocking = IsNonblocking(s);
        DebugPrint(dbg_hook, "task(%s) Hook %s(s=%d)(nonblocking:%d)(flags:%d).",
            tk ? tk->DebugInfo() : "nil", fn_name, (int)s, (int)is_nonblocking, (int)flags);
        if (!tk || is_nonblocking || (flags & e_overlapped))
            return fn(s, std::forward<Args>(args)...);

        // async WSARecv
        if (!SetNonblocking(s, true))
            return fn(s, std::forward<Args>(args)...);

        R ret = fn(s, std::forward<Args>(args)...);
        if (ret != -1 && ret >= 0) {
            SetNonblocking(s, false);
            return ret;
        }

        // If connection is closed, the Bytes will setted 0, and ret is 0, and WSAGetLastError() returns 0.
        int err = WSAGetLastError();
        if (WSAEWOULDBLOCK != err && WSAEINPROGRESS != err) {
            SetNonblocking(s, false);
            WSASetLastError(err);
            return ret;
        }

        // wait data arrives.
        int timeout = 0;
        if (!(flags & e_no_timeout)) {
            int timeoutlen = sizeof(timeout);
            getsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, &timeoutlen);
        }

        timeval tm{ timeout / 1000, timeout % 1000 * 1000 };
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(s, &rfds);
        select(1, &rfds, NULL, NULL, timeout ? &tm : NULL);

        ret = fn(s, std::forward<Args>(args)...);
        err = WSAGetLastError();
        SetNonblocking(s, false);
        WSASetLastError(err);
        return ret;
    }

    typedef SOCKET (*accept_t)(
        _In_    SOCKET          s,
        _Out_   struct sockaddr *addr,
        _Inout_ int             *addrlen
        );
    static accept_t accept_f = (accept_t)GetProcAddress(GetModuleHandle(L"Ws2_32.dll"), "accept");

    static SOCKET hook_accept(
        _In_    SOCKET          s,
        _Out_   struct sockaddr *addr,
        _Inout_ int             *addrlen
        )
    {
        return read_mode_hook<SOCKET>(accept_f, "accept", e_no_timeout, s, addr, addrlen);
    }

    typedef int (*WSARecv_t)(
        _In_    SOCKET                             s,
        _Inout_ LPWSABUF                           lpBuffers,
        _In_    DWORD                              dwBufferCount,
        _Out_   LPDWORD                            lpNumberOfBytesRecvd,
        _Inout_ LPDWORD                            lpFlags,
        _In_    LPWSAOVERLAPPED                    lpOverlapped,
        _In_    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
        );
    static WSARecv_t WSARecv_f = (WSARecv_t)GetProcAddress(GetModuleHandle(L"Ws2_32.dll"), "WSARecv");

    static int hook_WSARecv(
        _In_    SOCKET                             s,
        _Inout_ LPWSABUF                           lpBuffers,
        _In_    DWORD                              dwBufferCount,
        _Out_   LPDWORD                            lpNumberOfBytesRecvd,
        _Inout_ LPDWORD                            lpFlags,
        _In_    LPWSAOVERLAPPED                    lpOverlapped,
        _In_    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
        )
    {
        return read_mode_hook<int>(WSARecv_f, "WSARecv", lpOverlapped ? e_overlapped : 0, s,
            lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
    }

    typedef int (*WSASend_t)(
        _In_  SOCKET                             s,
        _In_  LPWSABUF                           lpBuffers,
        _In_  DWORD                              dwBufferCount,
        _Out_ LPDWORD                            lpNumberOfBytesSent,
        _In_  DWORD                              dwFlags,
        _In_  LPWSAOVERLAPPED                    lpOverlapped,
        _In_  LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
        );
    static WSASend_t WSASend_f = (WSASend_t)GetProcAddress(GetModuleHandle(L"Ws2_32.dll"), "WSASend");

    static int hook_WSASend(
        _In_  SOCKET                             s,
        _In_  LPWSABUF                           lpBuffers,
        _In_  DWORD                              dwBufferCount,
        _Out_ LPDWORD                            lpNumberOfBytesSent,
        _In_  DWORD                              dwFlags,
        _In_  LPWSAOVERLAPPED                    lpOverlapped,
        _In_  LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
        )
    {
        Task *tk = g_Scheduler.GetCurrentTask();
        bool is_nonblocking = IsNonblocking(s);
        DebugPrint(dbg_hook, "task(%s) Hook WSARecv(s=%d, lpOverlapped=%p)(nonblocking:%d).",
            tk ? tk->DebugInfo() : "nil", (int)s, lpOverlapped, (int)is_nonblocking);
        if (!tk || is_nonblocking || lpOverlapped)
            return WSASend_f(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);

        // async WSARecv
        if (!SetNonblocking(s, true))
            return WSASend_f(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);

        int ret = WSASend_f(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
        if (0 == ret) {
            SetNonblocking(s, false);
            return 0;
        }

        // If connection is closed, the Bytes will setted 0, and ret is 0, and WSAGetLastError() returns 0.
        int err = WSAGetLastError();
        if (WSAEWOULDBLOCK != err) {
            SetNonblocking(s, false);
            WSASetLastError(err);
            return ret;
        }

        // wait data arrives.
        int timeout = 0;
        int timeoutlen = sizeof(timeout);
        getsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, &timeoutlen);

        timeval tm{ timeout / 1000, timeout % 1000 * 1000 };
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(s, &wfds);
        select(1, NULL, &wfds, NULL, timeout ? &tm : NULL);

        ret = WSASend_f(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
        err = WSAGetLastError();
        SetNonblocking(s, false);
        WSASetLastError(err);
        return ret;
    }

    void coroutine_hook_init()
    {
        BOOL ok = true;
        ok &= Mhook_SetHook((PVOID*)&connect_f, &hook_connect);
        ok &= Mhook_SetHook((PVOID*)&ioctlsocket_f, &hook_ioctlsocket);
        ok &= Mhook_SetHook((PVOID*)&select_f, &hook_select);
        ok &= Mhook_SetHook((PVOID*)&WSARecv_f, &hook_WSARecv);
        ok &= Mhook_SetHook((PVOID*)&WSASend_f, &hook_WSASend);
        ok &= Mhook_SetHook((PVOID*)&accept_f, &hook_accept);
        if (!ok) {
            fprintf(stderr, "Hook failed!");
            exit(1);
        }
    }

} //namespace co
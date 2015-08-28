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

    static int hook_select(
        _In_    int                  nfds,
        _Inout_ fd_set               *readfds,
        _Inout_ fd_set               *writefds,
        _Inout_ fd_set               *exceptfds,
        _In_    const struct timeval *timeout
        )
    {
        static const struct timeval zero_tmv{0, 0};
        int ret = select_f(nfds, readfds, writefds, exceptfds, &zero_tmv);
        if (ret) return ret;

        int timeout_ms = timeout ? (timeout->tv_sec * 1000 + timeout->tv_usec / 1000) : -1;
        ULONGLONG start_time = GetTickCount64();
        int delta_time = 1;
        while (-1 == timeout_ms || GetTickCount64() - start_time < timeout_ms)
        {
            ret = select_f(nfds, readfds, writefds, exceptfds, &zero_tmv);
            if (ret > 0) return ret;
            g_Scheduler.SleepSwitch(delta_time);
            if (delta_time < 16)
                delta_time <<= 2;
        }

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

        fd_set wfds[1] = {}, efds[1] = {};
        FD_ZERO(wfds);
        FD_ZERO(efds);
        FD_SET(s, wfds);
        FD_SET(s, efds);
        static const struct timeval zero_tmv { 0, 0 };
        int delta_time = 8;
        for (;;) 
        {
            ret = select_f(1, NULL, wfds, efds, &zero_tmv);
            if (ret >= 1) {
                if (!FD_ISSET(s, efds) && FD_ISSET(s, wfds))
                    return 0;
            }

            int err = 0;
            int errlen = sizeof(err);
            getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
            if (err) {
                WSASetLastError(err);
                return SOCKET_ERROR;
            }

            g_Scheduler.SleepSwitch(delta_time);
            if (delta_time < 64)
                delta_time <<= 2;
        }

        return 0;
    }

    void coroutine_hook_init()
    {
        BOOL ok = true;
        ok &= Mhook_SetHook((PVOID*)&connect_f, &hook_connect);
        ok &= Mhook_SetHook((PVOID*)&ioctlsocket_f, &hook_ioctlsocket);
        ok &= Mhook_SetHook((PVOID*)&select_f, &hook_select);
        if (!ok) {
            fprintf(stderr, "Hook failed!");
            exit(1);
        }
    }

} //namespace co
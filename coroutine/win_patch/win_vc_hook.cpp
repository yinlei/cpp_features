#include <WinSock2.h>
#include <Windows.h>
#include <scheduler.h>
#include <mhook.h>

namespace co {

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
        DebugPrint(dbg_hook, "task(%s) Hook connect(s=%d).", tk ? tk->DebugInfo() : "nil", (int)s);
        return connect_f(s, name, namelen);
    }

    void coroutine_hook_init()
    {
        BOOL ok = true;
        ok &= Mhook_SetHook((PVOID*)&connect_f, &hook_connect);
        if (!ok) {
            fprintf(stderr, "Hook failed!");
            exit(1);
        }
    }

} //namespace co
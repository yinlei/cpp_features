#pragma once
#include <WinSock2.h>
#include <Windows.h>
#include <stdint.h>

// VS2013²»Ö§³Öthread_local
#if defined(_MSC_VER) && _MSC_VER < 1900
#define thread_local __declspec(thread)
#endif

inline void usleep(uint64_t microseconds)
{
    ::Sleep((uint32_t)(microseconds / 1000));
}

inline unsigned int sleep(unsigned int seconds)
{
    ::Sleep(seconds * 1000);
    return seconds;
}
#pragma once
#include <WinSock2.h>
#include <Windows.h>
#include <stdint.h>

inline void usleep(uint64_t microseconds)
{
    ::Sleep((uint32_t)(microseconds / 1000));
}
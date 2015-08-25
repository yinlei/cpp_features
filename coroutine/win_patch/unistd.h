#pragma once
#include <boost/asio.hpp>

inline void usleep(uint64_t microseconds)
{
    ::Sleep((uint32_t)(microseconds / 1000));
}
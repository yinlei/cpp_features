#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <boost/thread.hpp>
#include <gtest/gtest.h>
#include "coroutine.h"
using namespace std;

void foo()
{
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == socketfd) {
        perror("socket init error:");
        return ;
    }

    struct timeval rcvtimeout = {5, 0};
    if (-1 == setsockopt(socketfd, SOL_SOCKET,
                SO_RCVTIMEO, &rcvtimeout, sizeof(rcvtimeout)))
    {
        perror("setsockopt error:");
        return ;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(22);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (-1 == connect(socketfd, (sockaddr*)&addr, sizeof(addr))) {
        perror("connect error:");
        return ;
    }

    char buf[1024] = {};
    for (int i = 0; i < 2; ++i)
    {
        ssize_t n = read(socketfd, buf, sizeof(buf));
        if (n == 0) {
            printf("disconnected.\n");
        } else if (n < 0) {
            if (errno == EAGAIN) {
                printf("read error is EAGAIN.\n");
            } else {
                perror("read error:");
            }
        } else {
            printf("read %d bytes.\n", (int)n);
        }
    }
}

TEST(IOTimed, Main)
{
    g_Scheduler.GetOptions().debug = dbg_all;
    go foo;
    cout << "go" << endl;
    while (!g_Scheduler.IsEmpty())
        g_Scheduler.Run();
    cout << "end" << endl;
}


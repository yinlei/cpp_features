#include "coroutine.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using namespace std;

void foo()
{
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == socketfd) {
        perror("socket init error:");
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

    char buf[12] = "12\n";
    ssize_t n = write(socketfd, buf, 3);
    printf("writen %d bytes.\n", (int)n);
}

int main()
{
    g_Scheduler.GetOptions().debug = dbg_all;
    go foo;
    cout << "go" << endl;
    while (!g_Scheduler.IsEmpty())
        g_Scheduler.Run();
    cout << "end" << endl;
    return 0;
}


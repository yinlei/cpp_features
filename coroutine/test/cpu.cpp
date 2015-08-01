/************************************************
 * cpu.test
*************************************************/
#include <boost/thread.hpp>
#include "coroutine.h"
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>

static const uint16_t port = 43333;

void echo_server()
{
    int accept_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    socklen_t len = sizeof(addr);
    if (-1 == bind(accept_fd, (sockaddr*)&addr, len)) {
        fprintf(stderr, "bind error, please change the port value.\n");
        exit(1);
    }
    if (-1 == listen(accept_fd, 5)) {
        fprintf(stderr, "listen error.\n");
        exit(1);
    }

retry:
    // 阻塞的accept已被HOOK，等待期间切换执行其他协程。
    int sockfd = accept(accept_fd, (sockaddr*)&addr, &len);
    if (sockfd == -1) {
        if (EAGAIN == errno || EINTR == errno)
            goto retry;

        fprintf(stderr, "accept error:%s\n", strerror(errno));
        return ;
    }

    char buf[1024];
retry_read:
    // 阻塞的read已被HOOK，等待期间切换执行其他协程。
    int n = read(sockfd, buf, sizeof(buf));
    if (n == -1) {
        if (EAGAIN == errno || EINTR == errno)
            goto retry_read;

        fprintf(stderr, "read error:%s\n", strerror(errno));
    } else if (n == 0) {
        fprintf(stderr, "read eof\n");
    } else {
        // echo
        // 阻塞的write已被HOOK，等待期间切换执行其他协程。
        write(sockfd, buf, n);
    }
}

void client()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    // 阻塞的connect已被HOOK，等待期间切换执行其他协程。
    if (-1 == connect(sockfd, (sockaddr*)&addr, sizeof(addr))) {
        fprintf(stderr, "connect error:%s\n", strerror(errno));
        exit(1);
    }

    char buf[12] = "1234";
    int len = strlen(buf) + 1;
    // 阻塞的write已被HOOK，等待期间切换执行其他协程。
    write(sockfd, buf, len);
    printf("send [%d] %s\n", len, buf);

    char rcv_buf[12];
retry_read:
    // 阻塞的read已被HOOK，等待期间切换执行其他协程。
    int n = read(sockfd, rcv_buf, sizeof(rcv_buf));
    if (n == -1) {
        if (EAGAIN == errno || EINTR == errno)
            goto retry_read;

        fprintf(stderr, "read error:%s\n", strerror(errno));
    } else if (n == 0) {
        fprintf(stderr, "read eof\n");
    } else {
        // echo
        printf("recv [%d] %s\n", n, rcv_buf);
    }
}

int main(int argc, char** argv)
{
    int thread_count = 1;
    if (argc > 1) {
        thread_count = atoi(argv[1]);
    }
    printf("thread_count=%d\n", thread_count);

//    g_Scheduler.GetOptions().debug = dbg_sleep;

    go echo_server;
//    go client;

    // 单线程执行
    boost::thread_group tg;
    for (int i = 0; i < thread_count; ++i)
        tg.create_thread([] { g_Scheduler.RunUntilNoTask();} );
    tg.join_all();
    return 0;
}


#pragma once
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <boost/thread.hpp>
#include <assert.h>

#define assert_false(expr) assert(!(expr))

struct TestServer
{
    int accept_fd_;
    int epoll_fd_;
    bool is_work_;
    boost::thread work_thread_;

    TestServer() : is_work_(true)
    {
//        rlimit of = {RLIM_INFINITY, RLIM_INFINITY};
        rlimit of = {4096, 4096};
        if (-1 == setrlimit(RLIMIT_NOFILE, &of)) {
            fprintf(stderr, "setrlimit error %d:%s\n", errno, strerror(errno));
            exit(1);
        }

        accept_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        assert_false(accept_fd_ == -1);

        int reuse = 1;
        int n = setsockopt(accept_fd_, SOL_SOCKET, SO_REUSEPORT, (void*)&reuse, sizeof(reuse));
        assert_false(n == -1);

        int flags = fcntl(accept_fd_, F_GETFL);
        flags |= O_NONBLOCK;
        n = fcntl(accept_fd_, F_SETFL, flags);
        assert_false(n == -1);

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(43222);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        n = bind(accept_fd_, (sockaddr*)&addr, sizeof(addr));
        assert_false(n == -1);

        n = listen(accept_fd_, 1024);
        assert_false(n == -1);

        epoll_fd_ = epoll_create(1024);
        assert_false(epoll_fd_ == -1);

        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = accept_fd_;
        n = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, accept_fd_, &ev);
        assert_false(n == -1);

        boost::thread thr(&TestServer::DoWork, this);
        work_thread_.swap(thr);
    }

    ~TestServer()
    {
        is_work_ = false;
        work_thread_.join();
    }

    void Accept()
    {
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int sock = accept(accept_fd_, (sockaddr*)&addr, &len);
        if (sock == -1) return ;

        int flags = fcntl(sock, F_GETFL);
        flags |= O_NONBLOCK;
        int n = fcntl(sock, F_SETFL, flags);
        assert_false(n == -1);

        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = sock;
        n = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sock, &ev);
        assert_false(n == -1);
    }

    void Read(int sock)
    {
        char buf[1024];
retry:
        int n = read(sock, buf, sizeof(buf));
        if (n == -1) {
            if (errno == EINTR)
                goto retry;

            if (errno == EAGAIN)
                return ;

//            fprintf(stderr, "read error %d:%s\n", errno, strerror(errno));
            Close(sock);
        } else if (n == 0) {
//            fprintf(stderr, "read eof\n");
            Close(sock);
        } else {
            write(sock, buf, n);
        }
    }

    void Close(int sock)
    {
//        fprintf(stderr, "Close socket %d\n", sock);
        close(sock);
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, sock, NULL);
    }

    void DoWork()
    {
        epoll_event evs[1024];
        while (is_work_) {
retry:
            int n = epoll_wait(epoll_fd_, evs, 1024, 10);
            if (n == -1) {
                if (errno == EINTR)
                    goto retry;

                assert(false);
            }

            for (int i = 0; i < n; ++i) {
                epoll_event &ev = evs[i];
                if (ev.data.fd == accept_fd_) {
                    Accept();
                    continue;
                }

                int sock = ev.data.fd;
                if (ev.events & EPOLLERR) {
                    fprintf(stderr, "events is ERR\n");
                    Close(sock);
                    continue;
                }
                
                if (ev.events & EPOLLIN) {
                    Read(sock);
                }
            }
        }
    }
};
static TestServer s_server;

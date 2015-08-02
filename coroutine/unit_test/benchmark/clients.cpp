#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <sys/resource.h>
#include "coroutine.h"

using namespace boost::asio;
using namespace boost::asio::ip;
using boost::system::error_code;

// 由于socket的析构要依赖于io_service, 所以注意控制
// io_service的生命期要长于socket
io_service ios;
tcp::endpoint addr(address::from_string("127.0.0.1"), 43333);

std::atomic<long long unsigned> g_sendbytes{0}, g_recvbytes{0};
std::atomic<int> session_count{0};

void client()
{
    tcp::socket s(ios);
    error_code ec;
    s.connect(addr, ec);
    if (!ec) {
        ++session_count;
        printf("connected(%d).\n", (int)session_count);
        int buflen = 4096;
        char *buf = new char[buflen];
        std::unique_ptr<char[]> _ep(buf);
        for (;;) {
            if (ec) {
                --session_count;
                printf("disconnected(%d).\n", (int)session_count);
                break;
            }

            auto n = s.write_some(buffer(buf, buflen), ec);
            if (ec) continue;
            g_sendbytes += n;

            std::size_t rn = s.receive(buffer(buf, n), 0, ec);
            if (ec) continue;
            g_recvbytes += rn;
        }
    }

    // 断线以后, 创建新的协程去连接.
    go client;
}

void show_status()
{
    static long long unsigned last_sendbytes = 0, last_recvbytes = 0;
    printf("session:%d, send_speed:%llu KB/s, recv_speed:%llu KB/s\n",
            (int)session_count, (g_sendbytes - last_sendbytes) / 1024, (g_recvbytes - last_recvbytes) / 1024);
    last_sendbytes = g_sendbytes;
    last_recvbytes = g_recvbytes;
    co_timer_add(std::chrono::seconds(1), show_status);
}

int main(int argc, char **argv)
{
    int thread_count = 1;
    if (argc > 1)
        thread_count = atoi(argv[1]);

    rlimit of = {8192, 8192};
    if (-1 == setrlimit(RLIMIT_NOFILE, &of)) {
        perror("setrlimit");
        exit(1);
    }

    for (int i = 0; i < 1024; ++i)
        go client;

    co_timer_add(std::chrono::seconds(1), show_status);

    boost::thread_group tg;
    for (int i = 0; i < thread_count; ++i)
        tg.create_thread([] { g_Scheduler.RunUntilNoTask(); });
    tg.join_all();
    return 0;
}


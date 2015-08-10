#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <sys/resource.h>
#include "coroutine.h"

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std::chrono;
using boost::system::error_code;

// 由于socket的析构要依赖于io_service, 所以注意控制
// io_service的生命期要长于socket
io_service ios;
tcp::endpoint addr(address::from_string("127.0.0.1"), 43333);

std::atomic<long long unsigned> g_sendbytes{0}, g_recvbytes{0}, g_qps{0};
std::atomic<int> session_count{0};
uint32_t qdata = 4096;
int thread_count = 1;

void client()
{
    std::shared_ptr<tcp::socket> s(new tcp::socket(ios));
    error_code ec;
    s->connect(addr, ec);
    if (!ec) {
        ++session_count;
        co_chan<bool> err;
        go [err, s] {
            char *buf = new char[qdata];
            std::unique_ptr<char[]> _ep(buf);
            error_code ec;
            int noyield_for_c = 0;
            while (!ec) {
                ++noyield_for_c;
                auto n = s->write_some(buffer(buf, qdata), ec);
                if (ec) break;
                if ((noyield_for_c & 0x7f) == 0)
                    yield;
                g_sendbytes += n;
            }
            err << true;
        };

        go [err, s] {
            int buflen = 4096;
            char *buf = new char[buflen];
            std::unique_ptr<char[]> _ep(buf);
            error_code ec;
            int noyield_for_c = 0;
            while (!ec) {
                ++noyield_for_c;
                std::size_t rn = s->read_some(buffer(buf, buflen), ec);
                if (ec) break;
                if ((noyield_for_c & 0x7f) == 0)
                    yield;
                g_recvbytes += rn;
            }
            err << true;
        };

        err >> nullptr;
        --session_count;
    }

    // 断线以后, 创建新的协程去连接.
    go client;
}

void show_status()
{
    static int show_title = 0;
    static long long unsigned last_sendbytes = 0, last_recvbytes = 0;
    static auto last_time = system_clock::now();
    auto now = system_clock::now();
    if (show_title++ % 10 == 0) {
        printf("thread:%d, qdata:%d\n", thread_count, qdata);
        printf("  conn   send(KB)   recv(KB)   qps   time_delta(ms)\n");
    }
    printf("%6d %9llu %9llu %7d %6d\n",
            (int)session_count, (g_sendbytes - last_sendbytes) / 1024, (g_recvbytes - last_recvbytes) / 1024,
            (int)(double)(g_recvbytes - last_recvbytes) / qdata,
            (int)duration_cast<milliseconds>(now - last_time).count());
    last_time = now;
    last_sendbytes = g_sendbytes;
    last_recvbytes = g_recvbytes;
    co_timer_add(seconds(1), show_status);
}

int main(int argc, char **argv)
{
    int conn_count = 1024;
    if (argc > 1)
        thread_count = atoi(argv[1]);
    if (argc > 2)
        conn_count = atoi(argv[2]);
    if (argc > 3)
        qdata = atoi(argv[3]);

    rlimit of = {8192, 8192};
    if (-1 == setrlimit(RLIMIT_NOFILE, &of)) {
        perror("setrlimit");
        exit(1);
    }

    for (int i = 0; i < conn_count; ++i)
        go client;

    co_timer_add(seconds(1), show_status);

    boost::thread_group tg;
    for (int i = 0; i < thread_count; ++i)
        tg.create_thread([] { g_Scheduler.RunUntilNoTask(); });
    tg.join_all();
    return 0;
}


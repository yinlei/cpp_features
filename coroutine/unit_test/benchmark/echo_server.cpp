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

void echo_server()
{
    tcp::acceptor acc(ios, addr, true);
    printf("startup server, listen %s:%d\n", acc.local_endpoint().address().to_string().c_str(),
            acc.local_endpoint().port());
    std::atomic<int> session_count{0};
    for (;;) {
        std::shared_ptr<tcp::socket> s(new tcp::socket(ios));
        acc.accept(*s);
        go [&session_count, s]{
            int buflen = 4096;
            char *buf = new char[buflen];
            std::unique_ptr<char[]> _ep(buf);
            error_code ec;
            tcp::endpoint addr = s->remote_endpoint();
            ++session_count;
            printf("connected(%d). %s:%d\n", (int)session_count, addr.address().to_string().c_str(), addr.port());
            for (;;) {
                if (ec) {
                    --session_count;
                    printf("disconnected(%d). %s:%d\n", (int)session_count, addr.address().to_string().c_str(), addr.port());
                    return ;
                }

                auto n = s->read_some(buffer(buf, buflen), ec);
                if (ec) continue;

                n = s->write_some(buffer(buf, n), ec);
                if (ec) continue;
            }
        };
    }
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

    go echo_server;

    boost::thread_group tg;
    for (int i = 0; i < thread_count; ++i)
        tg.create_thread([] { g_Scheduler.RunUntilNoTask(); });
    tg.join_all();
    return 0;
}


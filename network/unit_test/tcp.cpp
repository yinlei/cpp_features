#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <gtest/gtest.h>
#include <coroutine/coroutine.h>
#include "network.h"
using namespace std;
using namespace co;
using namespace network;

TEST(Network, TcpServer)
{
    co_sched.GetOptions().debug = network::dbg_accept_error | network::dbg_accept_debug | network::dbg_session_alive;

    {
        tcp::server server;
        server.SetConnectedCb([](tcp::sess_id_t id){
            printf("connected.\n");
        }).SetDisconnectedCb([](tcp::sess_id_t id, boost_ec const& ec) {
            printf("disconnected. reason %d:%s\n", ec.value(), ec.message().c_str());
        }).SetReceiveCb([&](tcp::sess_id_t id, const char* data, size_t bytes){
            printf("receive: %.*s\n", (int)bytes, data);
            tcp::Send(id, data, bytes);
            if (strstr(std::string(data, bytes).c_str(), "shutdown")) {
                tcp::Shutdown(id);
            }
        });
        boost_ec ec = server.goStart("0.0.0.0", 3030);
        if (ec) {
            printf("server start error %d:%s\n", ec.value(), ec.message().c_str());
            EXPECT_TRUE(false);
        }

        bool bexit = false;
        go [&] {
            tcp::client c;
            int *ping_c = new int(0);   // stack object, will restored problem.
            boost_ec ec = c.SetSndTimeout(10)
             .SetConnectedCb([](tcp::sess_id_t id){
                tcp::Send(id, "ping", 4);
             })
             .SetReceiveCb([ping_c](tcp::sess_id_t id, const char* data, size_t bytes){
                if (++*ping_c < 3) {
                    tcp::Send(id, "ping", 4);
                }
                else
                    tcp::Send(id, "shutdown", 8);
             })
             .Connect("127.0.0.1", 3030);
            EXPECT_TRUE(!ec);

            for (;;)
            {
                if (IsEstab(c.GetSessId()))
                    co_yield;
                else {
                    bexit = true;
                    break;
                }
            }
        };

        while (!bexit)
            co_sched.Run();
    }

    co_sched.RunUntilNoTask();
}


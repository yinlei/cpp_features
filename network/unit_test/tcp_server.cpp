#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <gtest/gtest.h>
#include <coroutine/coroutine.h>
#include "transport.h"
using namespace std;
using namespace co;
using namespace network;

TEST(Network, TcpServer)
{
    TcpServer server;
    server.SetConnectedCb([](SessionId id){
        printf("connected.\n");
    }).SetDisconnectedCb([](SessionId id, boost_ec const& ec) {
        printf("disconnected. reason %d:%s\n", ec.value(), ec.message().c_str());
    }).SetReceiveCb([&](SessionId id, const char* data, size_t bytes, boost_ec& ec){
        printf("receive: %.*s\n", (int)bytes, data);
        server.Send(id, data, bytes);
        if (strstr(std::string(data, bytes).c_str(), "shutdown")) {
            server.Shutdown(id);
        }
    });
    boost_ec ec = server.goStart("0.0.0.0", 3030);
    if (ec) {
        printf("server start error %d:%s\n", ec.value(), ec.message().c_str());
//        EXCEPT_TRUE(false);
    }

    co_sched.RunUntilNoTask();
}

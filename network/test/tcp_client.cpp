#include <iostream>
#include <unistd.h>
#include <coroutine/coroutine.h>
#include "network.h"
using namespace std;
using namespace co;
using namespace network;

const char* g_ip = NULL;
uint16_t g_port = 0;

void on_disconnect(SessionId id, boost_ec const& ec)
{
    printf("disconnected. reason %d:%s\n", ec.value(), ec.message().c_str());
}

void foo()
{
    TcpClient client;
    client.SetConnectedCb([](SessionId id){
        printf("connected.\n");
    }).SetDisconnectedCb(&on_disconnect).SetReceiveCb([&](SessionId id, const char* data, size_t bytes, boost_ec& ec){
        printf("receive: %.*s\n", (int)bytes, data);
        client.Send(data, bytes);
        if (strstr(std::string(data, bytes).c_str(), "shutdown")) {
            client.Shutdown();
        }
    });
    boost_ec ec = client.Connect(g_ip, g_port);
    if (ec) {
        printf("connect error %d:%s\n", ec.value(), ec.message().c_str());
    } else {
        printf("connect to %s:%d\n", client.RemoteAddr().address().to_string().c_str(),
                client.RemoteAddr().port());
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: %s ip port\n", argv[0]);
        exit(1);
    }

    co_sched.GetOptions().debug = network::dbg_session_alive;

    g_ip = argv[1];
    g_port = atoi(argv[2]);

    go foo;
    co_sched.RunUntilNoTask();
    return 0;
}


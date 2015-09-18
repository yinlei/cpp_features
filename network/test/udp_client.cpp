#include <iostream>
#include <unistd.h>
#include <string.h>
#include <coroutine/coroutine.h>
#include "network.h"
using namespace std;
using namespace co;
using namespace network;

const char* g_ip = "127.0.0.1";
uint16_t g_port = 3030;

void foo()
{
    udp::client *client = new udp::client;
    client->SetReceiveCb([client](udp::sess_id_t id, const char* data, size_t bytes){
        printf("receive: %.*s\n", (int)bytes, data);
        delete client;
    });
    client->goStart();
    boost_ec ec = client->Connect(g_ip, g_port);
    if (ec) {
        printf("connect error %d:%s\n", ec.value(), ec.message().c_str());
    } else {
        printf("connect to %s:%d\n", client->RemoteAddr().address().to_string().c_str(),
                client->RemoteAddr().port());
    }
    char buf[] = "test udp network!";
    client->Send(buf, sizeof(buf));
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s ip port\n\n", argv[0]);
        exit(1);
    }

    if (argc >= 2)
        g_ip = argv[1];
    if (argc >= 3)
        g_port = atoi(argv[2]);

    go foo;
    co_sched.RunUntilNoTask();
    return 0;
}


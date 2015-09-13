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

void on_disconnect(tcp::sess_id_t id, boost_ec const& ec)
{
    printf("disconnected. reason %d:%s\n", ec.value(), ec.message().c_str());
}

void foo()
{
    tcp::client client;
    client.SetConnectedCb([](tcp::sess_id_t id){
        printf("connected.\n");
        go [id] {
            for (;;)
            {
                if (tcp::IsEstab(id))
                    tcp::Send(id, "ping", 4, [](tcp::e_snd_status s){
                            printf("send returns %d\n", (int)s);
                        });
                else
                    return ;

                ::sleep(3);
            }
        };
    }).SetDisconnectedCb(&on_disconnect).SetReceiveCb([](tcp::sess_id_t id, const char* data, size_t bytes, boost_ec& ec){
        printf("receive: %.*s\n", (int)bytes, data);
    });
    boost_ec ec = client.Connect(g_ip, g_port);
    if (ec) {
        printf("connect error %d:%s\n", ec.value(), ec.message().c_str());
    } else {
        printf("connect to %s:%d\n", tcp::RemoteAddr(client.GetSessId()).address().to_string().c_str(),
                tcp::RemoteAddr(client.GetSessId()).port());
    }

    for (;;)
    {
        if (tcp::IsEstab(client.GetSessId()))
            co_yield;
        else
            client.Connect(g_ip, g_port);
    }
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s ip port\n\n", argv[0]);
        exit(1);
    }

    co_sched.GetOptions().debug = network::dbg_session_alive;

    if (argc >= 2)
        g_ip = argv[1];
    if (argc >= 3)
        g_port = atoi(argv[2]);

    go foo;
    co_sched.RunUntilNoTask();
    return 0;
}


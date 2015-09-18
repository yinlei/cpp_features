#include <iostream>
#include <unistd.h>
#include <coroutine/coroutine.h>
#include "network.h"
using namespace std;
using namespace co;
using namespace network;

int main()
{
    co_sched.GetOptions().debug = network::dbg_session_alive;

    udp::server server;
    server.SetReceiveCb([&](udp::sess_id_t id, const char* data, size_t bytes){
        printf("receive: %.*s\n", (int)bytes, data);
        udp::Send(id, data, bytes);
    });
    boost_ec ec = server.goStart("0.0.0.0", 3030);
    if (ec) {
        printf("server start error %d:%s\n", ec.value(), ec.message().c_str());
    } else {
        printf("server start at %s:%d\n", server.LocalAddr().address().to_string().c_str(),
                server.LocalAddr().port());
    }

    co_sched.RunUntilNoTask();
}

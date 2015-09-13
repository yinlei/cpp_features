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

    tcp::server server;
    server.SetConnectedCb([](tcp::sess_id_t id){
        printf("connected from %s:%d\n", tcp::RemoteAddr(id).address().to_string().c_str(), tcp::RemoteAddr(id).port());
    }).SetDisconnectedCb([](tcp::sess_id_t id, boost_ec const& ec) {
        printf("disconnected. reason %d:%s\n", ec.value(), ec.message().c_str());
    }).SetReceiveCb([&](tcp::sess_id_t id, const char* data, size_t bytes, boost_ec& ec){
        printf("receive: %.*s\n", (int)bytes, data);
        tcp::Send(id, data, bytes);
        if (strstr(std::string(data, bytes).c_str(), "shutdown")) {
            tcp::Shutdown(id);
        }
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

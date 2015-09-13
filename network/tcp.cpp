#include "tcp.h"

namespace network {


void tcp::Send(sess_id_t id, Buffer && buf, SndCb cb)
{
    tcp_detail::Send(id, std::move(buf), cb);
}
void tcp::Send(sess_id_t id, const void* data, size_t bytes, SndCb cb)
{
    tcp_detail::Send(id, data, bytes, cb);
}
void tcp::Shutdown(sess_id_t id)
{
    tcp_detail::Shutdown(id);
}
bool tcp::IsEstab(sess_id_t id)
{
    return tcp_detail::IsEstab(id);
}
tcp::endpoint tcp::LocalAddr(sess_id_t id)
{
    return tcp_detail::LocalAddr(id);
}
tcp::endpoint tcp::RemoteAddr(sess_id_t id)
{
    return tcp_detail::RemoteAddr(id);
}

}//namespace network

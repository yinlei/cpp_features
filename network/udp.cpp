#include "udp.h"

namespace network {

    boost_ec udp::Send(sess_id_t id, Buffer && buf)
    {
        return id.udp_point->Send(id.remote_addr, &buf[0], buf.size());
    }
    boost_ec udp::Send(sess_id_t id, const void* data, size_t bytes)
    {
        return id.udp_point->Send(id.remote_addr, data, bytes);
    }
    udp::endpoint udp::LocalAddr(sess_id_t id)
    {
        return id.udp_point->LocalAddr();
    }
    udp::endpoint udp::RemoteAddr(sess_id_t id)
    {
        return id.udp_point->RemoteAddr();
    }

} //namespace network

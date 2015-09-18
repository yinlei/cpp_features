#pragma once
#include "udp_detail.h"

namespace network {

class udp
{
public:
    typedef udp_detail::SessionId sess_id_t;
    typedef udp_detail::UdpServer server;
    typedef udp_detail::UdpClient client;
    typedef boost::asio::ip::udp::endpoint endpoint;
    typedef udp_detail::Buffer Buffer;

    static boost_ec Send(sess_id_t id, Buffer && buf);
    static boost_ec Send(sess_id_t id, const void* data, size_t bytes);
    static endpoint LocalAddr(sess_id_t id);
    static endpoint RemoteAddr(sess_id_t id);
};

}//namespace network

#pragma once
#include "tcp_detail.h"

namespace network {

class tcp
{
public:
    typedef tcp_detail::eTcpSndStatus e_snd_status;
    typedef tcp_detail::SessionId sess_id_t;
    typedef tcp_detail::TcpServer server;
    typedef tcp_detail::TcpClient client;
    typedef boost::asio::ip::tcp::endpoint endpoint;
    typedef tcp_detail::Buffer Buffer;
    typedef tcp_detail::SndCb SndCb;

    static void Send(sess_id_t id, Buffer && buf, SndCb cb = NULL);
    static void Send(sess_id_t id, const void* data, size_t bytes, SndCb cb = NULL);
    static void Shutdown(sess_id_t id);
    static bool IsEstab(sess_id_t id);
    static endpoint LocalAddr(sess_id_t id);
    static endpoint RemoteAddr(sess_id_t id);
};

}//namespace network

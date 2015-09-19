#pragma once

#include "error.h"
#include "tcp.h"
#include "udp.h"

namespace network {

    struct ProtocolRef
    {
        Protocol* operator->() const
        {
            return *proto_;
        }

    private:
        Protocol** proto_;
        explicit ProtocolRef(Protocol* &proto);
        friend class Server;
        friend class Client;
    };

    class Server : public Options<Server>
    {
    public:
        // @url:
        //    tcp://127.0.0.1:3030
        //    udp://127.0.0.1:3030
        boost_ec goStart(std::string const& url);
        void Shutdown();
        endpoint LocalAddr();
        ProtocolRef GetProtocol();

    private:
        Protocol * protocol_ = nullptr;
        boost::shared_ptr<ServerBase> impl_;
        endpoint local_addr_;
    };

    class Client : public Options<Client>
    {
    public:
        // @url:
        //    tcp://127.0.0.1:3030
        //    udp://127.0.0.1:3030
        boost_ec Connect(std::string const& url);
        void Send(const void* data, size_t bytes, SndCb cb = NULL);
        void Shutdown();
        endpoint LocalAddr();
        endpoint RemoteAddr();
        ProtocolRef GetProtocol();
        SessionId GetSessId();

    private:
        Protocol * protocol_ = nullptr;
        boost::shared_ptr<ClientBase> impl_;
        endpoint local_addr_;
    };

} //namespace network

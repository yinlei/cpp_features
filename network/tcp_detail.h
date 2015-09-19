#pragma once

#include <string>
#include <stdint.h>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/enable_shared_from_this.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/variant.hpp>
#include <boost/function.hpp>
#include <coroutine/coroutine.h>
#include "error.h"
#include "abstract.h"
#include "option.h"

namespace network {
namespace tcp_detail {

using namespace boost::asio;
using namespace boost::asio::ip;
using boost_ec = boost::system::error_code;
using boost::shared_ptr;

class TcpSession;
typedef shared_ptr<TcpSession> tcp_sess_id_t;
class LifeHolder {};

io_service& GetTcpIoService();

class TcpServerImpl;
class TcpSession
    : public Options<TcpSession>, public boost::enable_shared_from_this<TcpSession>, public SessionIdBase
{
public:
    struct Msg
        : public boost::intrusive::list_base_hook<
            boost::intrusive::link_mode<boost::intrusive::auto_unlink>
          >
    {
        bool timeout = false;
        bool send_half = false;
        std::size_t pos = 0;
        std::atomic<int> use_ref;
        uint64_t id;
        SndCb cb;
        co_timer_id tid;
        Buffer buf;

        Msg(uint64_t uid, SndCb ocb) : use_ref{1}, id(uid), cb(ocb) {}
        void IncrementRef() { ++use_ref; }
        void DecrementRef() { if (--use_ref == 0) delete this; }
    };
    typedef boost::intrusive::list<
            Msg,
            boost::intrusive::constant_time_size<false>
        > MsgList;

    explicit TcpSession(shared_ptr<tcp::socket> s, shared_ptr<LifeHolder> holder, uint32_t max_pack_size);
    ~TcpSession();
    void goStart();
    void Send(Buffer && buf, SndCb const& cb = NULL);
    void Send(const void* data, size_t bytes, SndCb const& cb = NULL);
    void Shutdown();
    bool IsEstab();
    tcp_sess_id_t GetId();

private:
    void goReceive();
    void goSend();
    void SetCloseEc(boost_ec const& ec);
    void OnClose();
    bool CancelSend(Msg* msg);

private:
    shared_ptr<tcp::socket> socket_;
    shared_ptr<LifeHolder> holder_;
    Buffer recv_buf_;
    uint64_t msg_id_;
    co_mutex send_msg_list_mutex_;
    co_chan<void> send_msg_cond_;
    MsgList send_msg_list_;
    std::atomic<int> shutdown_ref_;
    co_mutex close_ec_mutex_;
    boost_ec close_ec_;

public:
    tcp::endpoint local_addr_;
    tcp::endpoint remote_addr_;
};

class TcpServerImpl
    : public Options<TcpServerImpl>, public LifeHolder, public boost::enable_shared_from_this<TcpServerImpl>
{
public:
    typedef std::map<::network::SessionId, shared_ptr<TcpSession>> Sessions;

    boost_ec goStart(std::string const& host, uint16_t port);
    void ShutdownAll();
    void Shutdown();
    tcp::endpoint LocalAddr();

private:
    void Accept();
    void OnSessionClose(::network::SessionId id, boost_ec const& ec);

private:
    shared_ptr<tcp::acceptor> acceptor_;
    tcp::endpoint local_addr_;
    shared_ptr<tcp::socket> socket_;
    co_mutex sessions_mutex_;
    Sessions sessions_;
    std::atomic<bool> shutdown_{false};
    friend TcpSession;
};

class TcpServer
    : public Options<TcpServer>, public ServerBase
{
public:
    TcpServer() : impl_(new TcpServerImpl())
    {
        Link(*impl_);
    }

    ~TcpServer()
    {
        Shutdown();
    }

    boost_ec goStart(std::string const& host, uint16_t port)
    {
        return impl_->goStart(host, port);
    }

    void ShutdownAll()
    {
        impl_->ShutdownAll();
    }

    void Shutdown()
    {
        impl_->Shutdown();
    }

    tcp::endpoint LocalAddr()
    {
        return impl_->LocalAddr();
    }

    OptionsBase* GetOptions()
    {
        return this;
    }

private:
    shared_ptr<TcpServerImpl> impl_;
};

class TcpClientImpl
    : public Options<TcpClientImpl>, public LifeHolder, public boost::enable_shared_from_this<TcpClientImpl>
{
public:
    boost_ec Connect(std::string const& host, uint16_t port);
    tcp_sess_id_t GetSessId();

private:
    void OnSessionClose(::network::SessionId id, boost_ec const& ec);

private:
    shared_ptr<TcpSession> sess_;
    co_mutex connect_mtx_;
    friend TcpSession;
};

class TcpClient
    : public Options<TcpClient>, public ClientBase
{
public:
    TcpClient() : impl_(new TcpClientImpl())
    {
        Link(*impl_);
    }
    ~TcpClient();

    boost_ec Connect(std::string const& host, uint16_t port)
    {
        return impl_->Connect(host, port);
    }
    SessionId GetSessId()
    {
        return impl_->GetSessId();
    }
    OptionsBase* GetOptions()
    {
        return this;
    }

private:
    shared_ptr<TcpClientImpl> impl_;
};

} //namespace tcp_detail
} //namespace network



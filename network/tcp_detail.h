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

namespace network {

static const uint64_t dbg_accept_error           = co::dbg_sys_max;
static const uint64_t dbg_accept_debug           = co::dbg_sys_max << 1;
static const uint64_t dbg_session_alive          = co::dbg_sys_max << 2;
static const uint64_t dbg_network_max            = dbg_session_alive;

namespace tcp_detail {

using namespace boost::asio;
using namespace boost::asio::ip;
using boost_ec = boost::system::error_code;
using boost::shared_ptr;

enum class eTcpSndStatus
{
    snd_ok,
    snd_timeout,
    snd_error,
};

typedef boost::function<void(eTcpSndStatus)> SndCb;
typedef std::vector<char> Buffer;
class TcpSession;
typedef shared_ptr<TcpSession> SessionId;
typedef boost::function<void(SessionId)> ConnectedCb;
typedef boost::function<void(SessionId, const char* data, size_t bytes)> ReceiveCb;
typedef boost::function<void(SessionId, boost_ec const&)> DisconnectedCb;
class LifeHolder {};

struct TcpOptionsData
{
    int sndtimeo_ = 0;
    uint32_t max_pack_size_ = 4096;
    ConnectedCb connect_cb_;
    ReceiveCb receive_cb_;
    DisconnectedCb disconnect_cb_;

    static TcpOptionsData& DefaultOption()
    {
        static TcpOptionsData data;
        return data;
    }
};

struct TcpOptionsBase
{
    TcpOptionsData opt_;
    std::list<TcpOptionsBase*> lnks_;

    void Link(TcpOptionsBase & other)
    {
        lnks_.push_back(&other);
    }

    void SetConnectedCb(ConnectedCb cb)
    {
        opt_.connect_cb_ = cb;
        OnSetConnectedCb();
        for (auto o:lnks_)
            o->SetConnectedCb(cb);
    }
    void SetReceiveCb(ReceiveCb cb)
    {
        opt_.receive_cb_ = cb;
        OnSetReceiveCb();
        for (auto o:lnks_)
            o->SetReceiveCb(cb);
    }
    void SetDisconnectedCb(DisconnectedCb cb)
    {
        opt_.disconnect_cb_ = cb;
        OnSetDisconnectedCb();
        for (auto o:lnks_)
            o->SetDisconnectedCb(cb);
    }
    void SetSndTimeout(int sndtimeo)
    {
        opt_.sndtimeo_ = sndtimeo;
        OnSetSndTimeout();
        for (auto o:lnks_)
            o->SetSndTimeout(sndtimeo);
    }
    void SetMaxPackSize(uint32_t max_pack_size)
    {
        opt_.max_pack_size_ = max_pack_size;
        OnSetMaxPackSize();
        for (auto o:lnks_)
            o->SetMaxPackSize(max_pack_size);
    }
    virtual void OnSetConnectedCb() {}
    virtual void OnSetReceiveCb() {}
    virtual void OnSetDisconnectedCb() {}
    virtual void OnSetSndTimeout() {}
    virtual void OnSetMaxPackSize() {}
};

template <typename Drived>
struct TcpOptions : public TcpOptionsBase
{
    Drived& GetThisDrived()
    {
        return *static_cast<Drived*>(this);
    }

    Drived& SetConnectedCb(ConnectedCb cb)
    {
        TcpOptionsBase::SetConnectedCb(cb);
        return GetThisDrived();
    }
    Drived& SetReceiveCb(ReceiveCb cb)
    {
        TcpOptionsBase::SetReceiveCb(cb);
        return GetThisDrived();
    }
    Drived& SetDisconnectedCb(DisconnectedCb cb)
    {
        TcpOptionsBase::SetDisconnectedCb(cb);
        return GetThisDrived();
    }
    Drived& SetSndTimeout(int sndtimeo)
    {
        TcpOptionsBase::SetSndTimeout(sndtimeo);
        return GetThisDrived();
    }
    Drived& SetMaxPackSize(uint32_t max_pack_size)
    {
        TcpOptionsBase::SetMaxPackSize(max_pack_size);
        return GetThisDrived();
    }
};

io_service& GetTcpIoService();

class TcpServerImpl;
class TcpSession
    : public TcpOptions<TcpSession>, public boost::enable_shared_from_this<TcpSession>
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
    void Send(Buffer && buf, SndCb cb = NULL);
    void Send(const void* data, size_t bytes, SndCb cb = NULL);
    void Shutdown();
    bool IsEstab();
    SessionId GetId();

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
    : public TcpOptions<TcpServerImpl>, public LifeHolder, public boost::enable_shared_from_this<TcpServerImpl>
{
public:
    typedef std::map<SessionId, shared_ptr<TcpSession>> Sessions;

    boost_ec goStart(std::string const& host, uint16_t port);
    void ShutdownAll();
    void ShutdownServer();
    tcp::endpoint LocalAddr();

private:
    void Accept();
    void OnSessionClose(SessionId id, boost_ec const& ec);

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
    : public TcpOptions<TcpServer>
{
public:
    TcpServer() : impl_(new TcpServerImpl())
    {
        Link(*impl_);
    }

    ~TcpServer()
    {
        ShutdownServer();
    }

    boost_ec goStart(std::string const& host, uint16_t port)
    {
        return impl_->goStart(host, port);
    }

    void ShutdownAll()
    {
        impl_->ShutdownAll();
    }

    void ShutdownServer()
    {
        impl_->ShutdownServer();
    }

    tcp::endpoint LocalAddr()
    {
        return impl_->LocalAddr();
    }

private:
    shared_ptr<TcpServerImpl> impl_;
};

class TcpClientImpl
    : public TcpOptions<TcpClientImpl>, public LifeHolder, public boost::enable_shared_from_this<TcpClientImpl>
{
public:
    boost_ec Connect(std::string const& host, uint16_t port);
    SessionId GetSessId();

private:
    void OnSessionClose(SessionId id, boost_ec const& ec);

private:
    shared_ptr<TcpSession> sess_;
    co_mutex connect_mtx_;
    friend TcpSession;
};

class TcpClient
    : public TcpOptions<TcpClient>
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

private:
    shared_ptr<TcpClientImpl> impl_;
};

void Send(SessionId id, Buffer && buf, SndCb cb = NULL);
void Send(SessionId id, const void* data, size_t bytes, SndCb cb = NULL);
void Shutdown(SessionId id);
bool IsEstab(SessionId id);
tcp::endpoint LocalAddr(SessionId id);
tcp::endpoint RemoteAddr(SessionId id);

} //namespace tcp_detail

} //namespace network



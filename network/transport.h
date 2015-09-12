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

namespace network {

using namespace boost::asio;
using namespace boost::asio::ip;
using boost_ec = boost::system::error_code;
using boost::shared_ptr;

static const uint64_t dbg_accept_error           = co::dbg_sys_max;
static const uint64_t dbg_accept_debug           = co::dbg_sys_max << 1;
static const uint64_t dbg_network_max            = dbg_accept_debug;

enum class eTcpSndStatus
{
    snd_ok,
    snd_timeout,
    snd_error,
};

typedef boost::function<void(eTcpSndStatus)> SndCb;
typedef std::vector<char> Buffer;

class Session {};
typedef shared_ptr<Session> SessionId;

class TcpServer;
class TcpSession
    : public Session, public boost::enable_shared_from_this<TcpSession>
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

    explicit TcpSession(shared_ptr<tcp::socket> s, TcpServer* server);
    ~TcpSession();
    void goStart();
    void SetTimeout(int sndtimeo);
    void Send(Buffer && buf, SndCb cb = NULL);
    void Send(const void* data, size_t bytes, SndCb cb = NULL);
    void Shutdown();
    SessionId GetId();

private:
    void goReceive();
    void goSend();
    void SetCloseEc(boost_ec const& ec);
    void OnClose();
    bool CancelSend(Msg* msg);

private:
    shared_ptr<tcp::socket> socket_;
    Buffer recv_buf_;
    int sndtimeo_;
    uint64_t msg_id_;
    co_mutex send_msg_list_mutex_;
    co_chan<void> send_msg_cond_;
    MsgList send_msg_list_;
    TcpServer* server_;
    std::atomic<int> shutdown_ref_;
    co_mutex close_ec_mutex_;
    boost_ec close_ec_;
};

class TcpServer
{
public:
    typedef std::map<SessionId, shared_ptr<TcpSession>> Sessions;
    typedef boost::function<void(SessionId)> ConnectedCb;
    typedef boost::function<void(SessionId, const char* data, size_t bytes, boost_ec&)> ReceiveCb;
    typedef boost::function<void(SessionId, boost_ec const&)> DisconnectedCb;

    TcpServer& SetConnectedCb(ConnectedCb cb);
    TcpServer& SetReceiveCb(ReceiveCb cb);
    TcpServer& SetDisconnectedCb(DisconnectedCb cb);
    TcpServer& SetSndTimeout(int sndtimeo);

    boost_ec goStart(std::string const& host, uint16_t port);
    void Send(SessionId id, Buffer && buf, SndCb cb = NULL);
    void Send(SessionId id, const void* data, size_t bytes, SndCb cb = NULL);
    void Shutdown(SessionId id);
    tcp::endpoint LocalAddr();

private:
    void Accept();
    void OnSessionClose(SessionId id);

private:
    io_service ios_;
    shared_ptr<tcp::acceptor> acceptor_;
    tcp::endpoint local_addr_;
    shared_ptr<tcp::socket> socket_;
    ConnectedCb connect_cb_;
    ReceiveCb receive_cb_;
    DisconnectedCb disconnect_cb_;
    Sessions sessions_;
    int sndtimeo_ = 0;
    friend TcpSession;
};

} //namespace network


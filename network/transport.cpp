#include "transport.h"
#include <chrono>

namespace network {

TcpSession::TcpSession(shared_ptr<tcp::socket> s,
        TcpServer* server)
    : socket_(s), recv_buf_(4096), server_(server),
    shutdown_ref_{2}
{
}

TcpSession::~TcpSession()
{
    std::lock_guard<co_mutex> lock(send_msg_list_mutex_);
    auto it = send_msg_list_.begin();
    while (it != send_msg_list_.end()) {
        Msg *msg = &*it;
        if (msg->cb)
            msg->cb(eTcpSndStatus::snd_error);
        it = send_msg_list_.erase(it);
        msg->DecrementRef();
    }
}

void TcpSession::goStart()
{
    go [this] {
        if (server_->connect_cb_)
            server_->connect_cb_(GetId());
        goReceive();
        goSend();
    };
}

void TcpSession::SetTimeout(int sndtimeo)
{
    sndtimeo_ = sndtimeo;
}

void TcpSession::goReceive()
{
    go [this]{
        for (;;)
        {
            boost_ec ec;
            std::size_t n = socket_->read_some(buffer(recv_buf_), ec);
            if (!ec && this->server_->receive_cb_)
                this->server_->receive_cb_(GetId(), recv_buf_.data(), n, ec);

            if (ec) {
                SetCloseEc(ec);
                boost_ec ignore_ec;
                socket_->shutdown(socket_base::shutdown_both, ignore_ec);
                if (--shutdown_ref_ == 0) {
                    OnClose();
                } else {
                    send_msg_cond_.TryPush(nullptr);
                }

                return ;
            }
        }
    };
}

void TcpSession::goSend()
{
    go [this]{
        for (;;)
        {
            std::list<Msg*> msgs;

            {
                std::unique_lock<co_mutex> lock(send_msg_list_mutex_);
                if (send_msg_list_.empty()) {
                    lock.unlock();
                    send_msg_cond_ >> nullptr;
                    if (shutdown_ref_ == 1) {
                        --shutdown_ref_;
                        OnClose();
                        return ;
                    }
                    lock.lock();
                }

                auto it = send_msg_list_.begin();
                for (int i = 0; it != send_msg_list_.end() && i < 128; ++i)
                {
                    Msg* msg = &*it;
                    msgs.push_back(msg);
                    it = send_msg_list_.erase(it);
                }
            }

            // Delete timeout msg.
            auto it = msgs.begin();
            while (it != msgs.end()) {
                Msg *msg = *it;
                if (msg->timeout && !msg->send_half) {
                    if (msg->cb)
                        msg->cb(eTcpSndStatus::snd_timeout);
                    it = msgs.erase(it);
                    msg->DecrementRef();
                } else {
                    ++it;
                }
            }

            if (msgs.empty())
                continue;

            // Make buffers
            std::vector<const_buffer> buffers(msgs.size());
            int i = 0;
            for (auto it = msgs.begin(); it != msgs.end(); ++it, ++i)
            {
                Msg *msg = *it;
                buffers[i] = buffer(&msg->buf[msg->pos], msg->buf.size() - msg->pos);
            }

            // Send Once
            boost_ec ec;
            std::size_t n = socket_->write_some(buffers, ec);
            if (ec) {
                SetCloseEc(ec);
                for (auto msg : msgs)
                {
                    if (msg->cb)
                        msg->cb(eTcpSndStatus::snd_error);
                    msg->DecrementRef();
                }

                boost_ec ignore_ec;
                socket_->shutdown(socket_base::shutdown_both, ignore_ec);
                if (--shutdown_ref_ == 0) {
                    OnClose();
                }

                return ;
            }

            // Remove sended msg. restore send-half and non-send msgs.
            it = msgs.begin();
            while (it != msgs.end() && n > 0) {
                Msg *msg = *it;
                std::size_t msg_capa = msg->buf.size() - msg->pos;
                if (msg_capa <= n) {
                    if (msg->cb)
                        msg->cb(eTcpSndStatus::snd_ok);
                    it = msgs.erase(it);
                    msg->DecrementRef();
                    n -= msg_capa;
                } else if (msg_capa > n) {
                    msg->pos += n;
                    msg->send_half = true;
                    break;
                }
            }

            for (auto msg : msgs)
            {
                if (msg->timeout && !msg->send_half) {
                    if (msg->cb)
                        msg->cb(eTcpSndStatus::snd_timeout);
                    msg->DecrementRef();
                    continue;
                }

                send_msg_list_.push_front(*msg);
            }
        }
    };
}

void TcpSession::SetCloseEc(boost_ec const& ec)
{
    std::lock_guard<co_mutex> lock(close_ec_mutex_);
    if (!close_ec_)
        close_ec_ = ec;
}

void TcpSession::OnClose()
{
    if (this->server_->disconnect_cb_)
        this->server_->disconnect_cb_(GetId(), close_ec_);
    boost_ec ignore_ec;
    socket_->close(ignore_ec);
    server_->OnSessionClose(GetId());
}

void TcpSession::Send(Buffer && buf, SndCb cb)
{
    if (!shutdown_ref_ && cb)
        cb(eTcpSndStatus::snd_error);

    Msg *msg = new Msg(++msg_id_, cb);
    msg->buf = std::move(buf);

    co::RefGuard<Msg> ref_guard(msg);

    {
        std::lock_guard<co_mutex> lock(send_msg_list_mutex_);
        bool cond = false;
        if (send_msg_list_.empty())
            cond = true;
        send_msg_list_.push_back(*msg);
        if (cond)
            send_msg_cond_.TryPush(nullptr);
    }

    if (sndtimeo_) {
        msg->IncrementRef();
        auto this_ptr = this->shared_from_this();
        msg->tid = co_timer_add(std::chrono::milliseconds(sndtimeo_), [=]{
                    msg->timeout = true;
                    if (this_ptr->CancelSend(msg))
                        if (msg->cb)
                            msg->cb(eTcpSndStatus::snd_timeout);

                    msg->DecrementRef();
                });
    }
}
void TcpSession::Send(const void* data, size_t bytes, SndCb cb)
{
    Buffer buf(bytes);
    memcpy(&buf[0], data, bytes);
    Send(std::move(buf), cb);
}

bool TcpSession::CancelSend(Msg* msg)
{
    {
        std::lock_guard<co_mutex> lock(send_msg_list_mutex_);
        if (!msg->is_linked()) return false;
        msg->unlink();
    }
    msg->DecrementRef();
    return true;
}

void TcpSession::Shutdown()
{
    boost_ec ignore_ec;
    socket_->shutdown(socket_base::shutdown_both, ignore_ec);
}

SessionId TcpSession::GetId()
{
    return boost::static_pointer_cast<Session>(this->shared_from_this()); 
}

TcpServer& TcpServer::SetConnectedCb(ConnectedCb cb)
{
    connect_cb_ = cb;
    return *this;
}
TcpServer& TcpServer::SetReceiveCb(ReceiveCb cb)
{
    receive_cb_ = cb;
    return *this;
}
TcpServer& TcpServer::SetDisconnectedCb(DisconnectedCb cb)
{
    disconnect_cb_ = cb;
    return *this;
}
TcpServer& TcpServer::SetSndTimeout(int sndtimeo)
{
    sndtimeo_ = sndtimeo;
    for (auto &v : sessions_)
        v.second->SetTimeout(sndtimeo_);
    return *this;
}

boost_ec TcpServer::goStart(std::string const& host, uint16_t port)
{
    try {
        local_addr_ = tcp::endpoint(address::from_string(host), port);
        acceptor_.reset(new tcp::acceptor(ios_, local_addr_, true));
    } catch (boost::system::system_error& e)
    {
        return e.code();
    }

    go [this] {
        this->Accept();
    };
    return boost_ec();
}

void TcpServer::Send(SessionId id, Buffer && buf, SndCb cb)
{
    auto it = sessions_.find(id);
    if (it == sessions_.end()) {
        if (cb)
            cb(eTcpSndStatus::snd_error);
        return ;
    }

    it->second->Send(std::move(buf), cb);
}
void TcpServer::Send(SessionId id, const void* data, size_t bytes, SndCb cb)
{
    auto it = sessions_.find(id);
    if (it == sessions_.end()) {
        if (cb)
            cb(eTcpSndStatus::snd_error);
        return ;
    }

    it->second->Send(data, bytes, cb);
}
void TcpServer::Shutdown(SessionId id)
{
    auto it = sessions_.find(id);
    if (it == sessions_.end()) return ;

    it->second->Shutdown();
}

void TcpServer::Accept()
{
    for (;;)
    {
        shared_ptr<tcp::socket> s(new tcp::socket(ios_));
        boost_ec ec;
        acceptor_->accept(*s, ec);
        if (ec) {
            DebugPrint(dbg_accept_error, "accept error %d:%s",
                    ec.value(), ec.message().c_str());
            co_yield;
            continue;
        }

        DebugPrint(dbg_accept_debug, "accept from %s:%d",
                s->remote_endpoint().address().to_string().c_str(),
                s->remote_endpoint().port());

        shared_ptr<TcpSession> sess(new TcpSession(s, this));
        sessions_[sess->GetId()] = sess;
        sess->SetTimeout(sndtimeo_);
        sess->goStart();
    }
}

void TcpServer::OnSessionClose(SessionId id)
{
    sessions_.erase(id);
}

tcp::endpoint TcpServer::LocalAddr()
{
    return local_addr_;
}

} //namespace network


#include "tcp_detail.h"
#include <chrono>
#include <boost/bind.hpp>

namespace network {
namespace tcp_detail {

    io_service& GetTcpIoService()
    {
        static io_service ios;
        return ios;
    }

    TcpSession::TcpSession(shared_ptr<tcp::socket> s, shared_ptr<LifeHolder> holder, uint32_t max_pack_size)
        : socket_(s), holder_(holder), recv_buf_(max_pack_size), send_msg_cond_(1), shutdown_ref_{2}
    {
        boost_ec ignore_ec;
        local_addr_ = s->local_endpoint(ignore_ec);
        remote_addr_ = s->remote_endpoint(ignore_ec);
    }

    TcpSession::~TcpSession()
    {
        DebugPrint(dbg_session_alive, "TcpSession destruct %s:%d",
                remote_addr_.address().to_string().c_str(), remote_addr_.port());
        std::lock_guard<co_mutex> lock(send_msg_list_mutex_);
        auto it = send_msg_list_.begin();
        while (it != send_msg_list_.end()) {
            Msg *msg = &*it;
            if (msg->cb)
                msg->cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            it = send_msg_list_.erase(it);
            msg->DecrementRef();
        }
    }

    void TcpSession::goStart()
    {
        auto this_ptr = this->shared_from_this();
        if (opt_.connect_cb_)
            opt_.connect_cb_(GetId());

        go [=] {
            auto holder = this_ptr;
            goReceive();
            goSend();
        };

    }

    void TcpSession::goReceive()
    {
        auto this_ptr = this->shared_from_this();
        go [=]{
            auto holder = this_ptr;
            size_t pos = 0;
            for (;;)
            {
                boost_ec ec;
                std::size_t n = 0;
                if (pos >= recv_buf_.size()) {
                    ec = MakeNetworkErrorCode(eNetworkErrorCode::ec_recv_overflow);
                } else {
                    n = socket_->read_some(buffer(&recv_buf_[pos], recv_buf_.size() - pos), ec);
                }

                if (!ec) {
                    if(n > 0) {
                        if (this->opt_.receive_cb_) {
                            size_t consume = this->opt_.receive_cb_(GetId(), recv_buf_.data(), n + pos);
                            if (consume == (size_t)-1)
                                ec = MakeNetworkErrorCode(eNetworkErrorCode::ec_data_parse_error);
                            else {
                                assert(consume <= n + pos);
                                pos = n + pos - consume;
                                if (pos > 0)
                                    memcpy(&recv_buf_[0], &recv_buf_[consume], pos);
                            }
                        } else {
                            pos += n;
                        }
                    }
                }

                if (ec) {
                    SetCloseEc(ec);
                    boost_ec ignore_ec;
                    socket_->shutdown(socket_base::shutdown_both, ignore_ec);
                    if (--shutdown_ref_ == 0) {
                        OnClose();
                    } else {
                        send_msg_cond_.TryPush(nullptr);
                    }

                    DebugPrint(dbg_session_alive, "TcpSession receive shutdown %s:%d",
                            remote_addr_.address().to_string().c_str(), remote_addr_.port());
                    return ;
                } 
            }
        };
    }

    void TcpSession::goSend()
    {
        auto this_ptr = this->shared_from_this();
        go [=]{
            auto holder = this_ptr;
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
                            DebugPrint(dbg_session_alive, "TcpSession send shutdown with cond %s:%d",
                                    remote_addr_.address().to_string().c_str(), remote_addr_.port());
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
                            msg->cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_timeout));
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
                            msg->cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
                        msg->DecrementRef();
                    }

                    boost_ec ignore_ec;
                    socket_->shutdown(socket_base::shutdown_both, ignore_ec);
                    if (--shutdown_ref_ == 0) {
                        OnClose();
                    }

                    DebugPrint(dbg_session_alive, "TcpSession send shutdown with write_some %s:%d",
                            remote_addr_.address().to_string().c_str(), remote_addr_.port());
                    return ;
                }

                // Remove sended msg. restore send-half and non-send msgs.
                it = msgs.begin();
                while (it != msgs.end() && n > 0) {
                    Msg *msg = *it;
                    std::size_t msg_capa = msg->buf.size() - msg->pos;
                    if (msg_capa <= n) {
                        if (msg->cb)
                            msg->cb(boost_ec());
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
                            msg->cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_timeout));
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
        DebugPrint(dbg_session_alive, "TcpSession close %s:%d",
                remote_addr_.address().to_string().c_str(), remote_addr_.port());
        boost_ec ignore_ec;
        socket_->close(ignore_ec);
        if (this->opt_.disconnect_cb_)
            this->opt_.disconnect_cb_(GetId(), close_ec_);
    }

    void TcpSession::Send(Buffer && buf, SndCb const& cb)
    {
        if (!shutdown_ref_) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        if (buf.empty()) {
            if (cb)
                cb(boost_ec());
            return ;
        }

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

        if (opt_.sndtimeo_) {
            msg->IncrementRef();
            auto this_ptr = this->shared_from_this();
            msg->tid = co_timer_add(std::chrono::milliseconds(opt_.sndtimeo_), [=]{
                    msg->timeout = true;
                    if (this_ptr->CancelSend(msg))
                    if (msg->cb)
                    msg->cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_timeout));

                    msg->DecrementRef();
                    });
        }
    }
    void TcpSession::Send(const void* data, size_t bytes, SndCb const& cb)
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
        SetCloseEc(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
        boost_ec ignore_ec;
        socket_->shutdown(socket_base::shutdown_both, ignore_ec);
    }

    bool TcpSession::IsEstab()
    {
        return !close_ec_;
    }

    tcp_sess_id_t TcpSession::GetId()
    {
        return this->shared_from_this();
    }

    boost_ec TcpServerImpl::goStart(endpoint addr)
    {
        try {
            local_addr_ = addr;
            acceptor_.reset(new tcp::acceptor(GetTcpIoService(), local_addr_, true));
        } catch (boost::system::system_error& e)
        {
            return e.code();
        }

        auto this_ptr = this->shared_from_this();
        go [this_ptr] {
            this_ptr->Accept();
        };
        return boost_ec();
    }

    void TcpServerImpl::ShutdownAll()
    {
        std::lock_guard<co_mutex> lock(sessions_mutex_);
        for (auto &v : sessions_)
            v.second->Shutdown();
    }
    void TcpServerImpl::Shutdown()
    {
        shutdown_ = true;
        shutdown(acceptor_->native_handle(), socket_base::shutdown_both);
        ShutdownAll();
    }
    void TcpServerImpl::Accept()
    {
        for (;;)
        {
            shared_ptr<tcp::socket> s(new tcp::socket(GetTcpIoService()));
            boost_ec ec;
            acceptor_->accept(*s, ec);
            if (ec) {
                if (shutdown_) {
                    boost_ec ignore_ec;
                    acceptor_->close(ignore_ec);
                    DebugPrint(dbg_accept_debug, "accept end");
                    return ;
                }

                DebugPrint(dbg_accept_error, "accept error %d:%s",
                        ec.value(), ec.message().c_str());
                co_yield;
                continue;
            }

            DebugPrint(dbg_accept_debug, "accept from %s:%d",
                    s->remote_endpoint().address().to_string().c_str(),
                    s->remote_endpoint().port());

            shared_ptr<TcpSession> sess(new TcpSession(s, this->shared_from_this(), opt_.max_pack_size_));

            {
                std::lock_guard<co_mutex> lock(sessions_mutex_);
                sessions_[sess->GetId()] = sess;
            }

            sess->SetSndTimeout(opt_.sndtimeo_)
                .SetConnectedCb(opt_.connect_cb_)
                .SetReceiveCb(opt_.receive_cb_)
                .SetDisconnectedCb(boost::bind(&TcpServerImpl::OnSessionClose, this, _1, _2))
                .goStart();
        }
    }

    void TcpServerImpl::OnSessionClose(::network::SessionId id, boost_ec const& ec)
    {
        if (opt_.disconnect_cb_)
            opt_.disconnect_cb_(id, ec);

        std::lock_guard<co_mutex> lock(sessions_mutex_);
        sessions_.erase(id);
    }

    tcp::endpoint TcpServerImpl::LocalAddr()
    {
        return local_addr_;
    }


    boost_ec TcpClientImpl::Connect(endpoint addr)
    {
        if (sess_ && sess_->IsEstab()) return MakeNetworkErrorCode(eNetworkErrorCode::ec_estab);
        std::unique_lock<co_mutex> lock(connect_mtx_, std::defer_lock);
        if (!lock.try_lock()) return MakeNetworkErrorCode(eNetworkErrorCode::ec_connecting);

        shared_ptr<tcp::socket> s(new tcp::socket(GetTcpIoService()));
        boost_ec ec;
        s->connect(addr, ec);
        if (ec)
            return ec;

        sess_.reset(new TcpSession(s, this->shared_from_this(), opt_.max_pack_size_));
        sess_->SetSndTimeout(opt_.sndtimeo_)
            .SetConnectedCb(opt_.connect_cb_)
            .SetReceiveCb(opt_.receive_cb_)
            .SetDisconnectedCb(boost::bind(&TcpClientImpl::OnSessionClose, this, _1, _2))
            .goStart();
        return boost_ec();
    }
    tcp_sess_id_t TcpClientImpl::GetSessId()
    {
        return sess_ ? sess_->GetId() : tcp_sess_id_t();
    }

    void TcpClientImpl::OnSessionClose(::network::SessionId id, boost_ec const& ec)
    {
        if (opt_.disconnect_cb_)
            opt_.disconnect_cb_(id, ec);
        sess_.reset();
    }

    TcpClient::~TcpClient()
    {
        auto sess = impl_->GetSessId();
        if (sess)
            sess->Shutdown();
    }

} //namespace tcp_detail
} //namespace network


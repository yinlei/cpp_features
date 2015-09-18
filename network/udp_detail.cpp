#include "udp_detail.h"

namespace network {
namespace udp_detail {

    UdpPointImpl::UdpPointImpl()
    {
        recv_buf_.resize(opt_.max_pack_size_);
        local_addr_ = udp::endpoint(udp::v4(), 0);
    }

    io_service& UdpPointImpl::GetUdpIoService()
    {
        static io_service ios;
        return ios;
    }

    boost_ec UdpPointImpl::goStart()
    {
        try {
            socket_.reset(new udp::socket(GetUdpIoService(), local_addr_));
        } catch (boost::system::system_error& e) {
            return e.code();
        }

        auto this_ptr = this->shared_from_this();
        go [this_ptr] {
            this_ptr->DoRecv();
        };
        return boost_ec();
    }
    boost_ec UdpPointImpl::goStart(std::string const& host, uint16_t port)
    {
        local_addr_ = udp::endpoint(address::from_string(host), port);
        boost_ec ec = goStart();
        if (ec)
            local_addr_ = udp::endpoint(udp::v4(), 0);
        return ec;
    }
    void UdpPointImpl::Shutdown()
    {
        if (shutdown_) return ;
        shutdown_ = true;
        boost_ec ignore_ec;
        socket_->shutdown(socket_base::shutdown_both, ignore_ec);
    }
    void UdpPointImpl::OnSetMaxPackSize()
    {
        recv_buf_.resize(opt_.max_pack_size_);
    }
    void UdpPointImpl::DoRecv()
    {
        for (;;)
        {
            udp::endpoint from_addr;
            boost_ec ec;
            std::size_t n = socket_->receive_from(buffer(&recv_buf_[0], recv_buf_.size()), from_addr, 0, ec);
            if (!ec && opt_.receive_cb_)
                opt_.receive_cb_(SessionId{this->shared_from_this(), from_addr}, &recv_buf_[0], n);

            if (shutdown_) break;
        }
    }

    boost_ec UdpPointImpl::Send(std::string const& host, uint16_t port, const void* data, std::size_t bytes)
    {
        udp::endpoint dst(address::from_string(host), port);
        return Send(dst, data, bytes);
    }
    boost_ec UdpPointImpl::Send(udp::endpoint destition, const void* data, std::size_t bytes)
    {
        boost_ec ec;
        std::size_t n = socket_->send_to(buffer(data, bytes), destition, 0, ec);
        if (ec) return ec;
        if (n < bytes) return MakeNetworkErrorCode(eNetworkErrorCode::ec_half);
        return boost_ec();
    }
    boost_ec UdpPointImpl::Connect(std::string const& host, uint16_t port)
    {
        boost_ec ec;
        udp::endpoint addr(address::from_string(host), port);
        socket_->connect(addr, ec);
        if (!ec)
            remote_addr_ = addr;
        return ec;
    }
    boost_ec UdpPointImpl::Send(const void* data, size_t bytes)
    {
        boost_ec ec;
        std::size_t n = socket_->send(buffer(data, bytes), 0, ec);
        if (ec) return ec;
        if (n < bytes) return MakeNetworkErrorCode(eNetworkErrorCode::ec_half);
        return boost_ec();
    }
    udp::endpoint UdpPointImpl::LocalAddr()
    {
        return local_addr_;
    }
    udp::endpoint UdpPointImpl::RemoteAddr()
    {
        return remote_addr_;
    }

} //namespace udp_detail
} //namespace network


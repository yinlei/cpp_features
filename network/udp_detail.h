#pragma once

#include <string>
#include <stdint.h>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <coroutine/coroutine.h>
#include "error.h"

namespace network {
namespace udp_detail {

using namespace boost::asio;
using namespace boost::asio::ip;
using boost_ec = boost::system::error_code;
using boost::shared_ptr;

class UdpPointImpl;
struct SessionId
{
    shared_ptr<UdpPointImpl> udp_point;
    udp::endpoint remote_addr;
};
typedef std::vector<char> Buffer;
typedef boost::function<void(SessionId, const char* data, size_t bytes)> ReceiveCb;

struct UdpOptionsData
{
    uint32_t max_pack_size_ = 4096;
    ReceiveCb receive_cb_;

    static UdpOptionsData& DefaultOption()
    {
        static UdpOptionsData data;
        return data;
    }
};

struct UdpOptionsBase
{
    UdpOptionsData opt_;
    std::list<UdpOptionsBase*> lnks_;

    void Link(UdpOptionsBase & other)
    {
        lnks_.push_back(&other);
    }

    void SetReceiveCb(ReceiveCb cb)
    {
        opt_.receive_cb_ = cb;
        OnSetReceiveCb();
        for (auto o:lnks_)
            o->SetReceiveCb(cb);
    }
    void SetMaxPackSize(uint32_t max_pack_size)
    {
        opt_.max_pack_size_ = max_pack_size;
        OnSetMaxPackSize();
        for (auto o:lnks_)
            o->SetMaxPackSize(max_pack_size);
    }
    virtual void OnSetReceiveCb() {}
    virtual void OnSetMaxPackSize() {}
};

template <typename Drived>
struct UdpOptions : public UdpOptionsBase
{
    Drived& GetThisDrived()
    {
        return *static_cast<Drived*>(this);
    }

    Drived& SetReceiveCb(ReceiveCb cb)
    {
        UdpOptionsBase::SetReceiveCb(cb);
        return GetThisDrived();
    }
    Drived& SetMaxPackSize(uint32_t max_pack_size)
    {
        UdpOptionsBase::SetMaxPackSize(max_pack_size);
        return GetThisDrived();
    }
};

class UdpPointImpl
    : public UdpOptions<UdpPointImpl>, public boost::enable_shared_from_this<UdpPointImpl>
{
public:
    UdpPointImpl();

    boost_ec goStart(std::string const& host, uint16_t port);
    boost_ec goStart();
    void Shutdown();
    boost_ec Send(std::string const& host, uint16_t port, const void* data, std::size_t bytes);
    boost_ec Send(udp::endpoint destition, const void* data, std::size_t bytes);
    boost_ec Connect(std::string const& host, uint16_t port);
    boost_ec Send(const void* data, size_t bytes);
    udp::endpoint LocalAddr();
    udp::endpoint RemoteAddr();

private:
    virtual void OnSetMaxPackSize() override;
    void DoRecv();

    static io_service& GetUdpIoService();

private:
    udp::endpoint local_addr_;
    udp::endpoint remote_addr_;
    shared_ptr<udp::socket> socket_;
    std::atomic<bool> shutdown_{false};
    Buffer recv_buf_;
};

class UdpPoint
    : public UdpOptions<UdpPoint>
{
public:
    UdpPoint() : impl_(new UdpPointImpl())
    {
        Link(*impl_);
    }

    virtual ~UdpPoint()
    {
        Shutdown();
    }

    boost_ec goStart(std::string const& host, uint16_t port)
    {
        return impl_->goStart(host, port);
    }
    boost_ec goStart()
    {
        return impl_->goStart();
    }
    void Shutdown()
    {
        return impl_->Shutdown();
    }
    boost_ec Send(std::string const& host, uint16_t port, const void* data, std::size_t bytes)
    {
        return impl_->Send(host, port, data, bytes);
    }
    boost_ec Send(udp::endpoint destition, const void* data, std::size_t bytes)
    {
        return impl_->Send(destition, data, bytes);
    }
    boost_ec Connect(std::string const& host, uint16_t port)
    {
        return impl_->Connect(host, port);
    }
    boost_ec Send(const void* data, size_t bytes)
    {
        return impl_->Send(data, bytes);
    }
    udp::endpoint LocalAddr()
    {
        return impl_->LocalAddr();
    }
    udp::endpoint RemoteAddr()
    {
        return impl_->RemoteAddr();
    }

private:
    shared_ptr<UdpPointImpl> impl_;
};

typedef UdpPoint UdpServer;
typedef UdpPoint UdpClient;

} //namespace udp_detail
} //namespace network



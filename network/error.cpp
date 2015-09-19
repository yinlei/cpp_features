#include "error.h"

namespace network
{

const char* network_error_category::name() const BOOST_SYSTEM_NOEXCEPT
{
    return "network_error";
}

std::string network_error_category::message(int v) const
{
    switch (v) {
        case (int)eNetworkErrorCode::ec_ok:
            return "ok";

        case (int)eNetworkErrorCode::ec_connecting:
            return "client was connecting";

        case (int)eNetworkErrorCode::ec_estab:
            return "client was ESTAB";

        case (int)eNetworkErrorCode::ec_shutdown:
            return "user shutdown";

        case (int)eNetworkErrorCode::ec_half:
            return "send or recv half of package";

        case (int)eNetworkErrorCode::ec_no_destition:
            return "udp send must be appoint a destition address";

        case (int)eNetworkErrorCode::ec_timeout:
            return "time out";

        case (int)eNetworkErrorCode::ec_parse_error:
            return "url parse error";
    }

    return "";
}

const boost::system::error_category& GetNetworkErrorCategory()
{
    static network_error_category obj;
    return obj;
}
boost_ec MakeNetworkErrorCode(eNetworkErrorCode code)
{
    return boost_ec((int)code, GetNetworkErrorCategory());
}
void ThrowError(eNetworkErrorCode code, const char* what)
{
    if (std::uncaught_exception()) return ;
    throw boost::system::system_error(MakeNetworkErrorCode(code), what);
}

} //namespace network

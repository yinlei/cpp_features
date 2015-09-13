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

#include "url.h"
#include <boost/regex.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

namespace http
{
    url::url()
    {
        clear();
    }

    /// -------------------- parse ----------------------
    url::url(std::string s)
    {
        parse(s);
    }
    url& url::operator=(std::string s)
    {
        parse(s);
        return *this;
    }
    std::string url::to_string() const
    {
        if (!ok()) return "";
        std::string str = protocol_s() + "://" + domain();
        if (port_)
            str += ":" + std::to_string((int)port_);

        if (path() == "/" && data().empty())
            return str;

        str += path();
        if (data().empty())
            return str;

        str += "?" + data();
        return str;
    }

    bool url::ok() const
    {
        return error_.empty() && protocol() != eProtocol::unkown;
    }
    url::operator bool() const
    {
        return ok();
    }
    /// -------------------------------------------------

    /// ------------------- protocol --------------------
    url::eProtocol url::protocol() const
    {
        return protocol_;
    }
    void url::set_protocol(eProtocol proto)
    {
        protocol_ = proto;
    }

#define __HTTP_URL_PROTOCOL_CASE(name) \
    case eProtocol::name: return #name

    std::string url::protocol_s() const
    {
        switch (protocol()) {
            __HTTP_URL_PROTOCOL_CASE(http);
            __HTTP_URL_PROTOCOL_CASE(https);
            __HTTP_URL_PROTOCOL_CASE(ftp);
            __HTTP_URL_PROTOCOL_CASE(mailto);
            __HTTP_URL_PROTOCOL_CASE(ldap);
            __HTTP_URL_PROTOCOL_CASE(file);
            __HTTP_URL_PROTOCOL_CASE(news);
            __HTTP_URL_PROTOCOL_CASE(gopher);
            __HTTP_URL_PROTOCOL_CASE(telnet);
            __HTTP_URL_PROTOCOL_CASE(tcp);
            __HTTP_URL_PROTOCOL_CASE(udp);
            __HTTP_URL_PROTOCOL_CASE(zk);
            default:
            __HTTP_URL_PROTOCOL_CASE(unkown);
        }
    }
    void url::set_protocol_s(std::string s)
    {
        boost::to_lower(s);
        
        if (s == "http") {
            protocol_ = eProtocol::http;
        } else if (s == "https") {
            protocol_ = eProtocol::https;
        } else if (s == "ftp") {
            protocol_ = eProtocol::ftp;
        } else if (s == "mailto") {
            protocol_ = eProtocol::mailto;
        } else if (s == "ldap") {
            protocol_ = eProtocol::ldap;
        } else if (s == "file") {
            protocol_ = eProtocol::file;
        } else if (s == "news") {
            protocol_ = eProtocol::news;
        } else if (s == "gopher") {
            protocol_ = eProtocol::gopher;
        } else if (s == "telnet") {
            protocol_ = eProtocol::telnet;
        } else if (s == "tcp") {
            protocol_ = eProtocol::tcp;
        } else if (s == "udp") {
            protocol_ = eProtocol::udp;
        } else if (s == "zk") {
            protocol_ = eProtocol::zk;
        } else {
            protocol_ = eProtocol::unkown;
        }
    }
    /// -------------------------------------------------

    /// -------------------- domain ---------------------
    std::string url::domain() const
    {
        return domain_;
    }
    void url::set_domain(std::string s)
    {
        domain_ = s;
    }
    /// -------------------------------------------------

    /// --------------------- port ----------------------
    uint16_t url::port() const
    {
        if (port_) return port_;

        switch (protocol()) {
            case eProtocol::http:
                return 80;
            case eProtocol::https:
                return 443;
            case eProtocol::ftp:
                return 21;
            case eProtocol::mailto:
                return 25;
            case eProtocol::ldap:
                return 389;
            case eProtocol::file:
                return 0;
            case eProtocol::news:
                return 0;
            case eProtocol::gopher:
                return 70;
            case eProtocol::telnet:
                return 23;
            case eProtocol::tcp:
                return 0;
            case eProtocol::udp:
                return 0;
            case eProtocol::zk:
                return 2181;

            default:
            case eProtocol::unkown:
                return 0;
        }
    }
    void url::set_port(uint16_t num_port)
    {
        port_ = num_port;
    }
    std::string url::service() const
    {
        uint16_t num_port = port();
        if (num_port)
            return std::to_string((int)num_port);
        else
            return protocol_s();
    }
    /// -------------------------------------------------

    /// --------------------- path ----------------------
    std::string url::path() const
    {
        return path_;
    }
    void url::set_path(std::string s)
    {
        path_ = s;
    }
    /// -------------------------------------------------

    /// --------------------- data ----------------------
    std::string url::data() const
    {
        return data_;
    }
    void url::set_data(std::string s)
    {
        data_ = s;
    }
    /// -------------------------------------------------

    void url::clear()
    {
        protocol_ = eProtocol::unkown;
        domain_.clear();
        port_ = 0;
        path_.clear();
        data_.clear();
        error_.clear();
    }

    void url::parse(std::string s)
    {
        static boost::regex re("^(\\w+)://([^:/]+)(:\\d+)?(/[^\\?]*)?(\\?.*)?$");
        clear();
        boost::smatch results;
        bool ok = boost::regex_match(s, results, re);
        if (!ok) {
            error_ = "regex match error.";
            return ;
        }

        set_protocol_s(results[1].str());
        if (protocol() == eProtocol::unkown) {
            error_ = "unkown protocol";
            return ;
        }

        set_domain(results[2].str());
        if (!results[3].str().empty())
            set_port(atoi(results[3].str().c_str() + 1));

        set_path(results[4].str());
        if (protocol() == eProtocol::http || protocol() == eProtocol::https) {
            if (path().empty())
                set_path("/");
            if (!results[5].str().empty())
                set_data(results[5].str().substr(1));
        } else {
            set_path(path() + results[5].str());
        }
    }

} //namespace http


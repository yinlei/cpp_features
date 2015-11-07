#include "http_head.h"

namespace http
{
    http_head::http_head(eHttpHeadType type)
        : type_(type)
    {
        clear();
    }

    void http_head::clear()
    {
        method_ = eMethod::Unkown;
        uri_.clear();
        status_ = 0;
        version_major_ = 1;
        version_minor_ = 1;
        fields_.clear();
        error_.clear();
    }

    /// 一次性解析
    std::size_t http_head::parse(std::string const& s);

    /// 流式解析
    std::size_t http_head::consume(std::string const& s)
    {
        return consume(s.c_str(), s.length());
    }
    std::size_t http_head::consume(const char* data, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
        {
            char c = data[i];
            consume(c);
            if (state_ == eParseState::done)
                return i + 1;
            else if (state_ == eParseState::error)
                return -1;
        }

        return len;
    }

    std::string http_head::to_string() const;

    bool http_head::ok() const
    {
        return error_.empty();
    }
    http_head::operator bool() const
    {
        return ok();
    }
    std::string http_head::error() const
    {
        return error_;
    }
    /// -------------------------------------------------

    /// ========= request: Method and uri ===============
    eMethod http_head::method() const
    {
        return method_;
    }
    void http_head::set_method(eMethod mthd)
    {
        method_ = mthd;
    }

    std::string http_head::method_s() const
    {
        switch (method_) {
            case eMethod::Get:
                return "GET";
            case eMethod::Post:
                return "POST";
            case eMethod::Put:
                return "PUT";
            case eMethod::Delete:
                return "DELETE";
            case eMethod::Trace:
                return "TRACE";
            case eMethod::Connect:
                return "CONNECT";
            case eMethod::Head:
                return "HEAD";
            case eMethod::Options:
                return "OPTIONS"
            case eMethod::Unkown:
            default:
                return "UNKOWN";
        }
    }
    void http_head::set_method_s(std::string s)
    {
        boost::to_upper(s);
        if (s == "GET") {
            method_ = eMethod::Get;
        } else if (s == "POST") {
            method_ = eMethod::Post;
        } else if (s == "PUT") {
            method_ = eMethod::Put;
        } else if (s == "DELETE") {
            method_ = eMethod::Delete;
        } else if (s == "TRACE") {
            method_ = eMethod::Trace;
        } else if (s == "CONNECT") {
            method_ = eMethod::Connect;
        } else if (s == "HEAD") {
            method_ = eMethod::Head;
        } else if (s == "OPTIONS") {
            method_ = eMethod::Options;
        } else {
            method_ = eMethod::Unkown;
        }
    }

    std::string http_head::uri() const
    {
        return uri_;
    }
    void http_head::set_uri(std::string s)
    {
        uri_ = s;
    }
    /// =================================================

    /// ============ response: StatusCode ===============
    int http_head::status() const
    {
        return status_;
    }
    void http_head::set_status(int v)
    {
        status_ = v;
    }
    /// =================================================

    /// -------------------- version --------------------
    int http_head::version_major() const
    {
        return version_major_;
    }
    int http_head::version_minor() const
    {
        return version_minor_;
    }
    void http_head::set_version_major(int v)
    {
        version_major_ = v;
    }
    void http_head::set_version_minor(int v)
    {
        version_minor_ = v;
    }
    /// -------------------------------------------------

    /// -------------------- fields ---------------------
    ICaseMap& http_head::fields()
    {
        return fields_;
    }
    ICaseMap const& http_head::fields() const
    {
        return fields_;
    }

    std::string http_head::get(std::string key) const
    {
        auto it = fields_.find(key);
        return (fields_.end() == it) ? "" : it->second;
    }

    std::size_t http_head::content_length() const
    {
        std::string v = get("Content-Length");
        return atoi(v.c_str());
    }
    void http_head::set_content_length(std::size_t len)
    {
        auto it = fields_.find("Content-Length");
        if (fields_.end() == it)
            fields_.insert(ICaseMap::value_type("Content-Length",
                        std::to_string(len)));
        else
            it->second = std::to_string(len);
    }

    std::string http_head::host() const
    {
        auto it = fields_.find("Host");
        return (fields_.end() == it) ? "" : it->second;
    }
    void http_head::set_host(std::string s)
    {
        auto it = fields_.find("Content-Length");
        if (fields_.end() == it)
            fields_.insert(ICaseMap::value_type("Content-Length", s));
        else
            it->second = s;
    }
    /// -------------------------------------------------

    eParseState http_head::consume(char c);

} //namespace http

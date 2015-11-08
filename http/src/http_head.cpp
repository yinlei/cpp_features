#include "http_head.h"
#include <assert.h>

namespace http
{
    http_head::http_head(eHttpHeadType type)
        : type_(type)
    {
        clear();
    }

    void http_head::clear()
    {
        type_ = eHttpHeadType::indeterminate;
        method_ = eMethod::Unkown;
        uri_.clear();
        status_ = 0;
        version_major_ = 1;
        version_minor_ = 1;
        fields_.clear();
        parse_node_ = parse_root();
        std::string empty_s;
        parse_buf_.swap(empty_s);
    }

    http_head::eHttpHeadType http_head::type() const
    {
        return type_;
    }
    void http_head::set_type(eHttpHeadType t)
    {
        type_ = t;
    }

    /// 一次性解析
    std::size_t http_head::parse(std::string const& s)
    {
        clear();
        std::size_t n = consume(s);
        if (parse_node_->state_ == eParseState::done)
            return n;
        
        clear();
        return -1;
    }

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
            if (parse_node_->state_ == eParseState::done)
                return i + 1;
            else if (parse_node_->state_ == eParseState::error)
                return -1;
        }

        return len;
    }

    std::string http_head::to_string() const
    {
        std::string s;
        if (type_ == eHttpHeadType::request)
            s += method_s() + " " + uri() + " ";
        else
            s += "STATUS " + std::to_string(status()) + " ";
        s += "HTTP/" + std::to_string(version_major()) + "." + std::to_string(version_minor());
        s += "\r\n";
        for (auto &kv : fields_)
            s += kv.first + ": " + kv.second + "\r\n";
        s += "\r\n";
        return s;
    }

    /// -------------------------------------------------

    /// ========= request: Method and uri ===============
    http_head::eMethod http_head::method() const
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
                return "OPTIONS";
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
    http_head::ICaseMap& http_head::fields()
    {
        return fields_;
    }
    http_head::ICaseMap const& http_head::fields() const
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

    http_head::eParseState http_head::consume(char c)
    {
        parse_node_ = parse_node_->next(c, parse_buf_, parse_buf2_);
        if (parse_node_->state_ == eParseState::indeterminate) {
            if (!parse_buf_.empty() && parse_node_->result_ != eNodeResult::none) {
                switch (parse_node_->result_) {
                    case eNodeResult::method:
                        set_method_s(parse_buf_);
                        set_type(eHttpHeadType::request);
                        break;
                    case eNodeResult::uri:
                        set_uri(parse_buf_);
                        break;
                    case eNodeResult::status:
                        set_status(atoi(parse_buf_.c_str()));
                        set_type(eHttpHeadType::response);
                        break;
                    case eNodeResult::ver_major:
                        set_version_major(atoi(parse_buf_.c_str()));
                        break;
                    case eNodeResult::ver_minor:
                        set_version_minor(atoi(parse_buf_.c_str()));
                        break;
                    case eNodeResult::kv:
                        fields_.insert(ICaseMap::value_type(parse_buf_, parse_buf2_));
                        break;
                    default:
                        break;
                }

                parse_buf_.clear();
                parse_buf2_.clear();
            }
        }

        return parse_node_->state_;
    }

    http_head::parse_engine_node::parse_engine_node(parse_engine_node* error_node, eParseState state, eNodeResult result)
        : state_(state), result_(result), debuginfo_(""), save_{}
    {
        if (!error_node) error_node = this;
        for (int i = 0; i < 256; ++i)
            next_[i] = error_node;
    }

    void http_head::parse_engine_node::set_debuginfo(const char* s)
    {
        debuginfo_ = s;
    }
    http_head::parse_engine_node* http_head::parse_engine_node::next(char c, std::string & buf, std::string & buf2)
    {
        unsigned char uc = (unsigned char)std::toupper(c);
        switch (save_[uc]) {
            case 1:
                buf += c;
                break;
            case 2:
                buf2 += c;
                break;
        }

        return next_[uc];
    }
    void http_head::parse_engine_node::link(char c, parse_engine_node* node, int save)
    {
        next_[(unsigned char)c] = node;
        save_[(unsigned char)c] = save;
    }
    void http_head::parse_engine_node::link(const char* s, parse_engine_node* node, int save)
    {
        int len = strlen(s);
        for (int i = 0; i < len; ++i)
            link(s[i], node, save);
    }
    void http_head::parse_engine_node::create_list(const char* s, parse_engine_node* start,
            parse_engine_node* end, parse_engine_node* error_node, int save)
    {
        parse_engine_node* pos = start;
        int len = strlen(s);
        for (int i = 0; i < len - 1; ++i)
        {
            if (pos->next_[(unsigned char)s[i]] == error_node) {
                parse_engine_node *node = new parse_engine_node(error_node);
                pos->link(s[i], node, save);
                pos = node;
            } else {
                pos = pos->next_[(unsigned char)s[i]];
            }
        }
        assert(pos->next_[(unsigned char)s[len - 1]] == error_node);
        pos->link(s[len - 1], end, save);
    }

    http_head::parse_engine_node* http_head::parse_root()
    {
        static parse_engine_node* root = __init_parse_root();
        return root;
    }

#define ENGINE_NODE_DEBUGGER(node) node->set_debuginfo(#node)
    http_head::parse_engine_node* http_head::__init_parse_root()
    {
        // error node
        parse_engine_node *err = new parse_engine_node(nullptr, eParseState::error);
        ENGINE_NODE_DEBUGGER(err);

        // root node
        parse_engine_node *root = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(root);

        // line end \r and \n (except first line).
        parse_engine_node *sr = new parse_engine_node(err, eParseState::indeterminate, eNodeResult::kv);
        ENGINE_NODE_DEBUGGER(sr);
        parse_engine_node *sn = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(sn);
        sr->link('\n', sn, 0);

        parse_engine_node *end_sr = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(end_sr);
        parse_engine_node *end_sn = new parse_engine_node(err, eParseState::done);
        ENGINE_NODE_DEBUGGER(end_sn);
        sn->link('\r', end_sr, 0);
        end_sr->link('\n', end_sn, 0);

        // method done.
        parse_engine_node *method = new parse_engine_node(err, eParseState::indeterminate, eNodeResult::method);
        ENGINE_NODE_DEBUGGER(method);
        method->link(" \t", method, 0);

        parse_engine_node *method_end = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(method_end);
        method_end->link(" \t", method, 0);

        parse_engine_node::create_list("GET", root, method_end, err);
        parse_engine_node::create_list("POST", root, method_end, err);
        parse_engine_node::create_list("PUT", root, method_end, err);
        parse_engine_node::create_list("DELETE", root, method_end, err);
        parse_engine_node::create_list("TRACE", root, method_end, err);
        parse_engine_node::create_list("CONNECT", root, method_end, err);
        parse_engine_node::create_list("HEAD", root, method_end, err);
        parse_engine_node::create_list("OPTIONS", root, method_end, err);

        // uri done
        parse_engine_node *uri = new parse_engine_node(err, eParseState::indeterminate, eNodeResult::uri);
        ENGINE_NODE_DEBUGGER(uri);
        uri->link(" \t", uri, 0);
        
        parse_engine_node *uri_info = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(uri_info);
        for (char c = 33; c < 127; ++c) {
            uri_info->link(c, uri_info);
            method->link(c, uri_info);
        }
        method->link(" \t", method, 0);
        method->link("\r\n", err, 0);
        uri_info->link(" \t", uri, 0);
        uri_info->link("\r\n", err, 0);

        // STATUS
        parse_engine_node *status = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(status);
        status->link(" \t", status, 0);

        parse_engine_node *status_end = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(status_end);
        status_end->link(" \t", status, 0);

        parse_engine_node::create_list("STATUS", root, status_end, err, 0);

        // status done.
        parse_engine_node *status_code = new parse_engine_node(err, eParseState::indeterminate, eNodeResult::status);
        ENGINE_NODE_DEBUGGER(status_code);
        status_code->link(" \t", status_code, 0);

        parse_engine_node *status_code_info = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(status_code_info);
        for (char c = '0'; c <= '9'; ++c) {
            status_code_info->link(c, status_code_info);
            status->link(c, status_code_info);
        }
        status_code_info->link(" \t", status_code, 0);

        // HTTP/1.1
        parse_engine_node *version_major = new parse_engine_node(err, eParseState::indeterminate, eNodeResult::ver_major);
        ENGINE_NODE_DEBUGGER(version_major);
        parse_engine_node *version_minor = new parse_engine_node(err, eParseState::indeterminate, eNodeResult::ver_minor);
        ENGINE_NODE_DEBUGGER(version_minor);
        parse_engine_node *version_major_info = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(version_major_info);
        parse_engine_node *version_minor_info = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(version_minor_info);

        // ---- HTTP/
        parse_engine_node::create_list("HTTP/", status_code, version_major_info, err, 0);
        parse_engine_node::create_list("HTTP/", uri, version_major_info, err, 0);

        // ---- version major
        for (char c = '0'; c <= '9'; ++c) {
            version_major_info->link(c, version_major_info);
        }
        version_major_info->link('.', version_major, 0);

        // ---- version minor
        for (char c = '0'; c <= '9'; ++c) {
            version_minor_info->link(c, version_minor_info);
            version_major->link(c, version_minor_info);
        }
        version_minor_info->link('\r', version_minor, 0);
        version_minor->link('\n', sn, 0);

        // key-value
        parse_engine_node *key_info = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(key_info);
        parse_engine_node *kv_space1 = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(kv_space1);
        parse_engine_node *kv_split = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(kv_split);
        parse_engine_node *kv_space2 = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(kv_space2);
        parse_engine_node *value_info = new parse_engine_node(err);
        ENGINE_NODE_DEBUGGER(value_info);

        // key
        for (char c = 33; c < 127; ++c) {
            sn->link(c, key_info);
            key_info->link(c, key_info);
        }

        // split
        key_info->link(":", kv_split, 0);
        key_info->link(" \t", kv_space1, 0);
        kv_space1->link(" \t", kv_space1, 0);
        kv_space1->link(":", kv_split, 0);
        kv_split->link(" \t", kv_space2, 0);
        kv_space2->link(" \t", kv_space2, 0);
        
        // value
        for (char c = 33; c < 127; ++c) {
            kv_space2->link(c, value_info, 2);
            kv_split->link(c, value_info, 2);
            value_info->link(c, value_info, 2);
        }
        value_info->link('\r', sr, 0);

        return root;
    }

} //namespace http

#pragma once

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <boost/algorithm/string.hpp>

namespace http
{
    class http_head
    {
    public:
        typedef std::multimap<std::string, std::string, boost::is_iless> ICaseMap;

        enum class eHttpHeadType
        {
            request,
            response,
        };

        explicit http_head(eHttpHeadType type);

        void clear();

        /// --------------------- parse ---------------------
        enum class eParseState
        {
            indeterminate,
            error,
            done
        };

        /// 一次性解析
        std::size_t parse(std::string const& s);

        /// 流式解析
        std::size_t consume(std::string const& s);
        std::size_t consume(const char* data, size_t len);

        std::string to_string() const;

        bool ok() const;
        explicit operator bool() const;
        std::string error() const;
        /// -------------------------------------------------

        /// ========= request: Method and uri ===============
        enum class eMethod
        {
            Unkown = 0,
            Get,
            Post,
            Put,
            Delete,
            Trace,
            Connect,
            Head,
            Options,
        };
        eMethod method() const;
        void set_method(eMethod mthd);

        std::string method_s() const;
        void set_method_s(std::string s);
        
        std::string uri() const;
        void set_uri(std::string s);
        /// =================================================

        /// ============ response: StatusCode ===============
        int status() const;
        void set_status(int v);
        /// =================================================

        /// -------------------- version --------------------
        int version_major() const;
        int version_minor() const;
        void set_version_major(int v);
        void set_version_minor(int v);
        /// -------------------------------------------------

        /// -------------------- fields ---------------------
        ICaseMap& fields();
        ICaseMap const& fields() const;

        std::string get(std::string key) const;

        std::size_t content_length() const;
        void set_content_length(std::size_t len);

        std::string host() const;
        void set_host(std::string s);
        /// -------------------------------------------------

    private:
        eParseState consume(char c);

        // TODO: parse engine.
        struct parse_engine_node
        {

        };

        eParseState state_;
        parse_engine_node* parse_node_;
        std::string parse_buf_;

    private:
        eHttpHeadType type_;

        // only request-type
        eMethod method_;
        std::string uri_;

        // only response-type
        int status_;

        int version_major_;
        int version_minor_;
        ICaseMap fields_;
        std::string error_;
    };

} //namespace http

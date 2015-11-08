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
        struct is_strless
        {
            template <typename S>
            inline bool operator()(S const& left, S const& right) const
            {
                if (left.size() != right.size())
                    return left.size() < right.size();

                for (size_t i = 0; i < left.size(); ++i)
                {
                    char l = std::toupper(left[i]);
                    char r = std::toupper(right[i]);
                    if (l != r)
                        return l < r;
                }

                return false;
            }
        };
        typedef std::multimap<std::string, std::string, is_strless> ICaseMap;

        enum class eHttpHeadType
        {
            indeterminate = 0,
            request,
            response,
        };

        explicit http_head(eHttpHeadType type = eHttpHeadType::request);

        void clear();

        /// --------------------- parse ---------------------
        enum class eParseState
        {
            indeterminate = 0,
            error,
            done
        };

        /// 一次性解析
        std::size_t parse(std::string const& s);

        /// 流式解析
        std::size_t consume(std::string const& s);
        std::size_t consume(const char* data, size_t len);

        std::string to_string() const;

        eHttpHeadType type() const;
        void set_type(eHttpHeadType t);
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

        /// parse engine.
        enum class eNodeResult
        {
            none,
            method,
            uri,
            status,
            ver_major,
            ver_minor,
            kv,
        };

        struct parse_engine_node
        {
            eParseState state_;
            eNodeResult result_;
            const char* debuginfo_;
            int save_[256];
            parse_engine_node* next_[256];

            explicit parse_engine_node(parse_engine_node* error_node, eParseState state = eParseState::indeterminate,
                    eNodeResult result = eNodeResult::none);
            void set_debuginfo(const char* s);
            parse_engine_node* next(char c, std::string & buf, std::string & buf2);
            void link(char c, parse_engine_node* node, int save = 1);
            void link(const char* s, parse_engine_node* node, int save = 1);
            static void create_list(const char* s, parse_engine_node* start, parse_engine_node* end, parse_engine_node* error_node, int save = 1);
        };
        parse_engine_node* parse_root();
        static parse_engine_node* __init_parse_root();

        parse_engine_node* parse_node_;
        std::string parse_buf_;
        std::string parse_buf2_;

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
    };

} //namespace http

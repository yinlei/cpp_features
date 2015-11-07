#pragma once

#include <string>

namespace http
{
    /// url: <protocol>://<domain>[:port]<path>?<data>
    class url
    {
    public:
        url();

        /// -------------------- parse ----------------------
        explicit url(std::string s);
        url& operator=(std::string s);
        std::string to_string() const;

        bool ok() const;
        explicit operator bool() const;
        std::string error() const;
        /// -------------------------------------------------

        /// ------------------- protocol --------------------
        enum class eProtocol {
            unkown = 0,
            http,
            https,
            ftp,
            mailto,
            ldap,
            file,
            news,
            gopher,
            telnet,

            // extend
            tcp,
            udp,
            zk,
        };

        eProtocol protocol() const;
        void set_protocol(eProtocol proto);

        std::string protocol_s() const;
        void set_protocol_s(std::string s);
        /// -------------------------------------------------

        /// -------------------- domain ---------------------
        std::string domain() const;
        void set_domain(std::string s);
        /// -------------------------------------------------

        /// ----------------- port/service ------------------
        uint16_t port() const;
        void set_port(uint16_t num_port);
        std::string service() const;
        /// -------------------------------------------------

        /// --------------------- path ----------------------
        std::string path() const;
        void set_path(std::string s);
        /// -------------------------------------------------

        /// --------------------- data ----------------------
        std::string data() const;
        void set_data(std::string s);
        /// -------------------------------------------------

        void clear();

    private:
        void parse(std::string s);

    private:
        // fields
        eProtocol protocol_;
        std::string domain_;
        uint16_t port_;
        std::string path_;
        std::string data_;

        // state
        std::string error_;
    };

} //namespace http

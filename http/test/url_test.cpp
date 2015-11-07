#include <http/url.h>
#include <gtest/gtest.h>
using namespace http;

TEST(URLTest, simple)
{
    {
        std::string u1 = "http://www.baidu.com:8080/index.html?abc=44&dd=xxxxx";
        url url_1(u1);
        EXPECT_EQ(url_1.protocol(), url::eProtocol::http);
        EXPECT_EQ(url_1.domain(), "www.baidu.com");
        EXPECT_EQ(url_1.port(), 8080);
        EXPECT_EQ(url_1.path(), "/index.html");
        EXPECT_EQ(url_1.data(), "abc=44&dd=xxxxx");
        EXPECT_EQ(url_1.to_string(), u1);
    }

    {
        std::string u1 = "https://www.baidu.com:8080/index.html?abc=44&dd=xxxxx";
        url url_1(u1);
        EXPECT_EQ(url_1.protocol(), url::eProtocol::https);
        EXPECT_EQ(url_1.domain(), "www.baidu.com");
        EXPECT_EQ(url_1.port(), 8080);
        EXPECT_EQ(url_1.path(), "/index.html");
        EXPECT_EQ(url_1.data(), "abc=44&dd=xxxxx");
        EXPECT_EQ(url_1.to_string(), u1);
    }

    {
        std::string u1 = "https://www.baidu.com";
        url url_1(u1);
        EXPECT_EQ(url_1.protocol(), url::eProtocol::https);
        EXPECT_EQ(url_1.domain(), "www.baidu.com");
        EXPECT_EQ(url_1.port(), 443);
        EXPECT_EQ(url_1.path(), "/");
        EXPECT_EQ(url_1.data(), "");
        EXPECT_EQ(url_1.to_string(), u1);
    }

    {
        std::string u1 = "zk://www.baidu.com:8080/index.html?abc=44&dd=xxxxx";
        url url_1(u1);
        EXPECT_EQ(url_1.protocol(), url::eProtocol::zk);
        EXPECT_EQ(url_1.domain(), "www.baidu.com");
        EXPECT_EQ(url_1.port(), 8080);
        EXPECT_EQ(url_1.path(), "/index.html?abc=44&dd=xxxxx");
        EXPECT_EQ(url_1.data(), "");
        EXPECT_EQ(url_1.to_string(), u1);
    }

    {
        std::string u1 = "zk://www.baidu.com/index.html?abc=44&dd=xxxxx";
        url url_1(u1);
        EXPECT_EQ(url_1.protocol(), url::eProtocol::zk);
        EXPECT_EQ(url_1.domain(), "www.baidu.com");
        EXPECT_EQ(url_1.port(), 2181);
        EXPECT_EQ(url_1.path(), "/index.html?abc=44&dd=xxxxx");
        EXPECT_EQ(url_1.data(), "");
        EXPECT_EQ(url_1.to_string(), u1);
    }

    {
        std::string u1 = "tcp://www.baidu.com";
        url url_1(u1);
        EXPECT_EQ(url_1.protocol(), url::eProtocol::tcp);
        EXPECT_EQ(url_1.domain(), "www.baidu.com");
        EXPECT_EQ(url_1.port(), 0);
        EXPECT_EQ(url_1.path(), "");
        EXPECT_EQ(url_1.data(), "");
        EXPECT_EQ(url_1.to_string(), u1);

        url_1.set_port(21);
        EXPECT_EQ(url_1.port(), 21);
    }

    {
        std::string u1 = "udp://www.baidu.com:21";
        url url_1(u1);
        EXPECT_EQ(url_1.protocol(), url::eProtocol::udp);
        EXPECT_EQ(url_1.domain(), "www.baidu.com");
        EXPECT_EQ(url_1.port(), 21);
        EXPECT_EQ(url_1.path(), "");
        EXPECT_EQ(url_1.data(), "");
        EXPECT_EQ(url_1.to_string(), u1);
    }
}


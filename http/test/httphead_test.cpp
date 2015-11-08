#include <gtest/gtest.h>
#include <http/http_head.h>
using namespace http;

TEST(HTTPHeadTest, simple)
{
    {
        std::string request_head = 
            "POST /test/index.html\tHttP/2.0\r\n"
            "ACCEPT: ac\r\n"
            "Content-lengTH\t:32\r\n"
            "HOst \t:\twww.baidu.com\r\n"
            "User:http_test\r\n\r\n";
        http_head head;
        std::size_t n = head.parse(request_head);
        EXPECT_EQ(n, request_head.length());
        EXPECT_EQ(head.type(), http_head::eHttpHeadType::request);

        request_head += "\r\n";
        n = head.parse(request_head);
        EXPECT_EQ(n, request_head.length() - 2);
        EXPECT_EQ(head.type(), http_head::eHttpHeadType::request);

        EXPECT_EQ(head.method(), http_head::eMethod::Post);
        EXPECT_EQ(head.uri(), "/test/index.html");
        EXPECT_EQ(head.version_major(), 2);
        EXPECT_EQ(head.version_minor(), 0);
        EXPECT_EQ(head.content_length(), 32);
        EXPECT_EQ(head.get("host"), "www.baidu.com");
        printf("%s\n", head.to_string().c_str());
    }

    {
        std::string response_head = 
            "STatus\t200 \tHttP/2.0\r\n"
            "ACCEPT: ac\r\n"
            "Content-lengTH\t:32\r\n"
            "HOst \t:\twww.baidu.com\r\n"
            "User : \thttp_test\r\n";
        http_head head;
        std::size_t n = head.parse(response_head);
        EXPECT_EQ(n, -1);
        EXPECT_EQ(head.type(), http_head::eHttpHeadType::indeterminate);

        response_head += "\r\n";
        n = head.parse(response_head);
        EXPECT_EQ(n, response_head.length());
        EXPECT_EQ(head.type(), http_head::eHttpHeadType::response);

        EXPECT_EQ(head.status(), 200);
        EXPECT_EQ(head.version_major(), 2);
        EXPECT_EQ(head.version_minor(), 0);
        EXPECT_EQ(head.content_length(), 32);
        EXPECT_EQ(head.get("host"), "www.baidu.com");
        printf("%s\n", head.to_string().c_str());
    }
}


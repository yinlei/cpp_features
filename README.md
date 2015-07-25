# cpp_features

为C++11扩展一些很cool的特性


### coroutine  - 像golang一样好用的协程库

##### 基础用法
~~~~~~~~~~cpp
#include "coroutine.h"
#include <iostream>
#include <unistd.h>
using namespace std;

void f2()
{
    cout << 2 << endl;
    yield;
    cout << 4 << endl;
    yield;
    cout << 6 << endl;
}

void f1()
{
    go f2;
    cout << 1 << endl;
    yield;
    cout << 3 << endl;
    yield;
    cout << 5 << endl;
}

int main()
{
    go f1;
    cout << "go" << endl;
    while (!g_Scheduler.IsEmpty()) {
        g_Scheduler.Run();
    }
    cout << "end" << endl;
    return 0;
}

// 输出结果
go
1
2
3
4
5
6
end
~~~~~~~~~~

##### 高级应用：将同步的第三方库变为异步模型，提升性能。（以hiredis为例)
~~~~~~~~~~cpp
#include <hiredis/hiredis.h>
#include "coroutine.h"
#include <stdio.h>
#include <memory>

void do_redis(int num)
{
    redisContext* redis_ctx = redisConnect("127.0.0.1", 6379);
    if (!redis_ctx) {
        printf("[%d] connect error.\n", num);
        return ;
    }

    if (redis_ctx->err) {
        printf("[%d] connect error %d\n", num, redis_ctx->err);
        return ;
    }

    printf("[%d] connected redis.\n", num);
    std::shared_ptr<redisContext> _ep(redis_ctx, [](redisContext* c){ redisFree(c); });

    const char* cmd_set = "set i 1";
    redisReply *reply = (redisReply*)redisCommand(redis_ctx, cmd_set);
    if (!reply) {
        printf("[%d] reply is NULL.\n", num);
        return ;
    }

    std::shared_ptr<redisReply> _ep_reply(reply, [](redisReply* reply){ freeReplyObject(reply); });

    if (!(reply->type == REDIS_REPLY_STATUS && strcasecmp(reply->str,"OK")==0)) {
        printf("[%d] execute command error.\n", num);
        return;  
    }     

    printf("[%d] execute command success.\n", num);
}

int main()
{
//    g_Scheduler.GetOptions().debug = true;
    for (int i = 0; i < 2; ++i)
    {
        go [=]{ do_redis(i); };
    }
    printf("go\n");
    while (!g_Scheduler.IsEmpty())
        g_Scheduler.Run();
    printf("end\n");
    return 0;
}

// 输出结果
go
[0] connected redis.
[1] connected redis.
[0] execute command success.
[1] execute command success.
end
~~~~~~~~~~

### multiret   - 让C++支持多返回值

~~~~~~~~~~cpp
// demo
#include <iostream>
#include <vector>
#include <list>
#include <tuple>
#include "multi_ret.h"
using namespace std;

std::vector<int> foo() {
    return {1, 2, 3};
}

std::list<double> foo2() {
    return {9, 8, 7.0};
}

std::tuple<int, double, short> foo3() {
    return std::tuple<int, double, short>{4, 5.0, 6};
}

std::pair<int, double> foo4() {
    return std::pair<int, double>{0, 0};
}

int main()
{
    int a = 0, b = 0;
    double c = 0;
    MR(a, b, c) = foo();
    cout << a << b << c << endl;

    MR(a, b, c) = foo2();
    cout << a << b << c << endl;

    MR(a, b, c) = foo3();
    cout << a << b << c << endl;

    MR(a, b) = foo4();
    cout << a << b << endl;

    return 0;
}
~~~~~~~~~~

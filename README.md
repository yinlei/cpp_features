# cpp_features

为C++11扩展一些很cool的特性


### coroutine  -协程库、并行编程库

coroutine是一个使用C++11编写的调度式stackful协程库,

同时也是一个强大的并行编程库, 是专为Linux服务端程序开发设计的底层框架。

目前支持两个平台:

    Linux   (GCC4.8+)
    
    Windows (VS2015)

使用coroutine编写并行程序，即可以像go、erlang这些并发语言一样
开发迅速且逻辑简洁，又有C++原生的性能优势，鱼和熊掌从此可以兼得。

coroutine有以下特点：
 *   1.提供不输于golang的强大协程，基于corontine编写代码，可以以同步的方式编写简单的代码，同时获得异步的性能，
 *   2.支持海量协程, 创建100万个协程只需使用1GB内存
 *   3.允许用户自由控制协程调度点，随意变更调度线程数；
 *   4.支持多线程调度协程，极易编写并行代码，高效的并行调度算法，可以有效利用多个CPU核心
 *   5.可以让链接进程序的同步的第三方库变为异步调用，大大提升其性能。
      再也不用担心某些DB官方不提供异步driver了，比如hiredis、mysqlclient这种客户端驱动可以直接使用，并且可以得到不输于异步driver的性能。
 *   6.动态链接和静态链接全都支持，便于使用C++11的用户静态链接生成可执行文件并部署至低版本的linux系统上。
 *   7.提供协程锁(co_mutex), 定时器, channel等特性, 帮助用户更加容易地编写程序. 
 *   8.网络性能强劲，超越ASIO异步模型；尤其在处理小包和多线程并行方面非常强大。
 
 *   如果你发现了任何bug、有好的建议、或使用上有不明之处，可以提交到issue，也可以直接联系作者:
      email:289633152@qq.com

 *   coroutine/samples目录下有很多示例代码，内含详细的使用说明，让用户可以循序渐进的学习coroutine库的使用方法。

##### coroutine库的链接方法：
     * 动态链接时，一定要最先链接libcoroutine.so，还需要链接libdl.so. 例如：
       g++ -std=c++11 test.cpp -lcoroutine -ldl [-lother_libs]
     * 静态链接时，只需链接libcoroutine.a即可，不要求第一个被链接，但要求libc.a最后被链接. 例如:
       g++ -std=c++11 test.cpp -lcoroutine -static -static-libgcc -static-libstdc++

##### 注意事项：
     * 1.使用多线程调度时，协程的每次切换，下一次继续执行都可能处于其他线程中，因此不能使用<线程局部变量>
     * 2.协程的调度是协作式调度，需要协程主动让出执行权，因此不要让一个代码段耗时过长，
         在耗时很多的循环中插入一些yield是一个很棒的选择
     * 3.除网络IO以外的阻塞系统调用，会真正阻塞调度线程的运行，请使用channel+线程池的策略处理.


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

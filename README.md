# cpp_features

为C++11扩展一些很cool的特性


### coroutine  -协程库、并行编程库

coroutine是一个使用C++11编写的调度式stackful协程库,
同时也是一个强大的并行编程库，只能运行在Linux-OS上, 依赖GNU-C.
是专为linux服务端程序开发设计的底层框架。

使用coroutine编写并行程序，即可以像go、erlang这些并发语言一样
开发迅速且逻辑简洁，又有C++原生的性能优势，鱼和熊掌从此可以兼得。

coroutine有以下特点：
 *   1.提供不输与golang的强大的协程
      基于corontine编写代码，可以以同步的方式
      编写简单的代码，同时获得异步的性能，
 *   2.允许用户自主控制协程调度点
 *   3.支持多线程调度协程，极易编写并行代码，高效的并行调度算法，可以有效利用多个CPU核心
 *   4.采用hook-socket函数族的方式，可以让链接
      进程序的同步的第三方库变为异步调用，大大
      提升其性能。
      再也不用担心某些DB官方不提供异步driver了，
      比如hiredis、mysqlclient这种客户端驱动
      可以直接使用，并且可以得到不输于异步
      driver的性能。
 *   5.动态链接和静态链接全都支持便于使用C++11的用户使用静态链接生成可执行文件并部署至低版本的linux系统上。
 *   6.提供协程锁(co_mutex), 定时器, channel等特性,
      帮助用户更加容易地编写程序. 
 
 *   如果你发现了任何bug、有好的建议、或使用上有不明之处，可以提交到issue，也可以直接联系作者:
      email:289633152@qq.com

 *   coroutine/samples目录下有很多示例代码，内含详细的使用说明，让用户可以循序渐进的学习coroutine库的使用方法。

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

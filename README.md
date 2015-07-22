# cpp_features

为C++11扩展一些很cool的特性


> coroutine  - 像golang一样好用的协程库

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
~~~~~~~~~~

~~~~~~~~~~cpp
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

> multiret   - 让C++支持多返回值

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

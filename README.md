# cpp_features

为C++11扩展一些很cool的特性


> coroutine  - 像golang一样好用的协程库

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

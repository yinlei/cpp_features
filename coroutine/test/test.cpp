#include "coroutine.h"
#include <iostream>
#include <unistd.h>
using namespace std;

void f2()
{
    cout << 4 << endl;
    yield;
    cout << 5 << endl;
    yield;
    cout << 6 << endl;
}

void f1()
{
    go f2;
    cout << 1 << endl;
    yield;
    cout << 2 << endl;
    yield;
    cout << 3 << endl;
}

int main()
{
    go f1;
    cout << "go" << endl;
    sleep(1);
    cout << "end" << endl;
    return 0;
}

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

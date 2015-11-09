#include "coroutine.h"

extern int co_main(int argc, char **argv);

int main(int argc, char **argv)
{
    go [=]{
        co_main(argc, argv);
    };

    co_sched.RunUntilNoTask();
    return 0;
}

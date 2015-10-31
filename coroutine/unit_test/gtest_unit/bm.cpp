#include <iostream>
#include <gtest/gtest.h>
#include "coroutine.h"
#include <chrono>
using namespace std;
using namespace co;

using ::testing::TestWithParam;
using ::testing::Values;

struct stdtimer
{
    explicit stdtimer(int count, std::string name) {
        count_ = count;
        name_ = name;
        t_ = chrono::system_clock::now();
    }
    ~stdtimer() {
        auto c = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - t_).count();
        cout << name_ << " run " << count_ << "times, cost " << (c / 1000) << " ms" << endl;
        cout << "per second op times: " << (size_t)((double)1000000  * count_ / c) << endl;
    }

    chrono::system_clock::time_point t_;
    std::string name_;
    int count_;
};

struct Times : public TestWithParam<int>
{
    int tc_;
    void SetUp() { tc_ = GetParam(); }
};

TEST_P(Times, testBm)
{
//    g_Scheduler.GetOptions().debug = dbg_sleepblock;

    {
        stdtimer st(tc_, "Create coroutine");
        for (int i = 0; i < tc_; ++i) {
            go []{};
        }
    }

    {
        stdtimer st(tc_, "Switch coroutine");
        g_Scheduler.RunUntilNoTask();
    }

//    co_chan<int> chan;
    co_chan<int> chan(tc_);
    go [=] {
        for (int i = 0; i < tc_; ++i) {
            chan << i;
        }
    };

    go [=] {
        int c;
        for (int i = 0; i < tc_; ++i)
            chan >> c;
    };

    {
        stdtimer st(tc_, "Channel");
        g_Scheduler.RunUntilNoTask();
    }

    {
        stdtimer st(tc_, "Create Timer");
        for (int i = 0; i < tc_; ++i)
            co_timer_add(std::chrono::nanoseconds(i), []{});
    }

    {
        std::vector<co_timer_id> id_list;
        id_list.reserve(tc_);
        for (int i = 0; i < tc_; ++i)
            id_list.push_back(co_timer_add(std::chrono::nanoseconds(i), []{}));

        stdtimer st(tc_, "Delete Timer");
        for (int i = 0; i < tc_; ++i)
            co_timer_cancel(id_list[i]);
    }

//    g_Scheduler.GetOptions().debug = co::dbg_timer;
    {
        bool *flag = new bool(true);
        auto id = co_timer_add(std::chrono::milliseconds(1), [=]{ *flag = false; });

        stdtimer st(tc_, "Process Timer");
        while (*flag)
            g_Scheduler.Run();
    }
}

INSTANTIATE_TEST_CASE_P(
        BmTest,
        Times,
        Values(100000));

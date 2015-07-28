#include "ts_queue.h"
#include "gtest/gtest.h"

class QueueElem : public TSQueueHook
{

};

TEST(TSQueue, DefaultContructor) {
    TSQueue<QueueElem> tsq; 
    EXPECT_EQ(NULL, tsq.pop());
}

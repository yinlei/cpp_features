#pragma once
#include <atomic>
#include <mutex>

struct LFLock
{
    volatile std::atomic_flag lck = ATOMIC_FLAG_INIT;

    inline void lock()
    {
        while (std::atomic_flag_test_and_set_explicit(&lck, std::memory_order_acquire));
    }

    inline bool try_lock()
    {
        return !std::atomic_flag_test_and_set_explicit(&lck, std::memory_order_acquire);
    }
    
    inline void unlock()
    {
        std::atomic_flag_clear_explicit(&lck, std::memory_order_release);
    }
};

struct TSQueueHook
{
    TSQueueHook *prev = NULL;
    TSQueueHook *next = NULL;
    LFLock lck;
    LFLock *list_lck;
    bool unlink()
    {
        if ((!prev && !next) || !list_lck) return false;
        std::lock_guard<LFLock> lkg(*list_lck);
        if (prev) prev->next = next;
        if (next) next->prev = prev;
        prev = next = NULL;
        list_lck = NULL;
        return true;
    }
};

template <typename T>
class TSQueue
{
    LFLock lck;
    TSQueueHook* head_;
    TSQueueHook* tail_;

public:
    TSQueue()
    {
        head_ = tail_ = new TSQueueHook;
    }

    ~TSQueue()
    {
        std::lock_guard<LFLock> lock(lck);
        while (head_ != tail_) {
            TSQueueHook *prev = tail_->prev;
            delete (T*)tail_;
            tail_ = prev;
        }
        delete head_;
        head_ = tail_ = 0;
    }

    bool empty()
    {
        return head_ == tail_;
    }

    void push(T* elem)
    {
        std::lock_guard<LFLock> lock(lck);
        TSQueueHook *hook = static_cast<TSQueueHook*>(elem);
        tail_->next = hook;
        hook->prev = tail_;
        hook->next = NULL;
        hook->list_lck = &lck;
        tail_ = hook;
    }

    T* pop()
    {
        if (head_ == tail_) return NULL;
        std::lock_guard<LFLock> lock(lck);
        if (head_ == tail_) return NULL;
        TSQueueHook* ptr = head_->next;
        if (ptr == tail_) tail_ = head_;
        head_->next = ptr->next;
        if (ptr->next) ptr->next->prev = head_;
        ptr->prev = ptr->next = NULL;
        ptr->list_lck = NULL;
        return (T*)ptr;
    }
};


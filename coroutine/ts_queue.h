#pragma once
#include <atomic>
#include <mutex>
#include <boost/operators.hpp>

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
class SList
{
public:
    struct iterator;
    struct iterator : public
        boost::equality_comparable<iterator,
        boost::unit_steppable<iterator,
        boost::dereferenceable<iterator, T*> > >
    {
        TSQueueHook* ptr;

        iterator() : ptr(NULL) {}
        explicit iterator(TSQueueHook* p) : ptr(p) {}
        friend bool operator==(iterator const& lhs, iterator const& rhs)
        { return lhs.ptr == rhs.ptr; }
        iterator& operator++() { ptr = ptr->next; return *this; }
        iterator& operator--() { ptr = ptr->prev; return *this; }
        T& operator*() { return *(T*)ptr; }
    };

    TSQueueHook* head_;
    TSQueueHook* tail_;
    void *check_;

public:
    SList() : head_(NULL), tail_(NULL), check_(NULL) {}
    SList(TSQueueHook* h, TSQueueHook* t, void *c) : head_(h), tail_(t), check_(c) {}

    iterator begin() { return iterator{head_}; }
    iterator end() { return iterator(); }
    inline bool empty() const { return head_ == NULL; }
    iterator erase(iterator it)
    {
        TSQueueHook* hook = (it++).ptr;
        if (hook->prev) hook->prev->next = hook->next;
        else head_ = head_->next;
        if (hook->next) hook->next->prev = hook->prev;
        else tail_ = tail_->prev;
        return it;
    }
    std::size_t size() const
    {
        std::size_t s = 0;
        for (TSQueueHook* pos = head_; pos; pos = pos->next, ++s) ;
        return s;
    }

    inline TSQueueHook* head() { return head_; }
    inline TSQueueHook* tail() { return tail_; }
    inline bool check(void *c) { return check_ == c; }
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

    void push(T* element)
    {
        std::lock_guard<LFLock> lock(lck);
        TSQueueHook *hook = static_cast<TSQueueHook*>(element);
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

    void push(SList<T> elements)
    {
        assert(elements.check(this));
        if (elements.empty()) return ;
        std::lock_guard<LFLock> lock(lck);
        tail_->next = elements.head();
        elements.head()->prev = tail_;
        elements.tail()->next = NULL;
        tail_ = elements.tail();
    }

    SList<T> pop(int n)
    {
        if (head_ == tail_) return SList<T>();
        std::lock_guard<LFLock> lock(lck);
        if (head_ == tail_) return SList<T>();
        TSQueueHook* first = head_->next;
        TSQueueHook* last = first;
        for (int i = 1; i < n && last->next; ++i)
            last = last->next;
        if (last == tail_) tail_ = head_;
        head_->next = last->next;
        if (last->next) last->next->prev = head_;
        first->prev = last->next = NULL;
        return SList<T>(first, last, this);
    }
};


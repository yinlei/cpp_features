#pragma once

#include <boost/version.hpp>
#include <boost/context/all.hpp>

#if (BOOST_VERSION < 105900)
#define OLD_BOOST_CONTEXT
#endif


namespace co {

#ifdef OLD_BOOST_CONTEXT
typedef struct sigaltstack {                       
    void *ss_sp;          
    int ss_flags;         
    size_t ss_size;       
} stack_t;              
#else
typedef struct sigaltstack {                       
    void **ss_sp;          
    int ss_flags;         
    size_t ss_size;       
} stack_t;              
#endif

struct ucontext_t
{
#ifdef OLD_BOOST_CONTEXT
    ::boost::context::fcontext_t *native;
#else
    ::boost::context::fcontext_t native;
#endif

    ucontext_t* uc_link;
    stack_t uc_stack;
    void* arg;
};

extern "C" {

void makecontext(ucontext_t *ucp, void (*func)(), int argc, void* argv);

int swapcontext(ucontext_t *oucp, ucontext_t *ucp);

int getcontext(ucontext_t *ucp);

} //extern "C"

} //namespace co

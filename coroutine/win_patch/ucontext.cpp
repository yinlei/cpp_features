#include "ucontext.h"

namespace co {

extern "C" {

void makecontext(ucontext_t *ucp, void (*func)(), int argc, void* argv)
{
    ucp->arg = argv;
    ucp->native = ::boost::context::make_fcontext(ucp->uc_stack.ss_sp,
            ucp->uc_stack.ss_size, (void(*)(intptr_t))func);
}

int swapcontext(ucontext_t *oucp, ucontext_t *ucp)
{
    ::boost::context::jump_fcontext(oucp->native, ucp->native, (intptr_t)ucp->arg);
    return 0;
}

int getcontext(ucontext_t *ucp)
{
    return 0;
}

} //extern "C"

} //namespace co

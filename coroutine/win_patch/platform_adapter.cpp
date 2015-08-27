#include "platform_adapter.h"
#include <windows.h>

namespace co {


	ProcesserRunGuard::ProcesserRunGuard()
	{
		ConvertThreadToFiber(NULL);
	}

	ProcesserRunGuard::~ProcesserRunGuard()
	{
		ConvertFiberToThread();
	}

} //namespace co

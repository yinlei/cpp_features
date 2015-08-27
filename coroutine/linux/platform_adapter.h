#pragma once

namespace co {

	struct ThreadLocalInfo;
	struct ProcesserRunGuard
	{
		ThreadLocalInfo *info_;
		ProcesserRunGuard(ThreadLocalInfo &info);
		~ProcesserRunGuard();
	};

} //namespace co
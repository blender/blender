#include "RecastTimer.h"

#if defined(WIN32)

// Win32
#include <windows.h>

rcTimeVal rcGetPerformanceTimer()
{
	__int64 count;
	QueryPerformanceCounter((LARGE_INTEGER*)&count);
	return count;
}

int rcGetDeltaTimeUsec(rcTimeVal start, rcTimeVal end)
{
	static __int64 freq = 0;
	if (freq == 0)
		QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
	__int64 elapsed = end - start;
	return (int)(elapsed*1000000 / freq);
}

#elif defined(__MACH__)

// OSX
#include <mach/mach_time.h>

rcTimeVal rcGetPerformanceTimer()
{
	return mach_absolute_time();
}

int rcGetDeltaTimeUsec(rcTimeVal start, rcTimeVal end)
{
	static mach_timebase_info_data_t timebaseInfo;
	if (timebaseInfo.denom == 0)
		mach_timebase_info(&timebaseInfo);
	uint64_t elapsed = end - start;
	uint64_t nanosec = elapsed * timebaseInfo.numer / timebaseInfo.denom;
	return (int)(nanosec / 1000);
}

#else

// TODO: Linux, etc

rcTimeVal rcGetPerformanceTimer()
{
	return 0;
}

int rcGetDeltaTimeUsec(rcTimeVal start, rcTimeVal end)
{
	return 0;
}

#endif
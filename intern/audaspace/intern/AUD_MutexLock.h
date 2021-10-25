#ifndef AUD_MUTEXLOCK_H
#define AUD_MUTEXLOCK_H

#include "AUD_ILockable.h"

class AUD_MutexLock
{
public:
	inline AUD_MutexLock(AUD_ILockable& lockable) :
		lockable(lockable)
	{
		lockable.lock();
	}

	inline ~AUD_MutexLock()
	{
		lockable.unlock();
	}

private:
	AUD_ILockable& lockable;
};

#endif // AUD_MUTEXLOCK_H

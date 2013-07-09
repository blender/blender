#ifndef AUD_ILOCKABLE_H
#define AUD_ILOCKABLE_H

/**
 * This class provides an interface for lockable objects.
 * The main reason for this interface is to be used with AUD_MutexLock.
 */
class AUD_ILockable
{
public:
	/**
	 * Locks the object.
	 */
	virtual void lock()=0;
	/**
	 * Unlocks the previously locked object.
	 */
	virtual void unlock()=0;
};

#endif // AUD_ILOCKABLE_H

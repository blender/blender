#ifndef BM_INLINE_C
#define BM_INLINE_C

#include "bmesh.h"

BM_INLINE int BM_TestHFlag(const void *element, const int flag)
{
	return ((BMHeader *)element)->flag & flag;
}

/*stuff for dealing with header flags*/
BM_INLINE void BM_SetHFlag(void *element, const int flag)
{
	((BMHeader *)element)->flag |= flag;
}

/*stuff for dealing with header flags*/
BM_INLINE void BM_ClearHFlag(void *element, const int flag)
{
	((BMHeader *)element)->flag &= ~flag;
}

/*stuff for dealing BM_ToggleHFlag header flags*/
BM_INLINE void BM_ToggleHFlag(void *element, const int flag)
{
	((BMHeader *)element)->flag ^= flag;
}

BM_INLINE void BM_SetIndex(void *element, const int index)
{
	((BMHeader *)element)->index = index;
}

BM_INLINE int BM_GetIndex(const void *element)
{
	return ((BMHeader *)element)->index;
}

#endif /*BM_INLINE_C*/


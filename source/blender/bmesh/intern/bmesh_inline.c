#ifndef BM_INLINE_C
#define BM_INLINE_C

#include "bmesh.h"

BM_INLINE char BM_TestHFlag(const void *element, const char hflag)
{
	return ((const BMHeader *)element)->hflag & hflag;
}

BM_INLINE void BM_SetHFlag(void *element, const char hflag)
{
	((BMHeader *)element)->hflag |= hflag;
}

BM_INLINE void BM_ClearHFlag(void *element, const char hflag)
{
	((BMHeader *)element)->hflag &= ~hflag;
}

BM_INLINE void BM_ToggleHFlag(void *element, const char hflag)
{
	((BMHeader *)element)->hflag ^= hflag;
}

BM_INLINE void BM_MergeHFlag(void *element_a, void *element_b)
{
	((BMHeader *)element_a)->hflag =
	((BMHeader *)element_b)->hflag = (((BMHeader *)element_a)->hflag |
	                                  ((BMHeader *)element_b)->hflag);
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


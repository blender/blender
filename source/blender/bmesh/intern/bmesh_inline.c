#ifndef BM_INLINE_C
#define BM_INLINE_C

#include "bmesh.h"

BM_INLINE int BM_TestHFlag(void *element, int flag)
{
	BMHeader *e = element;
	return e->flag & flag;
}

/*stuff for dealing with header flags*/
BM_INLINE void BM_SetHFlag(void *element, int flag)
{
	BMHeader *e = element;
	e->flag |= flag;
}

/*stuff for dealing with header flags*/
BM_INLINE void BM_ClearHFlag(void *element, int flag)
{
	BMHeader *e = element;
	e->flag &= ~flag;
}

/*stuff for dealing BM_ToggleHFlag header flags*/
BM_INLINE void BM_ToggleHFlag(void *element, int flag)
{
	BMHeader *e = element;
	e->flag ^= flag;
}

BM_INLINE void BMINDEX_SET(void *element, int index)
{
	BMHeader *e = element;
	e->index = index;
}

BM_INLINE int BMINDEX_GET(void *element)
{
	BMHeader *e = element;
	return e->index;
}

#endif /*BM_INLINE_C*/


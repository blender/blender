/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include "btAlignedAllocator.h"


#if defined (BT_HAS_ALIGNED_ALOCATOR)

#include <malloc.h>
void*	btAlignedAlloc	(int size, int alignment)
{
	return _aligned_malloc(size,alignment);
}

void	btAlignedFree	(void* ptr)
{
	_aligned_free(ptr);
}

#else

#ifdef __CELLOS_LV2__

#include <stdlib.h>

int numAllocs = 0;
int numFree = 0;

void*	btAlignedAlloc	(int size, int alignment)
{
	numAllocs++;
	return memalign(alignment, size);
}

void	btAlignedFree	(void* ptr)
{
	numFree++;
	free(ptr);
}

#else
///todo
///will add some multi-platform version that works without _aligned_malloc/_aligned_free

void*	btAlignedAlloc	(int size, int alignment)
{
	return new char[size];
}

void	btAlignedFree	(void* ptr)
{
	delete [] (char*) ptr;
}
#endif //

#endif



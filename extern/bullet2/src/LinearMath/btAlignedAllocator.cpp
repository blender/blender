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

int gNumAlignedAllocs = 0;
int gNumAlignedFree = 0;
int gTotalBytesAlignedAllocs = 0;//detect memory leaks

#ifdef BT_DEBUG_MEMORY_ALLOCATIONS
//this generic allocator provides the total allocated number of bytes
#include <stdio.h>

void*   btAlignedAllocInternal  (size_t size, int alignment,int line,char* filename)
{
 void *ret;
 char *real;
 unsigned long offset;

 gTotalBytesAlignedAllocs += size;
 gNumAlignedAllocs++;

 printf("allocation#%d from %s,line %d, size %d\n",gNumAlignedAllocs,filename,line,size);
 real = (char *)malloc(size + 2*sizeof(void *) + (alignment-1));
 if (real) {
   offset = (alignment - (unsigned long)(real + 2*sizeof(void *))) &
(alignment-1);
   ret = (void *)((real + 2*sizeof(void *)) + offset);
   *((void **)(ret)-1) = (void *)(real);
       *((int*)(ret)-2) = size;

 } else {
   ret = (void *)(real);//??
 }
 int* ptr = (int*)ret;
 *ptr = 12;
 return (ret);
}
#include <stdio.h>
void    btAlignedFreeInternal   (void* ptr,int line,char* filename)
{

 void* real;
 gNumAlignedFree++;

 if (ptr) {
   real = *((void **)(ptr)-1);
       int size = *((int*)(ptr)-2);
       gTotalBytesAlignedAllocs -= size;

	   printf("free #%d from %s,line %d, size %d\n",gNumAlignedFree,filename,line,size);

   free(real);
 } else
 {
	 printf("NULL ptr\n");
 }
}

#else //BT_DEBUG_MEMORY_ALLOCATIONS


#if defined (BT_HAS_ALIGNED_ALLOCATOR)





#include <malloc.h>
void*	btAlignedAllocInternal	(size_t size, int alignment)
{
	gNumAlignedAllocs++;

	void* ptr = _aligned_malloc(size,alignment);
//	printf("btAlignedAllocInternal %d, %x\n",size,ptr);
	return ptr;
}

void	btAlignedFreeInternal	(void* ptr)
{
	gNumAlignedFree++;
//	printf("btAlignedFreeInternal %x\n",ptr);
	_aligned_free(ptr);
}

#else

#ifdef __CELLOS_LV2__

#include <stdlib.h>



void*	btAlignedAllocInternal	(size_t size, int alignment)
{
	gNumAlignedAllocs++;
	return memalign(alignment, size);
}

void	btAlignedFreeInternal	(void* ptr)
{
	gNumAlignedFree++;
	free(ptr);
}

#else

void*	btAlignedAllocInternal	(size_t size, int alignment)
{
 void *ret;
  char *real;
  unsigned long offset;
 
  gNumAlignedAllocs++;

  real = (char *)malloc(size + sizeof(void *) + (alignment-1));
  if (real) {
    offset = (alignment - (unsigned long)(real + sizeof(void *))) & (alignment-1);
    ret = (void *)((real + sizeof(void *)) + offset);
    *((void **)(ret)-1) = (void *)(real);
  } else {
    ret = (void *)(real);
  }
  return (ret);
}

void	btAlignedFreeInternal	(void* ptr)
{

 void* real;
 gNumAlignedFree++;

  if (ptr) {
    real = *((void **)(ptr)-1);
    free(real);
  }
}
#endif //

#endif

#endif //BT_DEBUG_MEMORY_ALLOCATIONS


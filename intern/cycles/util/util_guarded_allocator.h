/*
 * Copyright 2011-2015 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_GUARDED_ALLOCATOR_H__
#define __UTIL_GUARDED_ALLOCATOR_H__

/* Define this in order to use Blender's guarded allocator to keep
 * track of allocated buffers, their sizes and peak memory usage.
 *
 * This is usually a bad level call, but it's really handy to keep
 * track of overall peak memory consumption during the scene
 * synchronization step.
 */
#undef WITH_BLENDER_GUARDEDALLOC

#include <memory>

#include "util_types.h"

#ifdef WITH_BLENDER_GUARDEDALLOC
#  include "../../guardedalloc/MEM_guardedalloc.h"
#endif

CCL_NAMESPACE_BEGIN

/* Internal use only. */
void util_guarded_mem_alloc(size_t n);
void util_guarded_mem_free(size_t n);

/* Guarded allocator for the use with STL. */
template <typename T>
class GuardedAllocator: public std::allocator<T> {
public:
	template<typename _Tp1>
	struct rebind {
		typedef GuardedAllocator<_Tp1> other;
	};

	T *allocate(size_t n, const void *hint = 0)
	{
		util_guarded_mem_alloc(n * sizeof(T));
#ifdef WITH_BLENDER_GUARDEDALLOC
		(void)hint;
		return (T*)MEM_mallocN(n * sizeof(T), "Cycles Alloc");
#else
		return std::allocator<T>::allocate(n, hint);
#endif
	}

	void deallocate(T *p, size_t n)
	{
		util_guarded_mem_free(n * sizeof(T));
#ifdef WITH_BLENDER_GUARDEDALLOC
		MEM_freeN((void*)p);
#else
		std::allocator<T>::deallocate(p, n);
#endif
	}

	GuardedAllocator() : std::allocator<T>() {  }
	GuardedAllocator(const GuardedAllocator &a) : std::allocator<T>(a) { }
	template <class U>
	GuardedAllocator(const GuardedAllocator<U> &a) : std::allocator<T>(a) { }
	~GuardedAllocator() { }
};

/* Get memory usage and peak from the guarded STL allocator. */
size_t util_guarded_get_mem_used(void);
size_t util_guarded_get_mem_peak(void);

CCL_NAMESPACE_END

#endif  /* __UTIL_GUARDED_ALLOCATOR_H__ */

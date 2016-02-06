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

#include <cstddef>
#include <memory>

#include "util_debug.h"
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
class GuardedAllocator {
public:
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef T *pointer;
	typedef const T *const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T value_type;

	GuardedAllocator() {}
	GuardedAllocator(const GuardedAllocator&) {}

	T *allocate(size_t n, const void *hint = 0)
	{
		util_guarded_mem_alloc(n * sizeof(T));
		(void)hint;
#ifdef WITH_BLENDER_GUARDEDALLOC
		if(n == 0) {
			return NULL;
		}
		return (T*)MEM_mallocN(n * sizeof(T), "Cycles Alloc");
#else
		return (T*)malloc(n * sizeof(T));
#endif
	}

	void deallocate(T *p, size_t n)
	{
		util_guarded_mem_free(n * sizeof(T));
		if(p != NULL) {
#ifdef WITH_BLENDER_GUARDEDALLOC
			MEM_freeN(p);
#else
			free(p);
#endif
		}
	}

	T *address(T& x) const
	{
		return &x;
	}

	const T *address(const T& x) const
	{
		return &x;
	}

	GuardedAllocator<T>& operator=(const GuardedAllocator&)
	{
		return *this;
	}

	void construct(T *p, const T& val)
	{
		new ((T *)p) T(val);
	}

	void destroy(T *p)
	{
		p->~T();
	}

	size_t max_size() const
	{
		return size_t(-1);
	}

	template <class U>
	struct rebind {
		typedef GuardedAllocator<U> other;
	};

	template <class U>
	GuardedAllocator(const GuardedAllocator<U>&) {}

	template <class U>
	GuardedAllocator& operator=(const GuardedAllocator<U>&) { return *this; }
};

/* Get memory usage and peak from the guarded STL allocator. */
size_t util_guarded_get_mem_used(void);
size_t util_guarded_get_mem_peak(void);

CCL_NAMESPACE_END

#endif  /* __UTIL_GUARDED_ALLOCATOR_H__ */

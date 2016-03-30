/*
 * Copyright 2011-2016 Blender Foundation
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

#ifndef __UTIL_STACK_ALLOCATOR_H__
#define __UTIL_STACK_ALLOCATOR_H__

#include <cstddef>
#include <memory>

#include "util_debug.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

/* Stack allocator for the use with STL. */
template <int SIZE, typename T>
class ccl_try_align(16) StackAllocator {
public:
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef T *pointer;
	typedef const T *const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T value_type;

	/* Allocator construction/destruction. */

	StackAllocator()
	: pointer_(0) {}

	StackAllocator(const StackAllocator&)
	: pointer_(0) {}

	template <class U>
	StackAllocator(const StackAllocator<SIZE, U>&)
	: pointer_(0) {}

	/* Memory allocation/deallocation. */

	T *allocate(size_t n, const void *hint = 0)
	{
		(void)hint;
		if(pointer_ + n >= SIZE) {
			assert(!"Stack allocator overallocated");
			/* NOTE: This is just a safety feature for the release builds, so
			 * we fallback to a less efficient allocation but preventing crash
			 * from happening.
			 */
			return (T*)malloc(n * sizeof(T));
		}
		T *mem = &data_[pointer_];
		pointer_ += n;
		return mem;
	}

	void deallocate(T *p, size_t /*n*/)
	{
		if(p == NULL) {
			return;
		}
		if(p < data_ || p >= data_ + SIZE) {
			/* Again this is just a safety feature for the release builds. */
			assert(!"Should never happen");
			free(p);
			return;
		}
		/* We don't support memory free for the stack allocator. */
	}

	/* Address of an reference. */

	T *address(T& x) const
	{
		return &x;
	}

	const T *address(const T& x) const
	{
		return &x;
	}

	/* Object construction/destruction. */

	void construct(T *p, const T& val)
	{
		new ((T *)p) T(val);
	}

	void destroy(T *p)
	{
		p->~T();
	}

	/* Maximum allocation size. */

	size_t max_size() const
	{
		return size_t(-1);
	}

	/* Rebind to other ype of allocator. */

	template <class U>
	struct rebind {
		typedef StackAllocator<SIZE, U> other;
	};

	/* Operators */

	template <class U>
	inline StackAllocator& operator=(const StackAllocator<SIZE, U>&)
	{
		return *this;
	}

	StackAllocator<SIZE, T>& operator=(const StackAllocator&)
	{
		return *this;
	}

	inline bool operator==(StackAllocator const& /*other*/) const
	{
		return true;
	}

	inline bool operator!=(StackAllocator const& other) const
	{
		return !operator==(other);
	}

private:
	int pointer_;
	T data_[SIZE];
};

CCL_NAMESPACE_END

#endif  /* __UTIL_GUARDED_ALLOCATOR_H__ */

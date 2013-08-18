/*
 * Copyright 2011-2013 Blender Foundation
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
 * limitations under the License
 */

#ifndef __UTIL_VECTOR_H__
#define __UTIL_VECTOR_H__

/* Vector */

#include <string.h>
#include <vector>

#include "util_types.h"

CCL_NAMESPACE_BEGIN

using std::vector;

static inline void *malloc_aligned(size_t size, size_t alignment)
{
	void *data = (void*)malloc(size + sizeof(void*) + alignment - 1);

	union { void *ptr; size_t offset; } u;
	u.ptr = (char*)data + sizeof(void*);
	u.offset = (u.offset + alignment - 1) & ~(alignment - 1);
	*(((void**)u.ptr) - 1) = data;

	return u.ptr;
}

static inline void free_aligned(void *ptr)
{
	if(ptr) {
		void *data = *(((void**)ptr) - 1);
		free(data);
	}
}

/* Array
 *
 * Simplified version of vector, serving multiple purposes:
 * - somewhat faster in that it does not clear memory on resize/alloc,
 *   this was actually showing up in profiles quite significantly. it
 *   also does not run any constructors/destructors
 * - if this is used, we are not tempted to use inefficient operations
 * - aligned allocation for SSE data types */

template<typename T, size_t alignment = 16>
class array
{
public:
	array()
	{
		data = NULL;
		datasize = 0;
	}

	array(size_t newsize)
	{
		if(newsize == 0) {
			data = NULL;
			datasize = 0;
		}
		else {
			data = (T*)malloc_aligned(sizeof(T)*newsize, alignment);
			datasize = newsize;
		}
	}

	array(const array& from)
	{
		*this = from;
	}

	array& operator=(const array& from)
	{
		if(from.datasize == 0) {
			data = NULL;
			datasize = 0;
		}
		else {
			data = (T*)malloc_aligned(sizeof(T)*from.datasize, alignment);
			memcpy(data, from.data, from.datasize*sizeof(T));
			datasize = from.datasize;
		}

		return *this;
	}

	array& operator=(const vector<T>& from)
	{
		datasize = from.size();
		data = NULL;

		if(datasize > 0) {
			data = (T*)malloc_aligned(sizeof(T)*datasize, alignment);
			memcpy(data, &from[0], datasize*sizeof(T));
			free_aligned(data);
			data = (T*)malloc_aligned(sizeof(T)*datasize, alignment);
			memcpy(data, &from[0], datasize*sizeof(T));
		}

		return *this;
	}

	~array()
	{
		free_aligned(data);
	}

	void resize(size_t newsize)
	{
		if(newsize == 0) {
			clear();
		}
		else if(newsize != datasize) {
			T *newdata = (T*)malloc_aligned(sizeof(T)*newsize, alignment);
			memcpy(newdata, data, ((datasize < newsize)? datasize: newsize)*sizeof(T));
			free_aligned(data);

			data = newdata;
			datasize = newsize;
		}
	}

	void clear()
	{
		free_aligned(data);
		data = NULL;
		datasize = 0;
	}

	size_t size() const
	{
		return datasize;
	}

	T& operator[](size_t i) const
	{
		return data[i];
	}

protected:
	T *data;
	size_t datasize;
};

CCL_NAMESPACE_END

#endif /* __UTIL_VECTOR_H__ */


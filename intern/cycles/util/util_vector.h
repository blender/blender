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
 * limitations under the License.
 */

#ifndef __UTIL_VECTOR_H__
#define __UTIL_VECTOR_H__

/* Vector */

#include <string.h>
#include <vector>

#include "util_aligned_malloc.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

using std::vector;

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
			data = (T*)util_aligned_malloc(sizeof(T)*newsize, alignment);
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
			data = (T*)util_aligned_malloc(sizeof(T)*from.datasize, alignment);
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
			data = (T*)util_aligned_malloc(sizeof(T)*datasize, alignment);
			memcpy(data, &from[0], datasize*sizeof(T));
		}

		return *this;
	}

	~array()
	{
		util_aligned_free(data);
	}

	void resize(size_t newsize)
	{
		if(newsize == 0) {
			clear();
		}
		else if(newsize != datasize) {
			T *newdata = (T*)util_aligned_malloc(sizeof(T)*newsize, alignment);
			if(data) {
				memcpy(newdata, data, ((datasize < newsize)? datasize: newsize)*sizeof(T));
				util_aligned_free(data);
			}

			data = newdata;
			datasize = newsize;
		}
	}

	void clear()
	{
		util_aligned_free(data);
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


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

#include <cassert>
#include <cstring>
#include <vector>

#include "util/util_aligned_malloc.h"
#include "util/util_guarded_allocator.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

/* Vector
 *
 * Own subclass-ed vestion of std::vector. Subclass is needed because:
 *
 * - Use own allocator which keeps track of used/peak memory.
 *
 * - Have method to ensure capacity is re-set to 0.
 */
template<typename value_type,
         typename allocator_type = GuardedAllocator<value_type> >
class vector : public std::vector<value_type, allocator_type>
{
public:
	/* Default constructor. */
	explicit vector() : std::vector<value_type, allocator_type>() {  }

	/* Fill constructor. */
	explicit vector(size_t n, const value_type& val = value_type())
		: std::vector<value_type, allocator_type>(n, val) {  }

	/* Range constructor. */
	template <class InputIterator>
	vector(InputIterator first, InputIterator last)
		: std::vector<value_type, allocator_type>(first, last) {  }

	/* Copy constructor. */
	vector(const vector &x) : std::vector<value_type, allocator_type>(x) {  }

	void shrink_to_fit(void)
	{
#if __cplusplus < 201103L
		vector<value_type>().swap(*this);
#else
		std::vector<value_type, allocator_type>::shrink_to_fit();
#endif
	}

	void free_memory(void)
	{
		std::vector<value_type, allocator_type>::resize(0);
		shrink_to_fit();
	}

	/* Some external API might demand working with std::vector. */
	operator std::vector<value_type>()
	{
		return std::vector<value_type>(this->begin(), this->end());
	}
};

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
	: data_(NULL),
	  datasize_(0),
	  capacity_(0)
	{}

	explicit array(size_t newsize)
	{
		if(newsize == 0) {
			data_ = NULL;
			datasize_ = 0;
			capacity_ = 0;
		}
		else {
			data_ = mem_allocate(newsize);
			datasize_ = newsize;
			capacity_ = datasize_;
		}
	}

	array(const array& from)
	{
		if(from.datasize_ == 0) {
			data_ = NULL;
			datasize_ = 0;
			capacity_ = 0;
		}
		else {
			data_ = mem_allocate(from.datasize_);
			memcpy(data_, from.data_, from.datasize_*sizeof(T));
			datasize_ = from.datasize_;
			capacity_ = datasize_;
		}
	}

	array& operator=(const array& from)
	{
		if(this != &from) {
			resize(from.size());
			memcpy(data_, from.data_, datasize_*sizeof(T));
		}

		return *this;
	}

	array& operator=(const vector<T>& from)
	{
		resize(from.size());

		if(from.size() > 0) {
			memcpy(data_, &from[0], datasize_*sizeof(T));
		}

		return *this;
	}

	~array()
	{
		mem_free(data_, capacity_);
	}

	bool operator==(const array<T>& other) const
	{
		if(datasize_ != other.datasize_) {
			return false;
		}

		return memcmp(data_, other.data_, datasize_*sizeof(T)) == 0;
	}

	void steal_data(array& from)
	{
		if(this != &from) {
			clear();

			data_ = from.data_;
			datasize_ = from.datasize_;
			capacity_ = from.capacity_;

			from.data_ = NULL;
			from.datasize_ = 0;
			from.capacity_ = 0;
		}
	}

	T* resize(size_t newsize)
	{
		if(newsize == 0) {
			clear();
		}
		else if(newsize != datasize_) {
			if(newsize > capacity_) {
				T *newdata = mem_allocate(newsize);
				if(newdata == NULL) {
					/* Allocation failed, likely out of memory. */
					clear();
					return NULL;
				}
				else if(data_ != NULL) {
					memcpy(newdata, data_, ((datasize_ < newsize)? datasize_: newsize)*sizeof(T));
					mem_free(data_, capacity_);
				}
				data_ = newdata;
				capacity_ = newsize;
			}
			datasize_ = newsize;
		}
		return data_;
	}

	void clear()
	{
		if(data_ != NULL) {
			mem_free(data_, capacity_);
			data_ = NULL;
		}
		datasize_ = 0;
		capacity_ = 0;
	}

	size_t empty() const
	{
		return datasize_ == 0;
	}

	size_t size() const
	{
		return datasize_;
	}

	T* data()
	{
		return data_;
	}

	const T* data() const
	{
		return data_;
	}

	T& operator[](size_t i) const
	{
		assert(i < datasize_);
		return data_[i];
	}

	void reserve(size_t newcapacity)
	{
		if(newcapacity > capacity_) {
			T *newdata = mem_allocate(newcapacity);
			if(data_ != NULL) {
				memcpy(newdata, data_, ((datasize_ < newcapacity)? datasize_: newcapacity)*sizeof(T));
				mem_free(data_, capacity_);
			}
			data_ = newdata;
			capacity_ = newcapacity;
		}
	}

	size_t capacity() const
	{
		return capacity_;
	}

	// do not use this method unless you are sure the code is not performance critical
	void push_back_slow(const T& t)
	{
		if(capacity_ == datasize_)
		{
			reserve(datasize_ == 0 ? 1 : (size_t)((datasize_ + 1) * 1.2));
		}

		data_[datasize_++] = t;
	}

	void push_back_reserved(const T& t)
	{
		assert(datasize_ < capacity_);
		push_back_slow(t);
	}

protected:
	inline T* mem_allocate(size_t N)
	{
		if(N == 0) {
			return NULL;
		}
		T *mem = (T*)util_aligned_malloc(sizeof(T)*N, alignment);
		if(mem != NULL) {
			util_guarded_mem_alloc(sizeof(T)*N);
		}
		else {
			throw std::bad_alloc();
		}
		return mem;
	}

	inline void mem_free(T *mem, size_t N)
	{
		if(mem != NULL) {
			util_guarded_mem_free(sizeof(T)*N);
			util_aligned_free(mem);
		}
	}

	T *data_;
	size_t datasize_;
	size_t capacity_;
};

CCL_NAMESPACE_END

#endif /* __UTIL_VECTOR_H__ */


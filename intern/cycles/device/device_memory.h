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

#ifndef __DEVICE_MEMORY_H__
#define __DEVICE_MEMORY_H__

/* Device Memory
 *
 * This file defines data types that can be used in device memory arrays, and
 * a device_vector<T> type to store such arrays.
 *
 * device_vector<T> contains an STL vector, metadata about the data type,
 * dimensions, elements, and a device pointer. For the CPU device this is just
 * a pointer to the STL vector data, as no copying needs to take place. For
 * other devices this is a pointer to device memory, where we will copy memory
 * to and from. */

#include "util/util_debug.h"
#include "util/util_half.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Device;

enum MemoryType {
	MEM_READ_ONLY,
	MEM_WRITE_ONLY,
	MEM_READ_WRITE
};

/* Supported Data Types */

enum DataType {
	TYPE_UCHAR,
	TYPE_UINT,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_HALF,
	TYPE_UINT64,
};

static inline size_t datatype_size(DataType datatype) 
{
	switch(datatype) {
		case TYPE_UCHAR: return sizeof(uchar);
		case TYPE_FLOAT: return sizeof(float);
		case TYPE_UINT: return sizeof(uint);
		case TYPE_INT: return sizeof(int);
		case TYPE_HALF: return sizeof(half);
		case TYPE_UINT64: return sizeof(uint64_t);
		default: return 0;
	}
}

/* Traits for data types */

template<typename T> struct device_type_traits {
	static const DataType data_type = TYPE_UCHAR;
	static const int num_elements = 0;
};

template<> struct device_type_traits<uchar> {
	static const DataType data_type = TYPE_UCHAR;
	static const int num_elements = 1;
};

template<> struct device_type_traits<uchar2> {
	static const DataType data_type = TYPE_UCHAR;
	static const int num_elements = 2;
};

template<> struct device_type_traits<uchar3> {
	static const DataType data_type = TYPE_UCHAR;
	static const int num_elements = 3;
};

template<> struct device_type_traits<uchar4> {
	static const DataType data_type = TYPE_UCHAR;
	static const int num_elements = 4;
};

template<> struct device_type_traits<uint> {
	static const DataType data_type = TYPE_UINT;
	static const int num_elements = 1;
};

template<> struct device_type_traits<uint2> {
	static const DataType data_type = TYPE_UINT;
	static const int num_elements = 2;
};

template<> struct device_type_traits<uint3> {
	static const DataType data_type = TYPE_UINT;
	static const int num_elements = 3;
};

template<> struct device_type_traits<uint4> {
	static const DataType data_type = TYPE_UINT;
	static const int num_elements = 4;
};

template<> struct device_type_traits<int> {
	static const DataType data_type = TYPE_INT;
	static const int num_elements = 1;
};

template<> struct device_type_traits<int2> {
	static const DataType data_type = TYPE_INT;
	static const int num_elements = 2;
};

template<> struct device_type_traits<int3> {
	static const DataType data_type = TYPE_INT;
	static const int num_elements = 3;
};

template<> struct device_type_traits<int4> {
	static const DataType data_type = TYPE_INT;
	static const int num_elements = 4;
};

template<> struct device_type_traits<float> {
	static const DataType data_type = TYPE_FLOAT;
	static const int num_elements = 1;
};

template<> struct device_type_traits<float2> {
	static const DataType data_type = TYPE_FLOAT;
	static const int num_elements = 2;
};

template<> struct device_type_traits<float3> {
	static const DataType data_type = TYPE_FLOAT;
	static const int num_elements = 4;
};

template<> struct device_type_traits<float4> {
	static const DataType data_type = TYPE_FLOAT;
	static const int num_elements = 4;
};

template<> struct device_type_traits<half> {
	static const DataType data_type = TYPE_HALF;
	static const int num_elements = 1;
};

template<> struct device_type_traits<half4> {
	static const DataType data_type = TYPE_HALF;
	static const int num_elements = 4;
};

template<> struct device_type_traits<uint64_t> {
	static const DataType data_type = TYPE_UINT64;
	static const int num_elements = 1;
};

/* Device Memory */

class device_memory
{
public:
	size_t memory_size() { return data_size*data_elements*datatype_size(data_type); }
	size_t memory_elements_size(int elements) {
		return elements*data_elements*datatype_size(data_type);
	}

	/* data information */
	DataType data_type;
	int data_elements;
	device_ptr data_pointer;
	size_t data_size;
	size_t device_size;
	size_t data_width;
	size_t data_height;
	size_t data_depth;

	/* device pointer */
	device_ptr device_pointer;

	device_memory()
	{
		data_type = device_type_traits<uchar>::data_type;
		data_elements = device_type_traits<uchar>::num_elements;
		data_pointer = 0;
		data_size = 0;
		device_size = 0;
		data_width = 0;
		data_height = 0;
		data_depth = 0;
		device_pointer = 0;
	}
	virtual ~device_memory() { assert(!device_pointer); }

	void resize(size_t size)
	{
		data_size = size;
		data_width = size;
	}

protected:
	/* no copying */
	device_memory(const device_memory&);
	device_memory& operator = (const device_memory&);
};

template<typename T>
class device_only_memory : public device_memory
{
public:
	device_only_memory()
	{
		data_type = device_type_traits<T>::data_type;
		data_elements = max(device_type_traits<T>::num_elements, 1);
	}

	void resize(size_t num)
	{
		device_memory::resize(num*sizeof(T));
	}
};

/* Device Vector */

template<typename T> class device_vector : public device_memory
{
public:
	device_vector()
	{
		data_type = device_type_traits<T>::data_type;
		data_elements = device_type_traits<T>::num_elements;

		assert(data_elements > 0);
	}

	virtual ~device_vector() {}

	/* vector functions */
	T *resize(size_t width, size_t height = 0, size_t depth = 0)
	{
		data_size = width * ((height == 0)? 1: height) * ((depth == 0)? 1: depth);
		if(data.resize(data_size) == NULL) {
			clear();
			return NULL;
		}
		data_width = width;
		data_height = height;
		data_depth = depth;
		if(data_size == 0) {
			data_pointer = 0;
			return NULL;
		}
		data_pointer = (device_ptr)&data[0];
		return &data[0];
	}

	T *copy(T *ptr, size_t width, size_t height = 0, size_t depth = 0)
	{
		T *mem = resize(width, height, depth);
		if(mem != NULL) {
			memcpy(mem, ptr, memory_size());
		}
		return mem;
	}

	void copy_at(T *ptr, size_t offset, size_t size)
	{
		if(size > 0) {
			size_t mem_size = size*data_elements*datatype_size(data_type);
			memcpy(&data[0] + offset, ptr, mem_size);
		}
	}

	void reference(T *ptr, size_t width, size_t height = 0, size_t depth = 0)
	{
		data.clear();
		data_size = width * ((height == 0)? 1: height) * ((depth == 0)? 1: depth);
		data_pointer = (device_ptr)ptr;
		data_width = width;
		data_height = height;
		data_depth = depth;
	}

	void clear()
	{
		data.clear();
		data_pointer = 0;
		data_width = 0;
		data_height = 0;
		data_depth = 0;
		data_size = 0;
		device_pointer = 0;
	}

	size_t size()
	{
		return data.size();
	}

	T* get_data()
	{
		return &data[0];
	}

private:
	array<T> data;
};

/* A device_sub_ptr is a pointer into another existing memory.
 * Therefore, it is not allocated separately, but just created from the already allocated base memory.
 * It is freed automatically when it goes out of scope, which should happen before the base memory is freed.
 * Note that some devices require the offset and size of the sub_ptr to be properly aligned. */
class device_sub_ptr
{
public:
	device_sub_ptr(Device *device, device_memory& mem, int offset, int size, MemoryType type);
	~device_sub_ptr();
	/* No copying. */
	device_sub_ptr& operator = (const device_sub_ptr&);

	device_ptr operator*() const
	{
		return ptr;
	}
protected:
	Device *device;
	device_ptr ptr;
};

CCL_NAMESPACE_END

#endif /* __DEVICE_MEMORY_H__ */


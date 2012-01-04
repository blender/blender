/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "util_debug.h"
#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

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
	TYPE_FLOAT
};

static inline size_t datatype_size(DataType datatype) 
{
	switch(datatype) {
		case TYPE_UCHAR: return sizeof(uchar);
		case TYPE_FLOAT: return sizeof(float);
		case TYPE_UINT: return sizeof(uint);
		case TYPE_INT: return sizeof(int);
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
	static const int num_elements = 3;
};

template<> struct device_type_traits<float4> {
	static const DataType data_type = TYPE_FLOAT;
	static const int num_elements = 4;
};

/* Device Memory */

class device_memory
{
public:
	size_t memory_size() { return data_size*data_elements*datatype_size(data_type); }

	/* data information */
	DataType data_type;
	int data_elements;
	device_ptr data_pointer;
	size_t data_size;
	size_t data_width;
	size_t data_height;

	/* device pointer */
	device_ptr device_pointer;

protected:
	device_memory() {}
	virtual ~device_memory() { assert(!device_pointer); }

	/* no copying */
	device_memory(const device_memory&);
	device_memory& operator = (const device_memory&);
};

/* Device Vector */

template<typename T> class device_vector : public device_memory
{
public:
	device_vector()
	{
		data_type = device_type_traits<T>::data_type;
		data_elements = device_type_traits<T>::num_elements;
		data_pointer = 0;
		data_size = 0;
		data_width = 0;
		data_height = 0;

		assert(data_elements > 0);

		device_pointer = 0;
	}

	virtual ~device_vector() {}

	/* vector functions */
	T *resize(size_t width, size_t height = 0)
	{
		data_size = (height == 0)? width: width*height;
		data.resize(data_size);
		data_pointer = (device_ptr)&data[0];
		data_width = width;
		data_height = height;

		return &data[0];
	}

	T *copy(T *ptr, size_t width, size_t height = 0)
	{
		T *mem = resize(width, height);
		memcpy(mem, ptr, memory_size());
		return mem;
	}

	void reference(T *ptr, size_t width, size_t height = 0)
	{
		data.clear();
		data_size = (height == 0)? width: width*height;
		data_pointer = (device_ptr)ptr;
		data_width = width;
		data_height = height;
	}

	void clear()
	{
		data.clear();
		data_pointer = 0;
		data_width = 0;
		data_height = 0;
		data_size = 0;
	}

	size_t size()
	{
		return data.size();
	}

private:
	array<T> data;
	bool referenced;
};

CCL_NAMESPACE_END

#endif /* __DEVICE_MEMORY_H__ */


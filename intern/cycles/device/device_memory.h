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
 * Data types for allocating, copying and freeing device memory. */

#include "util/util_half.h"
#include "util/util_texture.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Device;

enum MemoryType {
	MEM_READ_ONLY,
	MEM_READ_WRITE,
	MEM_DEVICE_ONLY,
	MEM_TEXTURE,
	MEM_PIXELS
};

/* Supported Data Types */

enum DataType {
	TYPE_UNKNOWN,
	TYPE_UCHAR,
	TYPE_UINT16,
	TYPE_UINT,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_HALF,
	TYPE_UINT64,
};

static inline size_t datatype_size(DataType datatype)
{
	switch(datatype) {
		case TYPE_UNKNOWN: return 1;
		case TYPE_UCHAR: return sizeof(uchar);
		case TYPE_FLOAT: return sizeof(float);
		case TYPE_UINT: return sizeof(uint);
		case TYPE_UINT16: return sizeof(uint16_t);
		case TYPE_INT: return sizeof(int);
		case TYPE_HALF: return sizeof(half);
		case TYPE_UINT64: return sizeof(uint64_t);
		default: return 0;
	}
}

/* Traits for data types */

template<typename T> struct device_type_traits {
	static const DataType data_type = TYPE_UNKNOWN;
	static const int num_elements = sizeof(T);
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

template<> struct device_type_traits<ushort4> {
	static const DataType data_type = TYPE_UINT16;
	static const int num_elements = 4;
};

template<> struct device_type_traits<uint16_t> {
	static const DataType data_type = TYPE_UINT16;
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

/* Device Memory
 *
 * Base class for all device memory. This should not be allocated directly,
 * instead the appropriate subclass can be used. */

class device_memory
{
public:
	size_t memory_size() { return data_size*data_elements*datatype_size(data_type); }
	size_t memory_elements_size(int elements) {
		return elements*data_elements*datatype_size(data_type);
	}

	/* Data information. */
	DataType data_type;
	int data_elements;
	size_t data_size;
	size_t device_size;
	size_t data_width;
	size_t data_height;
	size_t data_depth;
	MemoryType type;
	const char *name;
	InterpolationType interpolation;
	ExtensionType extension;

	/* Pointers. */
	Device *device;
	device_ptr device_pointer;
	void *host_pointer;
	void *shared_pointer;

	virtual ~device_memory();

	void swap_device(Device *new_device, size_t new_device_size, device_ptr new_device_ptr);
	void restore_device();

protected:
	friend class CUDADevice;

	/* Only create through subclasses. */
	device_memory(Device *device, const char *name, MemoryType type);

	/* No copying allowed. */
	device_memory(const device_memory&);
	device_memory& operator = (const device_memory&);

	/* Host allocation on the device. All host_pointer memory should be
	 * allocated with these functions, for devices that support using
	 * the same pointer for host and device. */
	void *host_alloc(size_t size);
	void host_free();

	/* Device memory allocation and copying. */
	void device_alloc();
	void device_free();
	void device_copy_to();
	void device_copy_from(int y, int w, int h, int elem);
	void device_zero();

	device_ptr original_device_ptr;
	size_t original_device_size;
	Device *original_device;
};

/* Device Only Memory
 *
 * Working memory only needed by the device, with no corresponding allocation
 * on the host. Only used internally in the device implementations. */

template<typename T>
class device_only_memory : public device_memory
{
public:
	device_only_memory(Device *device, const char *name)
	: device_memory(device, name, MEM_DEVICE_ONLY)
	{
		data_type = device_type_traits<T>::data_type;
		data_elements = max(device_type_traits<T>::num_elements, 1);
	}

	virtual ~device_only_memory()
	{
		free();
	}

	void alloc_to_device(size_t num, bool shrink_to_fit = true)
	{
		size_t new_size = num;
		bool reallocate;

		if(shrink_to_fit) {
			reallocate = (data_size != new_size);
		}
		else {
			reallocate = (data_size < new_size);
		}

		if(reallocate) {
			device_free();
			data_size = new_size;
			device_alloc();
		}
	}

	void free()
	{
		device_free();
		data_size = 0;
	}

	void zero_to_device()
	{
		device_zero();
	}
};

/* Device Vector
 *
 * Data vector to exchange data between host and device. Memory will be
 * allocated on the host first with alloc() and resize, and then filled
 * in and copied to the device with copy_to_device(). Or alternatively
 * allocated and set to zero on the device with zero_to_device().
 *
 * When using memory type MEM_TEXTURE, a pointer to this memory will be
 * automatically attached to kernel globals, using the provided name
 * matching an entry in kernel_textures.h. */

template<typename T> class device_vector : public device_memory
{
public:
	device_vector(Device *device, const char *name, MemoryType type)
	: device_memory(device, name, type)
	{
		data_type = device_type_traits<T>::data_type;
		data_elements = device_type_traits<T>::num_elements;

		assert(data_elements > 0);
	}

	virtual ~device_vector()
	{
		free();
	}

	/* Host memory allocation. */
	T *alloc(size_t width, size_t height = 0, size_t depth = 0)
	{
		size_t new_size = size(width, height, depth);

		if(new_size != data_size) {
			device_free();
			host_free();
			host_pointer = host_alloc(sizeof(T)*new_size);
			assert(device_pointer == 0);
		}

		data_size = new_size;
		data_width = width;
		data_height = height;
		data_depth = depth;

		return data();
	}

	/* Host memory resize. Only use this if the original data needs to be
	 * preserved, it is faster to call alloc() if it can be discarded. */
	T *resize(size_t width, size_t height = 0, size_t depth = 0)
	{
		size_t new_size = size(width, height, depth);

		if(new_size != data_size) {
			void *new_ptr = host_alloc(sizeof(T)*new_size);

			if(new_size && data_size) {
				size_t min_size = ((new_size < data_size)? new_size: data_size);
				memcpy((T*)new_ptr, (T*)host_pointer, sizeof(T)*min_size);
			}

			device_free();
			host_free();
			host_pointer = new_ptr;
			assert(device_pointer == 0);
		}

		data_size = new_size;
		data_width = width;
		data_height = height;
		data_depth = depth;

		return data();
	}

	/* Take over data from an existing array. */
	void steal_data(array<T>& from)
	{
		device_free();
		host_free();

		data_size = from.size();
		data_width = 0;
		data_height = 0;
		data_depth = 0;
		host_pointer = from.steal_pointer();
		assert(device_pointer == 0);
	}

	/* Free device and host memory. */
	void free()
	{
		device_free();
		host_free();

		data_size = 0;
		data_width = 0;
		data_height = 0;
		data_depth = 0;
		host_pointer = 0;
		assert(device_pointer == 0);
	}

	size_t size()
	{
		return data_size;
	}

	T* data()
	{
		return (T*)host_pointer;
	}

	T& operator[](size_t i)
	{
		assert(i < data_size);
		return data()[i];
	}

	void copy_to_device()
	{
		device_copy_to();
	}

	void copy_from_device(int y, int w, int h)
	{
		device_copy_from(y, w, h, sizeof(T));
	}

	void zero_to_device()
	{
		device_zero();
	}

protected:
	size_t size(size_t width, size_t height, size_t depth)
	{
		return width * ((height == 0)? 1: height) * ((depth == 0)? 1: depth);
	}
};

/* Pixel Memory
 *
 * Device memory to efficiently draw as pixels to the screen in interactive
 * rendering. Only copying pixels from the device is supported, not copying to. */

template<typename T> class device_pixels : public device_vector<T>
{
public:
	device_pixels(Device *device, const char *name)
	: device_vector<T>(device, name, MEM_PIXELS)
	{
	}

	void alloc_to_device(size_t width, size_t height, size_t depth = 0)
	{
		device_vector<T>::alloc(width, height, depth);

		if(!device_memory::device_pointer) {
			device_memory::device_alloc();
		}
	}

	T *copy_from_device(int y, int w, int h)
	{
		device_memory::device_copy_from(y, w, h, sizeof(T));
		return device_vector<T>::data();
	}
};

/* Device Sub Memory
 *
 * Pointer into existing memory. It is not allocated separately, but created
 * from an already allocated base memory. It is freed automatically when it
 * goes out of scope, which should happen before base memory is freed.
 *
 * Note: some devices require offset and size of the sub_ptr to be properly
 * aligned to device->mem_address_alingment(). */

class device_sub_ptr
{
public:
	device_sub_ptr(device_memory& mem, int offset, int size);
	~device_sub_ptr();

	device_ptr operator*() const
	{
		return ptr;
	}

protected:
	/* No copying. */
	device_sub_ptr& operator = (const device_sub_ptr&);

	Device *device;
	device_ptr ptr;
};

CCL_NAMESPACE_END

#endif /* __DEVICE_MEMORY_H__ */

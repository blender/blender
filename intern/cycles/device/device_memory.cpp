/*
 * Copyright 2011-2017 Blender Foundation
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

#include "device/device.h"
#include "device/device_memory.h"

CCL_NAMESPACE_BEGIN

/* Device Memory */

device_memory::device_memory(Device *device, const char *name, MemoryType type)
: data_type(device_type_traits<uchar>::data_type),
  data_elements(device_type_traits<uchar>::num_elements),
  data_size(0),
  device_size(0),
  data_width(0),
  data_height(0),
  data_depth(0),
  type(type),
  name(name),
  interpolation(INTERPOLATION_NONE),
  extension(EXTENSION_REPEAT),
  device(device),
  device_pointer(0),
  host_pointer(0),
  shared_pointer(0)
{
}

device_memory::~device_memory()
{
}

void *device_memory::host_alloc(size_t size)
{
	if(!size) {
		return 0;
	}

	void *ptr = util_aligned_malloc(size, MIN_ALIGNMENT_CPU_DATA_TYPES);

	if(ptr) {
		util_guarded_mem_alloc(size);
	}
	else {
		throw std::bad_alloc();
	}

	return ptr;
}

void device_memory::host_free()
{
	if(host_pointer) {
		util_guarded_mem_free(memory_size());
		util_aligned_free((void*)host_pointer);
		host_pointer = 0;
	}
}

void device_memory::device_alloc()
{
	assert(!device_pointer && type != MEM_TEXTURE);
	device->mem_alloc(*this);
}

void device_memory::device_free()
{
	if(device_pointer) {
		device->mem_free(*this);
	}
}

void device_memory::device_copy_to()
{
	if(host_pointer) {
		device->mem_copy_to(*this);
	}
}

void device_memory::device_copy_from(int y, int w, int h, int elem)
{
	assert(type != MEM_TEXTURE && type != MEM_READ_ONLY);
	device->mem_copy_from(*this, y, w, h, elem);
}

void device_memory::device_zero()
{
	if(data_size) {
		device->mem_zero(*this);
	}
}

void device_memory::swap_device(Device *new_device,
                                size_t new_device_size,
                                device_ptr new_device_ptr)
{
	original_device = device;
	original_device_size = device_size;
	original_device_ptr = device_pointer;

	device = new_device;
	device_size = new_device_size;
	device_pointer = new_device_ptr;
}

void device_memory::restore_device()
{
	device = original_device;
	device_size = original_device_size;
	device_pointer = original_device_ptr;
}

/* Device Sub Ptr */

device_sub_ptr::device_sub_ptr(device_memory& mem, int offset, int size)
: device(mem.device)
{
	ptr = device->mem_alloc_sub_ptr(mem, offset, size);
}

device_sub_ptr::~device_sub_ptr()
{
	device->mem_free_sub_ptr(ptr);
}

CCL_NAMESPACE_END

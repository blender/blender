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

#ifdef WITH_OPENCL

#include "util/util_foreach.h"

#include "device/opencl/opencl.h"
#include "device/opencl/memory_manager.h"

CCL_NAMESPACE_BEGIN

void MemoryManager::DeviceBuffer::add_allocation(Allocation& allocation)
{
	allocations.push_back(&allocation);
}

void MemoryManager::DeviceBuffer::update_device_memory(OpenCLDeviceBase *device)
{
	bool need_realloc = false;

	/* Calculate total size and remove any freed. */
	size_t total_size = 0;

	for(int i = allocations.size()-1; i >= 0; i--) {
		Allocation* allocation = allocations[i];

		/* Remove allocations that have been freed. */
		if(!allocation->mem || allocation->mem->memory_size() == 0) {
			allocation->device_buffer = NULL;
			allocation->size = 0;

			allocations.erase(allocations.begin()+i);

			need_realloc = true;

			continue;
		}

		/* Get actual size for allocation. */
		size_t alloc_size = align_up(allocation->mem->memory_size(), 16);

		if(allocation->size != alloc_size) {
			/* Allocation is either new or resized. */
			allocation->size = alloc_size;
			allocation->needs_copy_to_device = true;

			need_realloc = true;
		}

		total_size += alloc_size;
	}

	if(need_realloc) {
		cl_ulong max_buffer_size;
		clGetDeviceInfo(device->cdDevice, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &max_buffer_size, NULL);

		if(total_size > max_buffer_size) {
			device->set_error("Scene too complex to fit in available memory.");
			return;
		}

		device_only_memory<uchar> *new_buffer =
			new device_only_memory<uchar>(device, "memory manager buffer");

		new_buffer->alloc_to_device(total_size);

		size_t offset = 0;

		foreach(Allocation* allocation, allocations) {
			if(allocation->needs_copy_to_device) {
				/* Copy from host to device. */
				opencl_device_assert(device, clEnqueueWriteBuffer(device->cqCommandQueue,
					CL_MEM_PTR(new_buffer->device_pointer),
					CL_FALSE,
					offset,
					allocation->mem->memory_size(),
					allocation->mem->host_pointer,
					0, NULL, NULL
				));

				allocation->needs_copy_to_device = false;
			}
			else {
				/* Fast copy from memory already on device. */
				opencl_device_assert(device, clEnqueueCopyBuffer(device->cqCommandQueue,
					CL_MEM_PTR(buffer->device_pointer),
					CL_MEM_PTR(new_buffer->device_pointer),
					allocation->desc.offset,
					offset,
					allocation->mem->memory_size(),
					0, NULL, NULL
				));
			}

			allocation->desc.offset = offset;
			offset += allocation->size;
		}

		delete buffer;

		buffer = new_buffer;
	}
	else {
		assert(total_size == buffer->data_size);

		size_t offset = 0;

		foreach(Allocation* allocation, allocations) {
			if(allocation->needs_copy_to_device) {
				/* Copy from host to device. */
				opencl_device_assert(device, clEnqueueWriteBuffer(device->cqCommandQueue,
					CL_MEM_PTR(buffer->device_pointer),
					CL_FALSE,
					offset,
					allocation->mem->memory_size(),
					allocation->mem->host_pointer,
					0, NULL, NULL
				));

				allocation->needs_copy_to_device = false;
			}

			offset += allocation->size;
		}
	}

	/* Not really necessary, but seems to improve responsiveness for some reason. */
	clFinish(device->cqCommandQueue);
}

void MemoryManager::DeviceBuffer::free(OpenCLDeviceBase *)
{
	buffer->free();
}

MemoryManager::DeviceBuffer* MemoryManager::smallest_device_buffer()
{
	DeviceBuffer* smallest = device_buffers;

	foreach(DeviceBuffer& device_buffer, device_buffers) {
		if(device_buffer.size < smallest->size) {
			smallest = &device_buffer;
		}
	}

	return smallest;
}

MemoryManager::MemoryManager(OpenCLDeviceBase *device)
: device(device), need_update(false)
{
	foreach(DeviceBuffer& device_buffer, device_buffers) {
		device_buffer.buffer =
			new device_only_memory<uchar>(device, "memory manager buffer");
	}
}

void MemoryManager::free()
{
	foreach(DeviceBuffer& device_buffer, device_buffers) {
		device_buffer.free(device);
	}
}

void MemoryManager::alloc(const char *name, device_memory& mem)
{
	Allocation& allocation = allocations[name];

	allocation.mem = &mem;
	allocation.needs_copy_to_device = true;

	if(!allocation.device_buffer) {
		DeviceBuffer* device_buffer = smallest_device_buffer();
		allocation.device_buffer = device_buffer;

		allocation.desc.device_buffer = device_buffer - device_buffers;

		device_buffer->add_allocation(allocation);

		device_buffer->size += mem.memory_size();
	}

	need_update = true;
}

bool MemoryManager::free(device_memory& mem)
{
	foreach(AllocationsMap::value_type& value, allocations) {
		Allocation& allocation = value.second;
		if(allocation.mem == &mem) {

			allocation.device_buffer->size -= mem.memory_size();

			allocation.mem = NULL;
			allocation.needs_copy_to_device = false;

			need_update = true;
			return true;
		}
	}

	return false;
}

MemoryManager::BufferDescriptor MemoryManager::get_descriptor(string name)
{
	update_device_memory();

	Allocation& allocation = allocations[name];
	return allocation.desc;
}

void MemoryManager::update_device_memory()
{
	if(!need_update) {
		return;
	}

	need_update = false;

	foreach(DeviceBuffer& device_buffer, device_buffers) {
		device_buffer.update_device_memory(device);
	}
}

void MemoryManager::set_kernel_arg_buffers(cl_kernel kernel, cl_uint *narg)
{
	update_device_memory();

	foreach(DeviceBuffer& device_buffer, device_buffers) {
		if(device_buffer.buffer->device_pointer) {
			device->kernel_set_args(kernel, (*narg)++, *device_buffer.buffer);
		}
		else {
			device->kernel_set_args(kernel, (*narg)++, device->null_mem);
		}
	}
}

CCL_NAMESPACE_END

#endif  /* WITH_OPENCL */

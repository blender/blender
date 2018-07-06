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

#pragma once

#include "device/device.h"

#include "util/util_map.h"
#include "util/util_vector.h"
#include "util/util_string.h"

#include "clew.h"

CCL_NAMESPACE_BEGIN

class OpenCLDeviceBase;

class MemoryManager {
public:
	static const int NUM_DEVICE_BUFFERS = 8;

	struct BufferDescriptor {
		uint device_buffer;
		cl_ulong offset;
	};

private:
	struct DeviceBuffer;

	struct Allocation {
		device_memory *mem;

		DeviceBuffer *device_buffer;
		size_t size; /* Size of actual allocation, may be larger than requested. */

		BufferDescriptor desc;

		bool needs_copy_to_device;

		Allocation() : mem(NULL), device_buffer(NULL), size(0), needs_copy_to_device(false)
		{
		}
	};

	struct DeviceBuffer {
		device_only_memory<uchar> *buffer;
		vector<Allocation*> allocations;
		size_t size; /* Size of all allocations. */

		DeviceBuffer()
		: buffer(NULL), size(0)
		{
		}

		~DeviceBuffer()
		{
			delete buffer;
			buffer = NULL;
		}

		void add_allocation(Allocation& allocation);

		void update_device_memory(OpenCLDeviceBase *device);

		void free(OpenCLDeviceBase *device);
	};

	OpenCLDeviceBase *device;

	DeviceBuffer device_buffers[NUM_DEVICE_BUFFERS];

	typedef unordered_map<string, Allocation> AllocationsMap;
	AllocationsMap allocations;

	bool need_update;

	DeviceBuffer* smallest_device_buffer();

public:
	MemoryManager(OpenCLDeviceBase *device);

	void free(); /* Free all memory. */

	void alloc(const char *name, device_memory& mem);
	bool free(device_memory& mem);

	BufferDescriptor get_descriptor(string name);

	void update_device_memory();
	void set_kernel_arg_buffers(cl_kernel kernel, cl_uint *narg);
};

CCL_NAMESPACE_END

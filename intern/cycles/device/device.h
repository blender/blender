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

#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <stdlib.h>

#include "device_memory.h"

#include "util_list.h"
#include "util_string.h"
#include "util_thread.h"
#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class Progress;

enum DeviceType {
	DEVICE_NONE,
	DEVICE_CPU,
	DEVICE_OPENCL,
	DEVICE_CUDA,
	DEVICE_NETWORK,
	DEVICE_MULTI
};

enum MemoryType {
	MEM_READ_ONLY,
	MEM_WRITE_ONLY,
	MEM_READ_WRITE
};

/* Device Task */

class DeviceTask {
public:
	typedef enum { PATH_TRACE, TONEMAP, DISPLACE } Type;
	Type type;

	int x, y, w, h;
	device_ptr rng_state;
	device_ptr rgba;
	device_ptr buffer;
	int sample;
	int resolution;
	int offset, stride;

	device_ptr displace_input;
	device_ptr displace_offset;
	int displace_x, displace_w;

	DeviceTask(Type type = PATH_TRACE);

	void split(list<DeviceTask>& tasks, int num);
	void split(ThreadQueue<DeviceTask>& tasks, int num);
	void split_max_size(list<DeviceTask>& tasks, int max_size);
};

/* Device */

class Device {
protected:
	Device() {}

	bool background;
	string error_msg;

public:
	virtual ~Device() {}

	virtual bool support_full_kernel() = 0;

	/* info */
	virtual string description() = 0;
	const string& error_message() { return error_msg; }

	/* regular memory */
	virtual void mem_alloc(device_memory& mem, MemoryType type) = 0;
	virtual void mem_copy_to(device_memory& mem) = 0;
	virtual void mem_copy_from(device_memory& mem,
		size_t offset, size_t size) = 0;
	virtual void mem_zero(device_memory& mem) = 0;
	virtual void mem_free(device_memory& mem) = 0;

	/* constant memory */
	virtual void const_copy_to(const char *name, void *host, size_t size) = 0;

	/* texture memory */
	virtual void tex_alloc(const char *name, device_memory& mem,
		bool interpolation = false, bool periodic = false) {};
	virtual void tex_free(device_memory& mem) {};

	/* pixel memory */
	virtual void pixels_alloc(device_memory& mem);
	virtual void pixels_copy_from(device_memory& mem, int y, int w, int h);
	virtual void pixels_free(device_memory& mem);

	/* open shading language, only for CPU device */
	virtual void *osl_memory() { return NULL; }

	/* load/compile kernels, must be called before adding tasks */ 
	virtual bool load_kernels(bool experimental) { return true; }

	/* tasks */
	virtual void task_add(DeviceTask& task) = 0;
	virtual void task_wait() = 0;
	virtual void task_cancel() = 0;
	
	/* opengl drawing */
	virtual void draw_pixels(device_memory& mem, int y, int w, int h,
		int width, int height, bool transparent);

#ifdef WITH_NETWORK
	/* networking */
	void server_run();
#endif

	/* static */
	static Device *create(DeviceType type, bool background = true, int threads = 0);

	static DeviceType type_from_string(const char *name);
	static string string_from_type(DeviceType type);
	static vector<DeviceType> available_types();
};

CCL_NAMESPACE_END

#endif /* __DEVICE_H__ */


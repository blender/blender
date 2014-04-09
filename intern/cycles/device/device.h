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

#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <stdlib.h>

#include "device_memory.h"
#include "device_task.h"

#include "util_list.h"
#include "util_stats.h"
#include "util_string.h"
#include "util_thread.h"
#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class Progress;
class RenderTile;

/* Device Types */

enum DeviceType {
	DEVICE_NONE,
	DEVICE_CPU,
	DEVICE_OPENCL,
	DEVICE_CUDA,
	DEVICE_NETWORK,
	DEVICE_MULTI
};

class DeviceInfo {
public:
	DeviceType type;
	string description;
	string id;
	int num;
	bool display_device;
	bool advanced_shading;
	bool pack_images;
	vector<DeviceInfo> multi_devices;

	DeviceInfo()
	{
		type = DEVICE_CPU;
		id = "CPU";
		num = 0;
		display_device = false;
		advanced_shading = true;
		pack_images = false;
	}
};

/* Device */

struct DeviceDrawParams {
	boost::function<void(void)> bind_display_space_shader_cb;
	boost::function<void(void)> unbind_display_space_shader_cb;
};

class Device {
protected:
	Device(DeviceInfo& info_, Stats &stats_, bool background) : background(background), info(info_), stats(stats_) {}

	bool background;
	string error_msg;

public:
	virtual ~Device() {}

	/* info */
	DeviceInfo info;
	virtual const string& error_message() { return error_msg; }
	bool have_error() { return !error_message().empty(); }

	/* statistics */
	Stats &stats;

	/* regular memory */
	virtual void mem_alloc(device_memory& mem, MemoryType type) = 0;
	virtual void mem_copy_to(device_memory& mem) = 0;
	virtual void mem_copy_from(device_memory& mem,
		int y, int w, int h, int elem) = 0;
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
		int dy, int width, int height, bool transparent,
		const DeviceDrawParams &draw_params);

#ifdef WITH_NETWORK
	/* networking */
	void server_run();
#endif

	/* multi device */
	virtual void map_tile(Device *sub_device, RenderTile& tile) {}
	virtual int device_number(Device *sub_device) { return 0; }

	/* static */
	static Device *create(DeviceInfo& info, Stats &stats, bool background = true);

	static DeviceType type_from_string(const char *name);
	static string string_from_type(DeviceType type);
	static vector<DeviceType>& available_types();
	static vector<DeviceInfo>& available_devices();
};

CCL_NAMESPACE_END

#endif /* __DEVICE_H__ */


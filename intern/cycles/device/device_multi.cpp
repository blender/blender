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

#include <stdlib.h>
#include <sstream>

#include "device/device.h"
#include "device/device_intern.h"
#include "device/device_network.h"

#include "render/buffers.h"

#include "util/util_foreach.h"
#include "util/util_list.h"
#include "util/util_logging.h"
#include "util/util_map.h"
#include "util/util_time.h"

CCL_NAMESPACE_BEGIN

class MultiDevice : public Device
{
public:
	struct SubDevice {
		explicit SubDevice(Device *device_)
		: device(device_) {}

		Device *device;
		map<device_ptr, device_ptr> ptr_map;
	};

	list<SubDevice> devices;
	device_ptr unique_key;

	MultiDevice(DeviceInfo& info, Stats &stats, bool background_)
	: Device(info, stats, background_), unique_key(1)
	{
		foreach(DeviceInfo& subinfo, info.multi_devices) {
			Device *device = Device::create(subinfo, sub_stats_, background);

			/* Always add CPU devices at the back since GPU devices can change
			 * host memory pointers, which CPU uses as device pointer. */
			if(subinfo.type == DEVICE_CPU) {
				devices.push_back(SubDevice(device));
			}
			else {
				devices.push_front(SubDevice(device));
			}
		}

#ifdef WITH_NETWORK
		/* try to add network devices */
		ServerDiscovery discovery(true);
		time_sleep(1.0);

		vector<string> servers = discovery.get_server_list();

		foreach(string& server, servers) {
			Device *device = device_network_create(info, stats, server.c_str());
			if(device)
				devices.push_back(SubDevice(device));
		}
#endif
	}

	~MultiDevice()
	{
		foreach(SubDevice& sub, devices)
			delete sub.device;
	}

	const string& error_message()
	{
		foreach(SubDevice& sub, devices) {
			if(sub.device->error_message() != "") {
				if(error_msg == "")
					error_msg = sub.device->error_message();
				break;
			}
		}

		return error_msg;
	}

	virtual bool show_samples() const
	{
		if(devices.size() > 1) {
			return false;
		}
		return devices.front().device->show_samples();
	}

	bool load_kernels(const DeviceRequestedFeatures& requested_features)
	{
		foreach(SubDevice& sub, devices)
			if(!sub.device->load_kernels(requested_features))
				return false;

		return true;
	}

	void mem_alloc(device_memory& mem)
	{
		device_ptr key = unique_key++;

		foreach(SubDevice& sub, devices) {
			mem.device = sub.device;
			mem.device_pointer = 0;
			mem.device_size = 0;

			sub.device->mem_alloc(mem);
			sub.ptr_map[key] = mem.device_pointer;
		}

		mem.device = this;
		mem.device_pointer = key;
		stats.mem_alloc(mem.device_size);
	}

	void mem_copy_to(device_memory& mem)
	{
		device_ptr existing_key = mem.device_pointer;
		device_ptr key = (existing_key)? existing_key: unique_key++;
		size_t existing_size = mem.device_size;

		foreach(SubDevice& sub, devices) {
			mem.device = sub.device;
			mem.device_pointer = (existing_key)? sub.ptr_map[existing_key]: 0;
			mem.device_size = existing_size;

			sub.device->mem_copy_to(mem);
			sub.ptr_map[key] = mem.device_pointer;
		}

		mem.device = this;
		mem.device_pointer = key;
		stats.mem_alloc(mem.device_size - existing_size);
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		device_ptr key = mem.device_pointer;
		int i = 0, sub_h = h/devices.size();

		foreach(SubDevice& sub, devices) {
			int sy = y + i*sub_h;
			int sh = (i == (int)devices.size() - 1)? h - sub_h*i: sub_h;

			mem.device = sub.device;
			mem.device_pointer = sub.ptr_map[key];

			sub.device->mem_copy_from(mem, sy, w, sh, elem);
			i++;
		}

		mem.device = this;
		mem.device_pointer = key;
	}

	void mem_zero(device_memory& mem)
	{
		device_ptr existing_key = mem.device_pointer;
		device_ptr key = (existing_key)? existing_key: unique_key++;
		size_t existing_size = mem.device_size;

		foreach(SubDevice& sub, devices) {
			mem.device = sub.device;
			mem.device_pointer = (existing_key)? sub.ptr_map[existing_key]: 0;
			mem.device_size = existing_size;

			sub.device->mem_zero(mem);
			sub.ptr_map[key] = mem.device_pointer;
		}

		mem.device = this;
		mem.device_pointer = key;
		stats.mem_alloc(mem.device_size - existing_size);
	}

	void mem_free(device_memory& mem)
	{
		device_ptr key = mem.device_pointer;
		size_t existing_size = mem.device_size;

		foreach(SubDevice& sub, devices) {
			mem.device = sub.device;
			mem.device_pointer = sub.ptr_map[key];
			mem.device_size = existing_size;

			sub.device->mem_free(mem);
			sub.ptr_map.erase(sub.ptr_map.find(key));
		}

		mem.device = this;
		mem.device_pointer = 0;
		mem.device_size = 0;
		stats.mem_free(existing_size);
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		foreach(SubDevice& sub, devices)
			sub.device->const_copy_to(name, host, size);
	}

	void draw_pixels(
	    device_memory& rgba, int y,
	    int w, int h, int width, int height,
	    int dx, int dy, int dw, int dh,
	    bool transparent, const DeviceDrawParams &draw_params)
	{
		device_ptr key = rgba.device_pointer;
		int i = 0, sub_h = h/devices.size();
		int sub_height = height/devices.size();

		foreach(SubDevice& sub, devices) {
			int sy = y + i*sub_h;
			int sh = (i == (int)devices.size() - 1)? h - sub_h*i: sub_h;
			int sheight = (i == (int)devices.size() - 1)? height - sub_height*i: sub_height;
			int sdy = dy + i*sub_height;
			/* adjust math for w/width */

			rgba.device_pointer = sub.ptr_map[key];
			sub.device->draw_pixels(rgba, sy, w, sh, width, sheight, dx, sdy, dw, dh, transparent, draw_params);
			i++;
		}

		rgba.device_pointer = key;
	}

	void map_tile(Device *sub_device, RenderTile& tile)
	{
		foreach(SubDevice& sub, devices) {
			if(sub.device == sub_device) {
				if(tile.buffer) tile.buffer = sub.ptr_map[tile.buffer];
			}
		}
	}

	int device_number(Device *sub_device)
	{
		int i = 0;

		foreach(SubDevice& sub, devices) {
			if(sub.device == sub_device)
				return i;
			i++;
		}

		return -1;
	}

	void map_neighbor_tiles(Device *sub_device, RenderTile *tiles)
	{
		for(int i = 0; i < 9; i++) {
			if(!tiles[i].buffers) {
				continue;
			}

			/* If the tile was rendered on another device, copy its memory to
			 * to the current device now, for the duration of the denoising task.
			 * Note that this temporarily modifies the RenderBuffers and calls
			 * the device, so this function is not thread safe. */
			device_vector<float> &mem = tiles[i].buffers->buffer;
			if(mem.device != sub_device) {
				/* Only copy from device to host once. This is faster, but
				 * also required for the case where a CPU thread is denoising
				 * a tile rendered on the GPU. In that case we have to avoid
				 * overwriting the buffer being denoised by the CPU thread. */
				if(!tiles[i].buffers->map_neighbor_copied) {
					tiles[i].buffers->map_neighbor_copied = true;
					mem.copy_from_device(0, mem.data_size, 1);
				}

				mem.swap_device(sub_device, 0, 0);

				mem.copy_to_device();
				tiles[i].buffer = mem.device_pointer;
				tiles[i].device_size = mem.device_size;

				mem.restore_device();
			}
		}
	}

	void unmap_neighbor_tiles(Device * sub_device, RenderTile * tiles)
	{
		/* Copy denoised result back to the host. */
		device_vector<float> &mem = tiles[9].buffers->buffer;
		mem.swap_device(sub_device, tiles[9].device_size, tiles[9].buffer);
		mem.copy_from_device(0, mem.data_size, 1);
		mem.restore_device();
		/* Copy denoised result to the original device. */
		mem.copy_to_device();

		for(int i = 0; i < 9; i++) {
			if(!tiles[i].buffers) {
				continue;
			}

			device_vector<float> &mem = tiles[i].buffers->buffer;
			if(mem.device != sub_device) {
				mem.swap_device(sub_device, tiles[i].device_size, tiles[i].buffer);
				sub_device->mem_free(mem);
				mem.restore_device();
			}
		}
	}

	int get_split_task_count(DeviceTask& task)
	{
		int total_tasks = 0;
		list<DeviceTask> tasks;
		task.split(tasks, devices.size());
		foreach(SubDevice& sub, devices) {
			if(!tasks.empty()) {
				DeviceTask subtask = tasks.front();
				tasks.pop_front();

				total_tasks += sub.device->get_split_task_count(subtask);
			}
		}
		return total_tasks;
	}

	void task_add(DeviceTask& task)
	{
		list<DeviceTask> tasks;
		task.split(tasks, devices.size());

		foreach(SubDevice& sub, devices) {
			if(!tasks.empty()) {
				DeviceTask subtask = tasks.front();
				tasks.pop_front();

				if(task.buffer) subtask.buffer = sub.ptr_map[task.buffer];
				if(task.rgba_byte) subtask.rgba_byte = sub.ptr_map[task.rgba_byte];
				if(task.rgba_half) subtask.rgba_half = sub.ptr_map[task.rgba_half];
				if(task.shader_input) subtask.shader_input = sub.ptr_map[task.shader_input];
				if(task.shader_output) subtask.shader_output = sub.ptr_map[task.shader_output];

				sub.device->task_add(subtask);
			}
		}
	}

	void task_wait()
	{
		foreach(SubDevice& sub, devices)
			sub.device->task_wait();
	}

	void task_cancel()
	{
		foreach(SubDevice& sub, devices)
			sub.device->task_cancel();
	}

protected:
	Stats sub_stats_;
};

Device *device_multi_create(DeviceInfo& info, Stats &stats, bool background)
{
	return new MultiDevice(info, stats, background);
}

CCL_NAMESPACE_END

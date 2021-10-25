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
	device_ptr unique_ptr;

	MultiDevice(DeviceInfo& info, Stats &stats, bool background_)
	: Device(info, stats, background_), unique_ptr(1)
	{
		Device *device;

		foreach(DeviceInfo& subinfo, info.multi_devices) {
			device = Device::create(subinfo, sub_stats_, background);
			devices.push_back(SubDevice(device));
		}

#ifdef WITH_NETWORK
		/* try to add network devices */
		ServerDiscovery discovery(true);
		time_sleep(1.0);

		vector<string> servers = discovery.get_server_list();

		foreach(string& server, servers) {
			device = device_network_create(info, stats, server.c_str());
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

	void mem_alloc(const char *name, device_memory& mem, MemoryType type)
	{
		foreach(SubDevice& sub, devices) {
			mem.device_pointer = 0;
			sub.device->mem_alloc(name, mem, type);
			sub.ptr_map[unique_ptr] = mem.device_pointer;
		}

		mem.device_pointer = unique_ptr++;
		stats.mem_alloc(mem.device_size);
	}

	void mem_copy_to(device_memory& mem)
	{
		device_ptr tmp = mem.device_pointer;

		foreach(SubDevice& sub, devices) {
			mem.device_pointer = sub.ptr_map[tmp];
			sub.device->mem_copy_to(mem);
		}

		mem.device_pointer = tmp;
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		device_ptr tmp = mem.device_pointer;
		int i = 0, sub_h = h/devices.size();

		foreach(SubDevice& sub, devices) {
			int sy = y + i*sub_h;
			int sh = (i == (int)devices.size() - 1)? h - sub_h*i: sub_h;

			mem.device_pointer = sub.ptr_map[tmp];
			sub.device->mem_copy_from(mem, sy, w, sh, elem);
			i++;
		}

		mem.device_pointer = tmp;
	}

	void mem_zero(device_memory& mem)
	{
		device_ptr tmp = mem.device_pointer;

		foreach(SubDevice& sub, devices) {
			mem.device_pointer = sub.ptr_map[tmp];
			sub.device->mem_zero(mem);
		}

		mem.device_pointer = tmp;
	}

	void mem_free(device_memory& mem)
	{
		device_ptr tmp = mem.device_pointer;
		stats.mem_free(mem.device_size);

		foreach(SubDevice& sub, devices) {
			mem.device_pointer = sub.ptr_map[tmp];
			sub.device->mem_free(mem);
			sub.ptr_map.erase(sub.ptr_map.find(tmp));
		}

		mem.device_pointer = 0;
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		foreach(SubDevice& sub, devices)
			sub.device->const_copy_to(name, host, size);
	}

	void tex_alloc(const char *name,
	               device_memory& mem,
	               InterpolationType
	               interpolation,
	               ExtensionType extension)
	{
		VLOG(1) << "Texture allocate: " << name << ", "
		        << string_human_readable_number(mem.memory_size()) << " bytes. ("
		        << string_human_readable_size(mem.memory_size()) << ")";

		foreach(SubDevice& sub, devices) {
			mem.device_pointer = 0;
			sub.device->tex_alloc(name, mem, interpolation, extension);
			sub.ptr_map[unique_ptr] = mem.device_pointer;
		}

		mem.device_pointer = unique_ptr++;
		stats.mem_alloc(mem.device_size);
	}

	void tex_free(device_memory& mem)
	{
		device_ptr tmp = mem.device_pointer;
		stats.mem_free(mem.device_size);

		foreach(SubDevice& sub, devices) {
			mem.device_pointer = sub.ptr_map[tmp];
			sub.device->tex_free(mem);
			sub.ptr_map.erase(sub.ptr_map.find(tmp));
		}

		mem.device_pointer = 0;
	}

	void pixels_alloc(device_memory& mem)
	{
		foreach(SubDevice& sub, devices) {
			mem.device_pointer = 0;
			sub.device->pixels_alloc(mem);
			sub.ptr_map[unique_ptr] = mem.device_pointer;
		}

		mem.device_pointer = unique_ptr++;
	}

	void pixels_free(device_memory& mem)
	{
		device_ptr tmp = mem.device_pointer;

		foreach(SubDevice& sub, devices) {
			mem.device_pointer = sub.ptr_map[tmp];
			sub.device->pixels_free(mem);
			sub.ptr_map.erase(sub.ptr_map.find(tmp));
		}

		mem.device_pointer = 0;
	}

	void pixels_copy_from(device_memory& mem, int y, int w, int h)
	{
		device_ptr tmp = mem.device_pointer;
		int i = 0, sub_h = h/devices.size();

		foreach(SubDevice& sub, devices) {
			int sy = y + i*sub_h;
			int sh = (i == (int)devices.size() - 1)? h - sub_h*i: sub_h;

			mem.device_pointer = sub.ptr_map[tmp];
			sub.device->pixels_copy_from(mem, sy, w, sh);
			i++;
		}

		mem.device_pointer = tmp;
	}

	void draw_pixels(device_memory& rgba, int y, int w, int h, int dx, int dy, int width, int height, bool transparent,
		const DeviceDrawParams &draw_params)
	{
		device_ptr tmp = rgba.device_pointer;
		int i = 0, sub_h = h/devices.size();
		int sub_height = height/devices.size();

		foreach(SubDevice& sub, devices) {
			int sy = y + i*sub_h;
			int sh = (i == (int)devices.size() - 1)? h - sub_h*i: sub_h;
			int sheight = (i == (int)devices.size() - 1)? height - sub_height*i: sub_height;
			int sdy = dy + i*sub_height;
			/* adjust math for w/width */

			rgba.device_pointer = sub.ptr_map[tmp];
			sub.device->draw_pixels(rgba, sy, w, sh, dx, sdy, width, sheight, transparent, draw_params);
			i++;
		}

		rgba.device_pointer = tmp;
	}

	void map_tile(Device *sub_device, RenderTile& tile)
	{
		foreach(SubDevice& sub, devices) {
			if(sub.device == sub_device) {
				if(tile.buffer) tile.buffer = sub.ptr_map[tile.buffer];
				if(tile.rng_state) tile.rng_state = sub.ptr_map[tile.rng_state];
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
			if(tiles[i].buffers->device != sub_device) {
				device_vector<float> &mem = tiles[i].buffers->buffer;

				tiles[i].buffers->copy_from_device();
				device_ptr original_ptr = mem.device_pointer;
				mem.device_pointer = 0;
				sub_device->mem_alloc("Temporary memory for neighboring tile", mem, MEM_READ_WRITE);
				sub_device->mem_copy_to(mem);
				tiles[i].buffer = mem.device_pointer;
				mem.device_pointer = original_ptr;
			}
		}
	}

	void unmap_neighbor_tiles(Device * sub_device, RenderTile * tiles)
	{
		for(int i = 0; i < 9; i++) {
			if(!tiles[i].buffers) {
				continue;
			}
			if(tiles[i].buffers->device != sub_device) {
				device_vector<float> &mem = tiles[i].buffers->buffer;

				device_ptr original_ptr = mem.device_pointer;
				mem.device_pointer = tiles[i].buffer;

				/* Copy denoised tile to the host. */
				if(i == 4) {
					tiles[i].buffers->copy_from_device(sub_device);
				}

				size_t mem_size = mem.device_size;
				sub_device->mem_free(mem);
				mem.device_pointer = original_ptr;
				mem.device_size = mem_size;

				/* Copy denoised tile to the original device. */
				if(i == 4) {
					tiles[i].buffers->device->mem_copy_to(mem);
				}
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
				if(task.shader_output_luma) subtask.shader_output_luma = sub.ptr_map[task.shader_output_luma];

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


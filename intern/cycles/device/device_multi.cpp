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

#include <stdlib.h>
#include <sstream>

#include "device.h"
#include "device_intern.h"
#include "device_network.h"

#include "buffers.h"

#include "util_foreach.h"
#include "util_list.h"
#include "util_map.h"
#include "util_time.h"

CCL_NAMESPACE_BEGIN

class MultiDevice : public Device
{
public:
	struct SubDevice {
		SubDevice(Device *device_)
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
			device = Device::create(subinfo, stats, background);
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

	bool load_kernels(bool experimental)
	{
		foreach(SubDevice& sub, devices)
			if(!sub.device->load_kernels(experimental))
				return false;

		return true;
	}

	void mem_alloc(device_memory& mem, MemoryType type)
	{
		foreach(SubDevice& sub, devices) {
			mem.device_pointer = 0;
			sub.device->mem_alloc(mem, type);
			sub.ptr_map[unique_ptr] = mem.device_pointer;
		}

		mem.device_pointer = unique_ptr++;
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

	void tex_alloc(const char *name, device_memory& mem, InterpolationType interpolation, bool periodic)
	{
		foreach(SubDevice& sub, devices) {
			mem.device_pointer = 0;
			sub.device->tex_alloc(name, mem, interpolation, periodic);
			sub.ptr_map[unique_ptr] = mem.device_pointer;
		}

		mem.device_pointer = unique_ptr++;
	}

	void tex_free(device_memory& mem)
	{
		device_ptr tmp = mem.device_pointer;

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

	void draw_pixels(device_memory& rgba, int y, int w, int h, int dy, int width, int height, bool transparent,
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
			sub.device->draw_pixels(rgba, sy, w, sh, sdy, width, sheight, transparent, draw_params);
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
};

Device *device_multi_create(DeviceInfo& info, Stats &stats, bool background)
{
	return new MultiDevice(info, stats, background);
}

static bool device_multi_add(vector<DeviceInfo>& devices, DeviceType type, bool with_display, bool with_advanced_shading, const char *id_fmt, int num)
{
	DeviceInfo info;

	/* create map to find duplicate descriptions */
	map<string, int> dupli_map;
	map<string, int>::iterator dt;
	int num_added = 0, num_display = 0;

	info.advanced_shading = with_advanced_shading;
	info.pack_images = false;

	foreach(DeviceInfo& subinfo, devices) {
		if(subinfo.type == type) {
			if(subinfo.advanced_shading != info.advanced_shading)
				continue;
			if(subinfo.display_device) {
				if(with_display)
					num_display++;
				else
					continue;
			}

			string key = subinfo.description;

			if(dupli_map.find(key) == dupli_map.end())
				dupli_map[key] = 1;
			else
				dupli_map[key]++;

			info.multi_devices.push_back(subinfo);
			if(subinfo.display_device)
				info.display_device = true;
			info.pack_images = info.pack_images || subinfo.pack_images;
			num_added++;
		}
	}

	if(num_added <= 1 || (with_display && num_display == 0))
		return false;

	/* generate string */
	stringstream desc;
	vector<string> last_tokens;
	bool first = true;

	for(dt = dupli_map.begin(); dt != dupli_map.end(); dt++) {
		if(!first) desc << " + ";
		first = false;

		/* get name and count */
		string name = dt->first;
		int count = dt->second;

		/* strip common prefixes */
		vector<string> tokens;
		string_split(tokens, dt->first);

		if(tokens.size() > 1) {
			int i;

			for(i = 0; i < tokens.size() && i < last_tokens.size(); i++)
				if(tokens[i] != last_tokens[i])
					break;

			name = "";
			for(; i < tokens.size(); i++) {
				name += tokens[i];
				if(i != tokens.size() - 1)
					name += " ";
			}
		}

		last_tokens = tokens;

		/* add */
		if(count > 1)
			desc << name << " (" << count << "x)";
		else
			desc << name;
	}

	/* add info */
	info.type = DEVICE_MULTI;
	info.description = desc.str();
	info.id = string_printf(id_fmt, num);
	info.display_device = with_display;
	info.num = 0;

	if(with_display)
		devices.push_back(info);
	else
		devices.insert(devices.begin(), info);
	
	return true;
}

void device_multi_info(vector<DeviceInfo>& devices)
{
	int num = 0;

	if(!device_multi_add(devices, DEVICE_CUDA, false, true, "CUDA_MULTI_%d", num++))
		device_multi_add(devices, DEVICE_CUDA, false, false, "CUDA_MULTI_%d", num++);
	if(!device_multi_add(devices, DEVICE_CUDA, true, true, "CUDA_MULTI_%d", num++))
		device_multi_add(devices, DEVICE_CUDA, true, false, "CUDA_MULTI_%d", num++);

	num = 0;
	if(!device_multi_add(devices, DEVICE_OPENCL, false, true, "OPENCL_MULTI_%d", num++))
		device_multi_add(devices, DEVICE_OPENCL, false, false, "OPENCL_MULTI_%d", num++);
	if(!device_multi_add(devices, DEVICE_OPENCL, true, true, "OPENCL_MULTI_%d", num++))
		device_multi_add(devices, DEVICE_OPENCL, true, false, "OPENCL_MULTI_%d", num++);
}

CCL_NAMESPACE_END


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

#include <stdlib.h>
#include <sstream>

#include "device.h"
#include "device_intern.h"
#include "device_network.h"

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

	MultiDevice(bool background_)
	: unique_ptr(1)
	{
		Device *device;

		/* add CPU device */
		device = Device::create(DEVICE_CPU, background);
		devices.push_back(SubDevice(device));

#ifdef WITH_CUDA
		/* try to add GPU device */
		device = Device::create(DEVICE_CUDA, background);
		if(device) {
			devices.push_back(SubDevice(device));
		}
		else
#endif
		{
#ifdef WITH_OPENCL
			device = Device::create(DEVICE_OPENCL, background);
			if(device)
				devices.push_back(SubDevice(device));
#endif
		}

#ifdef WITH_NETWORK
		/* try to add network devices */
		ServerDiscovery discovery(true);
		time_sleep(1.0);

		list<string> servers = discovery.get_server_list();

		foreach(string& server, servers) {
			device = device_network_create(server.c_str());
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

	bool support_full_kernel()
	{
		foreach(SubDevice& sub, devices) {
			if(!sub.device->support_full_kernel())
				return false;
		}

		return true;
	}

	string description()
	{
		/* create map to find duplicate descriptions */
		map<string, int> dupli_map;
		map<string, int>::iterator dt;

		foreach(SubDevice& sub, devices) {
			string key = sub.device->description();

			if(dupli_map.find(key) == dupli_map.end())
				dupli_map[key] = 1;
			else
				dupli_map[key]++;
		}

		/* generate string */
		stringstream desc;
		bool first = true;

		for(dt = dupli_map.begin(); dt != dupli_map.end(); dt++) {
			if(!first) desc << ", ";
			first = false;

			if(dt->second > 1)
				desc << dt->second << "x " << dt->first;
			else
				desc << dt->first;
		}

		return desc.str();
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

	void mem_copy_from(device_memory& mem, size_t offset, size_t size)
	{
		device_ptr tmp = mem.device_pointer;

		/* todo: how does this work? */
		foreach(SubDevice& sub, devices) {
			mem.device_pointer = sub.ptr_map[tmp];
			sub.device->mem_copy_from(mem, offset, size);
			break;
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

	void tex_alloc(const char *name, device_memory& mem, bool interpolation, bool periodic)
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

	void draw_pixels(device_memory& rgba, int y, int w, int h, int width, int height, bool transparent)
	{
		device_ptr tmp = rgba.device_pointer;
		int i = 0, sub_h = h/devices.size();
		int sub_height = height/devices.size();

		foreach(SubDevice& sub, devices) {
			int sy = y + i*sub_h;
			int sh = (i == (int)devices.size() - 1)? h - sub_h*i: sub_h;
			int sheight = (i == (int)devices.size() - 1)? height - sub_height*i: sub_height;
			/* adjust math for w/width */

			rgba.device_pointer = sub.ptr_map[tmp];
			sub.device->draw_pixels(rgba, sy, w, sh, width, sheight, transparent);
			i++;
		}

		rgba.device_pointer = tmp;
	}

	void task_add(DeviceTask& task)
	{
		ThreadQueue<DeviceTask> tasks;
		task.split(tasks, devices.size());

		foreach(SubDevice& sub, devices) {
			DeviceTask subtask;

			if(tasks.worker_wait_pop(subtask)) {
				if(task.buffer) subtask.buffer = sub.ptr_map[task.buffer];
				if(task.rng_state) subtask.rng_state = sub.ptr_map[task.rng_state];
				if(task.rgba) subtask.rgba = sub.ptr_map[task.rgba];
				if(task.displace_input) subtask.displace_input = sub.ptr_map[task.displace_input];
				if(task.displace_offset) subtask.displace_offset = sub.ptr_map[task.displace_offset];

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

Device *device_multi_create(bool background)
{
	return new MultiDevice(background);
}

CCL_NAMESPACE_END


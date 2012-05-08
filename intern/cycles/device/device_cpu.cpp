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
#include <string.h>

#include "device.h"
#include "device_intern.h"

#include "kernel.h"
#include "kernel_types.h"

#include "osl_shader.h"

#include "util_debug.h"
#include "util_foreach.h"
#include "util_function.h"
#include "util_opengl.h"
#include "util_progress.h"
#include "util_system.h"
#include "util_thread.h"

CCL_NAMESPACE_BEGIN

class CPUDevice : public Device
{
public:
	vector<thread*> threads;
	ThreadQueue<DeviceTask> tasks;
	KernelGlobals *kg;
	
	CPUDevice(int threads_num)
	{
		kg = kernel_globals_create();

		/* do now to avoid thread issues */
		system_cpu_support_optimized();

		if(threads_num == 0)
			threads_num = system_cpu_thread_count();

		threads.resize(threads_num);

		for(size_t i = 0; i < threads.size(); i++)
			threads[i] = new thread(function_bind(&CPUDevice::thread_run, this, i));
	}

	~CPUDevice()
	{
		tasks.stop();

		foreach(thread *t, threads) {
			t->join();
			delete t;
		}

		kernel_globals_free(kg);
	}

	bool support_advanced_shading()
	{
		return true;
	}

	void mem_alloc(device_memory& mem, MemoryType type)
	{
		mem.device_pointer = mem.data_pointer;
	}

	void mem_copy_to(device_memory& mem)
	{
		/* no-op */
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		/* no-op */
	}

	void mem_zero(device_memory& mem)
	{
		memset((void*)mem.device_pointer, 0, mem.memory_size());
	}

	void mem_free(device_memory& mem)
	{
		mem.device_pointer = 0;
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		kernel_const_copy(kg, name, host, size);
	}

	void tex_alloc(const char *name, device_memory& mem, bool interpolation, bool periodic)
	{
		kernel_tex_copy(kg, name, mem.data_pointer, mem.data_width, mem.data_height);
		mem.device_pointer = mem.data_pointer;
	}

	void tex_free(device_memory& mem)
	{
		mem.device_pointer = 0;
	}

	void *osl_memory()
	{
#ifdef WITH_OSL
		return kernel_osl_memory(kg);
#else
		return NULL;
#endif
	}

	void thread_run(int t)
	{
		DeviceTask task;

		while(tasks.worker_wait_pop(task)) {
			if(task.type == DeviceTask::PATH_TRACE)
				thread_path_trace(task);
			else if(task.type == DeviceTask::TONEMAP)
				thread_tonemap(task);
			else if(task.type == DeviceTask::SHADER)
				thread_shader(task);

			tasks.worker_done();
		}
	}

	void thread_path_trace(DeviceTask& task)
	{
		if(tasks.worker_cancel())
			return;

#ifdef WITH_OSL
		if(kernel_osl_use(kg))
			OSLShader::thread_init(kg);
#endif

#ifdef WITH_OPTIMIZED_KERNEL
		if(system_cpu_support_optimized()) {
			for(int y = task.y; y < task.y + task.h; y++) {
				for(int x = task.x; x < task.x + task.w; x++)
					kernel_cpu_optimized_path_trace(kg, (float*)task.buffer, (unsigned int*)task.rng_state,
						task.sample, x, y, task.offset, task.stride);

				if(tasks.worker_cancel())
					break;
			}
		}
		else
#endif
		{
			for(int y = task.y; y < task.y + task.h; y++) {
				for(int x = task.x; x < task.x + task.w; x++)
					kernel_cpu_path_trace(kg, (float*)task.buffer, (unsigned int*)task.rng_state,
						task.sample, x, y, task.offset, task.stride);

				if(tasks.worker_cancel())
					break;
			}
		}

#ifdef WITH_OSL
		if(kernel_osl_use(kg))
			OSLShader::thread_free(kg);
#endif
	}

	void thread_tonemap(DeviceTask& task)
	{
#ifdef WITH_OPTIMIZED_KERNEL
		if(system_cpu_support_optimized()) {
			for(int y = task.y; y < task.y + task.h; y++)
				for(int x = task.x; x < task.x + task.w; x++)
					kernel_cpu_optimized_tonemap(kg, (uchar4*)task.rgba, (float*)task.buffer,
						task.sample, task.resolution, x, y, task.offset, task.stride);
		}
		else
#endif
		{
			for(int y = task.y; y < task.y + task.h; y++)
				for(int x = task.x; x < task.x + task.w; x++)
					kernel_cpu_tonemap(kg, (uchar4*)task.rgba, (float*)task.buffer,
						task.sample, task.resolution, x, y, task.offset, task.stride);
		}
	}

	void thread_shader(DeviceTask& task)
	{
#ifdef WITH_OSL
		if(kernel_osl_use(kg))
			OSLShader::thread_init(kg);
#endif

#ifdef WITH_OPTIMIZED_KERNEL
		if(system_cpu_support_optimized()) {
			for(int x = task.shader_x; x < task.shader_x + task.shader_w; x++) {
				kernel_cpu_optimized_shader(kg, (uint4*)task.shader_input, (float4*)task.shader_output, task.shader_eval_type, x);

				if(tasks.worker_cancel())
					break;
			}
		}
		else
#endif
		{
			for(int x = task.shader_x; x < task.shader_x + task.shader_w; x++) {
				kernel_cpu_shader(kg, (uint4*)task.shader_input, (float4*)task.shader_output, task.shader_eval_type, x);

				if(tasks.worker_cancel())
					break;
			}
		}

#ifdef WITH_OSL
		if(kernel_osl_use(kg))
			OSLShader::thread_free(kg);
#endif
	}

	void task_add(DeviceTask& task)
	{
		/* split task into smaller ones, more than number of threads for uneven
		   workloads where some parts of the image render slower than others */
		task.split(tasks, threads.size()*10);
	}

	void task_wait()
	{
		tasks.wait_done();
	}

	void task_cancel()
	{
		tasks.cancel();
	}
};

Device *device_cpu_create(DeviceInfo& info, int threads)
{
	return new CPUDevice(threads);
}

void device_cpu_info(vector<DeviceInfo>& devices)
{
	DeviceInfo info;

	info.type = DEVICE_CPU;
	info.description = system_cpu_brand_string();
	info.id = "CPU";
	info.num = 0;
	info.advanced_shading = true;

	devices.insert(devices.begin(), info);
}

CCL_NAMESPACE_END


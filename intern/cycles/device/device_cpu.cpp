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
#include <string.h>

#include "device.h"
#include "device_intern.h"

#include "kernel.h"
#include "kernel_compat_cpu.h"
#include "kernel_types.h"
#include "kernel_globals.h"

#include "osl_shader.h"
#include "osl_globals.h"

#include "buffers.h"

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
	TaskPool task_pool;
	KernelGlobals kernel_globals;

#ifdef WITH_OSL
	OSLGlobals osl_globals;
#endif
	
	CPUDevice(DeviceInfo& info, Stats &stats, bool background)
	: Device(info, stats, background)
	{
#ifdef WITH_OSL
		kernel_globals.osl = &osl_globals;
#endif

		/* do now to avoid thread issues */
		system_cpu_support_sse2();
		system_cpu_support_sse3();
		system_cpu_support_sse41();
		system_cpu_support_avx();
		system_cpu_support_avx2();
	}

	~CPUDevice()
	{
		task_pool.stop();
	}

	void mem_alloc(device_memory& mem, MemoryType type)
	{
		mem.device_pointer = mem.data_pointer;

		stats.mem_alloc(mem.memory_size());
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

		stats.mem_free(mem.memory_size());
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		kernel_const_copy(&kernel_globals, name, host, size);
	}

	void tex_alloc(const char *name, device_memory& mem, InterpolationType interpolation, bool periodic)
	{
		kernel_tex_copy(&kernel_globals, name, mem.data_pointer, mem.data_width, mem.data_height, mem.data_depth, interpolation);
		mem.device_pointer = mem.data_pointer;

		stats.mem_alloc(mem.memory_size());
	}

	void tex_free(device_memory& mem)
	{
		mem.device_pointer = 0;

		stats.mem_free(mem.memory_size());
	}

	void *osl_memory()
	{
#ifdef WITH_OSL
		return &osl_globals;
#else
		return NULL;
#endif
	}

	void thread_run(DeviceTask *task)
	{
		if(task->type == DeviceTask::PATH_TRACE)
			thread_path_trace(*task);
		else if(task->type == DeviceTask::FILM_CONVERT)
			thread_film_convert(*task);
		else if(task->type == DeviceTask::SHADER)
			thread_shader(*task);
	}

	class CPUDeviceTask : public DeviceTask {
	public:
		CPUDeviceTask(CPUDevice *device, DeviceTask& task)
		: DeviceTask(task)
		{
			run = function_bind(&CPUDevice::thread_run, device, this);
		}
	};

	void thread_path_trace(DeviceTask& task)
	{
		if(task_pool.canceled()) {
			if(task.need_finish_queue == false)
				return;
		}

		KernelGlobals kg = kernel_globals;

#ifdef WITH_OSL
		OSLShader::thread_init(&kg, &kernel_globals, &osl_globals);
#endif

		RenderTile tile;
		
		while(task.acquire_tile(this, tile)) {
			float *render_buffer = (float*)tile.buffer;
			uint *rng_state = (uint*)tile.rng_state;
			int start_sample = tile.start_sample;
			int end_sample = tile.start_sample + tile.num_samples;

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
			if(system_cpu_support_avx2()) {
				for(int sample = start_sample; sample < end_sample; sample++) {
					if (task.get_cancel() || task_pool.canceled()) {
						if(task.need_finish_queue == false)
							break;
					}

					for(int y = tile.y; y < tile.y + tile.h; y++) {
						for(int x = tile.x; x < tile.x + tile.w; x++) {
							kernel_cpu_avx2_path_trace(&kg, render_buffer, rng_state,
													  sample, x, y, tile.offset, tile.stride);
						}
					}

					tile.sample = sample + 1;

					task.update_progress(&tile);
				}
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
			if(system_cpu_support_avx()) {
				for(int sample = start_sample; sample < end_sample; sample++) {
					if (task.get_cancel() || task_pool.canceled()) {
						if(task.need_finish_queue == false)
							break;
					}

					for(int y = tile.y; y < tile.y + tile.h; y++) {
						for(int x = tile.x; x < tile.x + tile.w; x++) {
							kernel_cpu_avx_path_trace(&kg, render_buffer, rng_state,
								sample, x, y, tile.offset, tile.stride);
						}
					}

					tile.sample = sample + 1;

					task.update_progress(&tile);
				}
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41			
			if(system_cpu_support_sse41()) {
				for(int sample = start_sample; sample < end_sample; sample++) {
					if (task.get_cancel() || task_pool.canceled()) {
						if(task.need_finish_queue == false)
							break;
					}

					for(int y = tile.y; y < tile.y + tile.h; y++) {
						for(int x = tile.x; x < tile.x + tile.w; x++) {
							kernel_cpu_sse41_path_trace(&kg, render_buffer, rng_state,
								sample, x, y, tile.offset, tile.stride);
						}
					}

					tile.sample = sample + 1;

					task.update_progress(&tile);
				}
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
			if(system_cpu_support_sse3()) {
				for(int sample = start_sample; sample < end_sample; sample++) {
					if (task.get_cancel() || task_pool.canceled()) {
						if(task.need_finish_queue == false)
							break;
					}

					for(int y = tile.y; y < tile.y + tile.h; y++) {
						for(int x = tile.x; x < tile.x + tile.w; x++) {
							kernel_cpu_sse3_path_trace(&kg, render_buffer, rng_state,
								sample, x, y, tile.offset, tile.stride);
						}
					}

					tile.sample = sample + 1;

					task.update_progress(&tile);
				}
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
			if(system_cpu_support_sse2()) {
				for(int sample = start_sample; sample < end_sample; sample++) {
					if (task.get_cancel() || task_pool.canceled()) {
						if(task.need_finish_queue == false)
							break;
					}

					for(int y = tile.y; y < tile.y + tile.h; y++) {
						for(int x = tile.x; x < tile.x + tile.w; x++) {
							kernel_cpu_sse2_path_trace(&kg, render_buffer, rng_state,
								sample, x, y, tile.offset, tile.stride);
						}
					}

					tile.sample = sample + 1;

					task.update_progress(&tile);
				}
			}
			else
#endif
			{
				for(int sample = start_sample; sample < end_sample; sample++) {
					if (task.get_cancel() || task_pool.canceled()) {
						if(task.need_finish_queue == false)
							break;
					}

					for(int y = tile.y; y < tile.y + tile.h; y++) {
						for(int x = tile.x; x < tile.x + tile.w; x++) {
							kernel_cpu_path_trace(&kg, render_buffer, rng_state,
								sample, x, y, tile.offset, tile.stride);
						}
					}

					tile.sample = sample + 1;

					task.update_progress(&tile);
				}
			}

			task.release_tile(tile);

			if(task_pool.canceled()) {
				if(task.need_finish_queue == false)
					break;
			}
		}

#ifdef WITH_OSL
		OSLShader::thread_free(&kg);
#endif
	}

	void thread_film_convert(DeviceTask& task)
	{
		float sample_scale = 1.0f/(task.sample + 1);

		if(task.rgba_half) {
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
			if(system_cpu_support_avx2()) {
				for(int y = task.y; y < task.y + task.h; y++)
					for(int x = task.x; x < task.x + task.w; x++)
						kernel_cpu_avx2_convert_to_half_float(&kernel_globals, (uchar4*)task.rgba_half, (float*)task.buffer,
															 sample_scale, x, y, task.offset, task.stride);
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
			if(system_cpu_support_avx()) {
				for(int y = task.y; y < task.y + task.h; y++)
					for(int x = task.x; x < task.x + task.w; x++)
						kernel_cpu_avx_convert_to_half_float(&kernel_globals, (uchar4*)task.rgba_half, (float*)task.buffer,
							sample_scale, x, y, task.offset, task.stride);
			}
			else
#endif	
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41			
			if(system_cpu_support_sse41()) {
				for(int y = task.y; y < task.y + task.h; y++)
					for(int x = task.x; x < task.x + task.w; x++)
						kernel_cpu_sse41_convert_to_half_float(&kernel_globals, (uchar4*)task.rgba_half, (float*)task.buffer,
							sample_scale, x, y, task.offset, task.stride);
			}
			else
#endif		
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3		
			if(system_cpu_support_sse3()) {
				for(int y = task.y; y < task.y + task.h; y++)
					for(int x = task.x; x < task.x + task.w; x++)
						kernel_cpu_sse3_convert_to_half_float(&kernel_globals, (uchar4*)task.rgba_half, (float*)task.buffer,
							sample_scale, x, y, task.offset, task.stride);
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
			if(system_cpu_support_sse2()) {
				for(int y = task.y; y < task.y + task.h; y++)
					for(int x = task.x; x < task.x + task.w; x++)
						kernel_cpu_sse2_convert_to_half_float(&kernel_globals, (uchar4*)task.rgba_half, (float*)task.buffer,
							sample_scale, x, y, task.offset, task.stride);
			}
			else
#endif
			{
				for(int y = task.y; y < task.y + task.h; y++)
					for(int x = task.x; x < task.x + task.w; x++)
						kernel_cpu_convert_to_half_float(&kernel_globals, (uchar4*)task.rgba_half, (float*)task.buffer,
							sample_scale, x, y, task.offset, task.stride);
			}
		}
		else {
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
			if(system_cpu_support_avx2()) {
				for(int y = task.y; y < task.y + task.h; y++)
					for(int x = task.x; x < task.x + task.w; x++)
						kernel_cpu_avx2_convert_to_byte(&kernel_globals, (uchar4*)task.rgba_byte, (float*)task.buffer,
													   sample_scale, x, y, task.offset, task.stride);
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
			if(system_cpu_support_avx()) {
				for(int y = task.y; y < task.y + task.h; y++)
					for(int x = task.x; x < task.x + task.w; x++)
						kernel_cpu_avx_convert_to_byte(&kernel_globals, (uchar4*)task.rgba_byte, (float*)task.buffer,
							sample_scale, x, y, task.offset, task.stride);
			}
			else
#endif		
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41			
			if(system_cpu_support_sse41()) {
				for(int y = task.y; y < task.y + task.h; y++)
					for(int x = task.x; x < task.x + task.w; x++)
						kernel_cpu_sse41_convert_to_byte(&kernel_globals, (uchar4*)task.rgba_byte, (float*)task.buffer,
							sample_scale, x, y, task.offset, task.stride);
			}
			else
#endif			
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
			if(system_cpu_support_sse3()) {
				for(int y = task.y; y < task.y + task.h; y++)
					for(int x = task.x; x < task.x + task.w; x++)
						kernel_cpu_sse3_convert_to_byte(&kernel_globals, (uchar4*)task.rgba_byte, (float*)task.buffer,
							sample_scale, x, y, task.offset, task.stride);
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
			if(system_cpu_support_sse2()) {
				for(int y = task.y; y < task.y + task.h; y++)
					for(int x = task.x; x < task.x + task.w; x++)
						kernel_cpu_sse2_convert_to_byte(&kernel_globals, (uchar4*)task.rgba_byte, (float*)task.buffer,
							sample_scale, x, y, task.offset, task.stride);
			}
			else
#endif
			{
				for(int y = task.y; y < task.y + task.h; y++)
					for(int x = task.x; x < task.x + task.w; x++)
						kernel_cpu_convert_to_byte(&kernel_globals, (uchar4*)task.rgba_byte, (float*)task.buffer,
							sample_scale, x, y, task.offset, task.stride);
			}
		}
	}

	void thread_shader(DeviceTask& task)
	{
		KernelGlobals kg = kernel_globals;

#ifdef WITH_OSL
		OSLShader::thread_init(&kg, &kernel_globals, &osl_globals);
#endif

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
		if(system_cpu_support_avx2()) {
			for(int sample = 0; sample < task.num_samples; sample++) {
				for(int x = task.shader_x; x < task.shader_x + task.shader_w; x++)
					kernel_cpu_avx2_shader(&kg, (uint4*)task.shader_input, (float4*)task.shader_output,
					    task.shader_eval_type, x, task.offset, sample);

				if(task.get_cancel() || task_pool.canceled())
					break;

				task.update_progress(NULL);
			}
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
		if(system_cpu_support_avx()) {
			for(int sample = 0; sample < task.num_samples; sample++) {
				for(int x = task.shader_x; x < task.shader_x + task.shader_w; x++)
					kernel_cpu_avx_shader(&kg, (uint4*)task.shader_input, (float4*)task.shader_output,
					    task.shader_eval_type, x, task.offset, sample);

				if(task.get_cancel() || task_pool.canceled())
					break;

				task.update_progress(NULL);
			}
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41			
		if(system_cpu_support_sse41()) {
			for(int sample = 0; sample < task.num_samples; sample++) {
				for(int x = task.shader_x; x < task.shader_x + task.shader_w; x++)
					kernel_cpu_sse41_shader(&kg, (uint4*)task.shader_input, (float4*)task.shader_output,
					    task.shader_eval_type, x, task.offset, sample);

				if(task.get_cancel() || task_pool.canceled())
					break;

				task.update_progress(NULL);
			}
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
		if(system_cpu_support_sse3()) {
			for(int sample = 0; sample < task.num_samples; sample++) {
				for(int x = task.shader_x; x < task.shader_x + task.shader_w; x++)
					kernel_cpu_sse3_shader(&kg, (uint4*)task.shader_input, (float4*)task.shader_output,
					    task.shader_eval_type, x, task.offset, sample);

				if(task.get_cancel() || task_pool.canceled())
					break;

				task.update_progress(NULL);
			}
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
		if(system_cpu_support_sse2()) {
			for(int sample = 0; sample < task.num_samples; sample++) {
				for(int x = task.shader_x; x < task.shader_x + task.shader_w; x++)
					kernel_cpu_sse2_shader(&kg, (uint4*)task.shader_input, (float4*)task.shader_output,
					    task.shader_eval_type, x, task.offset, sample);

				if(task.get_cancel() || task_pool.canceled())
					break;

				task.update_progress(NULL);
			}
		}
		else
#endif
		{
			for(int sample = 0; sample < task.num_samples; sample++) {
				for(int x = task.shader_x; x < task.shader_x + task.shader_w; x++)
					kernel_cpu_shader(&kg, (uint4*)task.shader_input, (float4*)task.shader_output,
					    task.shader_eval_type, x, task.offset, sample);

				if(task.get_cancel() || task_pool.canceled())
					break;

				task.update_progress(NULL);
			}
		}

#ifdef WITH_OSL
		OSLShader::thread_free(&kg);
#endif
	}

	int get_split_task_count(DeviceTask& task)
	{
		if (task.type == DeviceTask::SHADER)
			return task.get_subtask_count(TaskScheduler::num_threads(), 256);
		else
			return task.get_subtask_count(TaskScheduler::num_threads());
	}

	void task_add(DeviceTask& task)
	{
		/* split task into smaller ones */
		list<DeviceTask> tasks;

		if(task.type == DeviceTask::SHADER)
			task.split(tasks, TaskScheduler::num_threads(), 256);
		else
			task.split(tasks, TaskScheduler::num_threads());

		foreach(DeviceTask& task, tasks)
			task_pool.push(new CPUDeviceTask(this, task));
	}

	void task_wait()
	{
		task_pool.wait_work();
	}

	void task_cancel()
	{
		task_pool.cancel();
	}
};

Device *device_cpu_create(DeviceInfo& info, Stats &stats, bool background)
{
	return new CPUDevice(info, stats, background);
}

void device_cpu_info(vector<DeviceInfo>& devices)
{
	DeviceInfo info;

	info.type = DEVICE_CPU;
	info.description = system_cpu_brand_string();
	info.id = "CPU";
	info.num = 0;
	info.advanced_shading = true;
	info.pack_images = false;

	devices.insert(devices.begin(), info);
}

CCL_NAMESPACE_END


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
#include <string.h>

/* So ImathMath is included before our kernel_cpu_compat. */
#ifdef WITH_OSL
/* So no context pollution happens from indirectly included windows.h */
#  include "util_windows.h"
#  include <OSL/oslexec.h>
#endif

#include "device.h"
#include "device_intern.h"
#include "device_split_kernel.h"

#include "kernel.h"
#include "kernel_compat_cpu.h"
#include "kernel_types.h"
#include "split/kernel_split_data.h"
#include "kernel_globals.h"

#include "osl_shader.h"
#include "osl_globals.h"

#include "buffers.h"

#include "util_debug.h"
#include "util_foreach.h"
#include "util_function.h"
#include "util_logging.h"
#include "util_map.h"
#include "util_opengl.h"
#include "util_progress.h"
#include "util_system.h"
#include "util_thread.h"

CCL_NAMESPACE_BEGIN

class CPUDevice;

class CPUSplitKernel : public DeviceSplitKernel {
	CPUDevice *device;
public:
	explicit CPUSplitKernel(CPUDevice *device);

	virtual bool enqueue_split_kernel_data_init(const KernelDimensions& dim,
	                                            RenderTile& rtile,
	                                            int num_global_elements,
	                                            device_memory& kernel_globals,
	                                            device_memory& kernel_data_,
	                                            device_memory& split_data,
	                                            device_memory& ray_state,
	                                            device_memory& queue_index,
	                                            device_memory& use_queues_flag,
	                                            device_memory& work_pool_wgs);

	virtual SplitKernelFunction* get_split_kernel_function(string kernel_name, const DeviceRequestedFeatures&);
	virtual int2 split_kernel_local_size();
	virtual int2 split_kernel_global_size(device_memory& kg, device_memory& data, DeviceTask *task);
	virtual uint64_t state_buffer_size(device_memory& kg, device_memory& data, size_t num_threads);
};

class CPUDevice : public Device
{
	static unordered_map<string, void*> kernel_functions;

	static void register_kernel_function(const char* name, void* func)
	{
		kernel_functions[name] = func;
	}

	static const char* get_arch_name()
	{
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
		if(system_cpu_support_avx2()) {
			return "cpu_avx2";
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
		if(system_cpu_support_avx()) {
			return "cpu_avx";
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
		if(system_cpu_support_sse41()) {
			return "cpu_sse41";
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
		if(system_cpu_support_sse3()) {
			return "cpu_sse3";
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
		if(system_cpu_support_sse2()) {
			return "cpu_sse2";
		}
		else
#endif
		{
			return "cpu";
		}
	}

	template<typename F>
	static F get_kernel_function(string name)
	{
		name = string("kernel_") + get_arch_name() + "_" + name;

		unordered_map<string, void*>::iterator it = kernel_functions.find(name);

		if(it == kernel_functions.end()) {
			assert(!"kernel function not found");
			return NULL;
		}

		return (F)it->second;
	}

	friend class CPUSplitKernel;

public:
	TaskPool task_pool;
	KernelGlobals kernel_globals;

#ifdef WITH_OSL
	OSLGlobals osl_globals;
#endif

	bool use_split_kernel;

	DeviceRequestedFeatures requested_features;
	
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

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
		if(system_cpu_support_avx2()) {
			VLOG(1) << "Will be using AVX2 kernels.";
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
		if(system_cpu_support_avx()) {
			VLOG(1) << "Will be using AVX kernels.";
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
		if(system_cpu_support_sse41()) {
			VLOG(1) << "Will be using SSE4.1 kernels.";
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
		if(system_cpu_support_sse3()) {
			VLOG(1) << "Will be using SSE3kernels.";
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
		if(system_cpu_support_sse2()) {
			VLOG(1) << "Will be using SSE2 kernels.";
		}
		else
#endif
		{
			VLOG(1) << "Will be using regular kernels.";
		}

		use_split_kernel = DebugFlags().cpu.split_kernel;
		if(use_split_kernel) {
			VLOG(1) << "Will be using split kernel.";
		}

		kernel_cpu_register_functions(register_kernel_function);
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
		kernel_cpu_sse2_register_functions(register_kernel_function);
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
		kernel_cpu_sse3_register_functions(register_kernel_function);
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
		kernel_cpu_sse41_register_functions(register_kernel_function);
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
		kernel_cpu_avx_register_functions(register_kernel_function);
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
		kernel_cpu_avx2_register_functions(register_kernel_function);
#endif
	}

	~CPUDevice()
	{
		task_pool.stop();
	}

	virtual bool show_samples() const
	{
		return (TaskScheduler::num_threads() == 1);
	}

	void mem_alloc(const char *name, device_memory& mem, MemoryType /*type*/)
	{
		if(name) {
			VLOG(1) << "Buffer allocate: " << name << ", "
			        << string_human_readable_number(mem.memory_size()) << " bytes. ("
			        << string_human_readable_size(mem.memory_size()) << ")";
		}

		mem.device_pointer = mem.data_pointer;

		if(!mem.device_pointer) {
			mem.device_pointer = (device_ptr)malloc(mem.memory_size());
		}

		mem.device_size = mem.memory_size();
		stats.mem_alloc(mem.device_size);
	}

	void mem_copy_to(device_memory& /*mem*/)
	{
		/* no-op */
	}

	void mem_copy_from(device_memory& /*mem*/,
	                   int /*y*/, int /*w*/, int /*h*/,
	                   int /*elem*/)
	{
		/* no-op */
	}

	void mem_zero(device_memory& mem)
	{
		memset((void*)mem.device_pointer, 0, mem.memory_size());
	}

	void mem_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			if(!mem.data_pointer) {
				free((void*)mem.device_pointer);
			}

			mem.device_pointer = 0;
			stats.mem_free(mem.device_size);
			mem.device_size = 0;
		}
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		kernel_const_copy(&kernel_globals, name, host, size);
	}

	void tex_alloc(const char *name,
	               device_memory& mem,
	               InterpolationType interpolation,
	               ExtensionType extension)
	{
		VLOG(1) << "Texture allocate: " << name << ", "
		        << string_human_readable_number(mem.memory_size()) << " bytes. ("
		        << string_human_readable_size(mem.memory_size()) << ")";
		kernel_tex_copy(&kernel_globals,
		                name,
		                mem.data_pointer,
		                mem.data_width,
		                mem.data_height,
		                mem.data_depth,
		                interpolation,
		                extension);
		mem.device_pointer = mem.data_pointer;
		mem.device_size = mem.memory_size();
		stats.mem_alloc(mem.device_size);
	}

	void tex_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			mem.device_pointer = 0;
			stats.mem_free(mem.device_size);
			mem.device_size = 0;
		}
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
		if(task->type == DeviceTask::PATH_TRACE) {
			if(!use_split_kernel) {
				thread_path_trace(*task);
			}
			else {
				thread_path_trace_split(*task);
			}
		}
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

		KernelGlobals kg = thread_kernel_globals_init();
		RenderTile tile;

		void(*path_trace_kernel)(KernelGlobals*, float*, unsigned int*, int, int, int, int, int);

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
		if(system_cpu_support_avx2()) {
			path_trace_kernel = kernel_cpu_avx2_path_trace;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
		if(system_cpu_support_avx()) {
			path_trace_kernel = kernel_cpu_avx_path_trace;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
		if(system_cpu_support_sse41()) {
			path_trace_kernel = kernel_cpu_sse41_path_trace;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
		if(system_cpu_support_sse3()) {
			path_trace_kernel = kernel_cpu_sse3_path_trace;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
		if(system_cpu_support_sse2()) {
			path_trace_kernel = kernel_cpu_sse2_path_trace;
		}
		else
#endif
		{
			path_trace_kernel = kernel_cpu_path_trace;
		}

		while(task.acquire_tile(this, tile)) {
			float *render_buffer = (float*)tile.buffer;
			uint *rng_state = (uint*)tile.rng_state;
			int start_sample = tile.start_sample;
			int end_sample = tile.start_sample + tile.num_samples;

			for(int sample = start_sample; sample < end_sample; sample++) {
				if(task.get_cancel() || task_pool.canceled()) {
					if(task.need_finish_queue == false)
						break;
				}

				for(int y = tile.y; y < tile.y + tile.h; y++) {
					for(int x = tile.x; x < tile.x + tile.w; x++) {
						path_trace_kernel(&kg, render_buffer, rng_state,
						                  sample, x, y, tile.offset, tile.stride);
					}
				}

				tile.sample = sample + 1;

				task.update_progress(&tile, tile.w*tile.h);
			}

			task.release_tile(tile);

			if(task_pool.canceled()) {
				if(task.need_finish_queue == false)
					break;
			}
		}

		thread_kernel_globals_free(&kg);
	}

	void thread_path_trace_split(DeviceTask& task)
	{
		if(task_pool.canceled()) {
			if(task.need_finish_queue == false)
				return;
		}

		RenderTile tile;

		CPUSplitKernel split_kernel(this);

		/* allocate buffer for kernel globals */
		device_memory kgbuffer;
		kgbuffer.resize(sizeof(KernelGlobals));
		mem_alloc("kernel_globals", kgbuffer, MEM_READ_WRITE);

		KernelGlobals *kg = (KernelGlobals*)kgbuffer.device_pointer;
		*kg = thread_kernel_globals_init();

		requested_features.max_closure = MAX_CLOSURE;
		if(!split_kernel.load_kernels(requested_features)) {
			thread_kernel_globals_free((KernelGlobals*)kgbuffer.device_pointer);
			mem_free(kgbuffer);

			return;
		}

		while(task.acquire_tile(this, tile)) {
			device_memory data;
			split_kernel.path_trace(&task, tile, kgbuffer, data);

			task.release_tile(tile);

			if(task_pool.canceled()) {
				if(task.need_finish_queue == false)
					break;
			}
		}

		thread_kernel_globals_free((KernelGlobals*)kgbuffer.device_pointer);
		mem_free(kgbuffer);
	}

	void thread_film_convert(DeviceTask& task)
	{
		float sample_scale = 1.0f/(task.sample + 1);

		if(task.rgba_half) {
			void(*convert_to_half_float_kernel)(KernelGlobals *, uchar4 *, float *, float, int, int, int, int);
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
			if(system_cpu_support_avx2()) {
				convert_to_half_float_kernel = kernel_cpu_avx2_convert_to_half_float;
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
			if(system_cpu_support_avx()) {
				convert_to_half_float_kernel = kernel_cpu_avx_convert_to_half_float;
			}
			else
#endif	
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41			
			if(system_cpu_support_sse41()) {
				convert_to_half_float_kernel = kernel_cpu_sse41_convert_to_half_float;
			}
			else
#endif		
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3		
			if(system_cpu_support_sse3()) {
				convert_to_half_float_kernel = kernel_cpu_sse3_convert_to_half_float;
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
			if(system_cpu_support_sse2()) {
				convert_to_half_float_kernel = kernel_cpu_sse2_convert_to_half_float;
			}
			else
#endif
			{
				convert_to_half_float_kernel = kernel_cpu_convert_to_half_float;
			}

			for(int y = task.y; y < task.y + task.h; y++)
				for(int x = task.x; x < task.x + task.w; x++)
					convert_to_half_float_kernel(&kernel_globals, (uchar4*)task.rgba_half, (float*)task.buffer,
						sample_scale, x, y, task.offset, task.stride);
		}
		else {
			void(*convert_to_byte_kernel)(KernelGlobals *, uchar4 *, float *, float, int, int, int, int);
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
			if(system_cpu_support_avx2()) {
				convert_to_byte_kernel = kernel_cpu_avx2_convert_to_byte;
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
			if(system_cpu_support_avx()) {
				convert_to_byte_kernel = kernel_cpu_avx_convert_to_byte;
			}
			else
#endif		
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41			
			if(system_cpu_support_sse41()) {
				convert_to_byte_kernel = kernel_cpu_sse41_convert_to_byte;
			}
			else
#endif			
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
			if(system_cpu_support_sse3()) {
				convert_to_byte_kernel = kernel_cpu_sse3_convert_to_byte;
			}
			else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
			if(system_cpu_support_sse2()) {
				convert_to_byte_kernel = kernel_cpu_sse2_convert_to_byte;
			}
			else
#endif
			{
				convert_to_byte_kernel = kernel_cpu_convert_to_byte;
			}

			for(int y = task.y; y < task.y + task.h; y++)
				for(int x = task.x; x < task.x + task.w; x++)
					convert_to_byte_kernel(&kernel_globals, (uchar4*)task.rgba_byte, (float*)task.buffer,
						sample_scale, x, y, task.offset, task.stride);

		}
	}

	void thread_shader(DeviceTask& task)
	{
		KernelGlobals kg = kernel_globals;

#ifdef WITH_OSL
		OSLShader::thread_init(&kg, &kernel_globals, &osl_globals);
#endif
		void(*shader_kernel)(KernelGlobals*, uint4*, float4*, float*, int, int, int, int, int);

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
		if(system_cpu_support_avx2()) {
			shader_kernel = kernel_cpu_avx2_shader;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
		if(system_cpu_support_avx()) {
			shader_kernel = kernel_cpu_avx_shader;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41			
		if(system_cpu_support_sse41()) {
			shader_kernel = kernel_cpu_sse41_shader;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
		if(system_cpu_support_sse3()) {
			shader_kernel = kernel_cpu_sse3_shader;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
		if(system_cpu_support_sse2()) {
			shader_kernel = kernel_cpu_sse2_shader;
		}
		else
#endif
		{
			shader_kernel = kernel_cpu_shader;
		}

		for(int sample = 0; sample < task.num_samples; sample++) {
			for(int x = task.shader_x; x < task.shader_x + task.shader_w; x++)
				shader_kernel(&kg,
				              (uint4*)task.shader_input,
				              (float4*)task.shader_output,
				              (float*)task.shader_output_luma,
				              task.shader_eval_type,
				              task.shader_filter,
				              x,
				              task.offset,
				              sample);

			if(task.get_cancel() || task_pool.canceled())
				break;

			task.update_progress(NULL);

		}

#ifdef WITH_OSL
		OSLShader::thread_free(&kg);
#endif
	}

	int get_split_task_count(DeviceTask& task)
	{
		if(task.type == DeviceTask::SHADER)
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

protected:
	inline KernelGlobals thread_kernel_globals_init()
	{
		KernelGlobals kg = kernel_globals;
		kg.transparent_shadow_intersections = NULL;
		const int decoupled_count = sizeof(kg.decoupled_volume_steps) /
		                            sizeof(*kg.decoupled_volume_steps);
		for(int i = 0; i < decoupled_count; ++i) {
			kg.decoupled_volume_steps[i] = NULL;
		}
		kg.decoupled_volume_steps_index = 0;
#ifdef WITH_OSL
		OSLShader::thread_init(&kg, &kernel_globals, &osl_globals);
#endif
		return kg;
	}

	inline void thread_kernel_globals_free(KernelGlobals *kg)
	{
		if(kg == NULL) {
			return;
		}

		if(kg->transparent_shadow_intersections != NULL) {
			free(kg->transparent_shadow_intersections);
		}
		const int decoupled_count = sizeof(kg->decoupled_volume_steps) /
		                            sizeof(*kg->decoupled_volume_steps);
		for(int i = 0; i < decoupled_count; ++i) {
			if(kg->decoupled_volume_steps[i] != NULL) {
				free(kg->decoupled_volume_steps[i]);
			}
		}
#ifdef WITH_OSL
		OSLShader::thread_free(kg);
#endif
	}

	virtual bool load_kernels(DeviceRequestedFeatures& requested_features_) {
		requested_features = requested_features_;

		return true;
	}
};

/* split kernel */

class CPUSplitKernelFunction : public SplitKernelFunction {
public:
	CPUDevice* device;
	void (*func)(KernelGlobals *kg, KernelData *data);

	CPUSplitKernelFunction(CPUDevice* device) : device(device), func(NULL) {}
	~CPUSplitKernelFunction() {}

	virtual bool enqueue(const KernelDimensions& dim, device_memory& kernel_globals, device_memory& data)
	{
		if(!func) {
			return false;
		}

		KernelGlobals *kg = (KernelGlobals*)kernel_globals.device_pointer;
		kg->global_size = make_int2(dim.global_size[0], dim.global_size[1]);

		for(int y = 0; y < dim.global_size[1]; y++) {
			for(int x = 0; x < dim.global_size[0]; x++) {
				kg->global_id = make_int2(x, y);

				func(kg, (KernelData*)data.device_pointer);
			}
		}

		return true;
	}
};

CPUSplitKernel::CPUSplitKernel(CPUDevice *device) : DeviceSplitKernel(device), device(device)
{
}

bool CPUSplitKernel::enqueue_split_kernel_data_init(const KernelDimensions& dim,
                                                    RenderTile& rtile,
                                                    int num_global_elements,
                                                    device_memory& kernel_globals,
                                                    device_memory& data,
                                                    device_memory& split_data,
                                                    device_memory& ray_state,
                                                    device_memory& queue_index,
                                                    device_memory& use_queues_flags,
                                                    device_memory& work_pool_wgs)
{
	typedef void(*data_init_t)(KernelGlobals *kg,
	                           ccl_constant KernelData *data,
	                           ccl_global void *split_data_buffer,
	                           int num_elements,
	                           ccl_global char *ray_state,
	                           ccl_global uint *rng_state,
	                           int start_sample,
	                           int end_sample,
	                           int sx, int sy, int sw, int sh, int offset, int stride,
	                           ccl_global int *Queue_index,
	                           int queuesize,
	                           ccl_global char *use_queues_flag,
	                           ccl_global unsigned int *work_pool_wgs,
	                           unsigned int num_samples,
	                           ccl_global float *buffer);

	data_init_t data_init;

#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
	if(system_cpu_support_avx2()) {
		data_init = kernel_cpu_avx2_data_init;
	}
	else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
	if(system_cpu_support_avx()) {
		data_init = kernel_cpu_avx_data_init;
	}
	else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
	if(system_cpu_support_sse41()) {
		data_init = kernel_cpu_sse41_data_init;
	}
	else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
	if(system_cpu_support_sse3()) {
		data_init = kernel_cpu_sse3_data_init;
	}
	else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
	if(system_cpu_support_sse2()) {
		data_init = kernel_cpu_sse2_data_init;
	}
	else
#endif
	{
		data_init = kernel_cpu_data_init;
	}

	KernelGlobals *kg = (KernelGlobals*)kernel_globals.device_pointer;
	kg->global_size = make_int2(dim.global_size[0], dim.global_size[1]);

	for(int y = 0; y < dim.global_size[1]; y++) {
		for(int x = 0; x < dim.global_size[0]; x++) {
			kg->global_id = make_int2(x, y);

			data_init((KernelGlobals*)kernel_globals.device_pointer,
			          (KernelData*)data.device_pointer,
			          (void*)split_data.device_pointer,
			          num_global_elements,
			          (char*)ray_state.device_pointer,
			          (uint*)rtile.rng_state,
			          rtile.start_sample,
			          rtile.start_sample + rtile.num_samples,
			          rtile.x,
			          rtile.y,
			          rtile.w,
			          rtile.h,
			          rtile.offset,
			          rtile.stride,
			          (int*)queue_index.device_pointer,
			          dim.global_size[0] * dim.global_size[1],
			          (char*)use_queues_flags.device_pointer,
			          (uint*)work_pool_wgs.device_pointer,
			          rtile.num_samples,
			          (float*)rtile.buffer);
		}
	}

	return true;
}

SplitKernelFunction* CPUSplitKernel::get_split_kernel_function(string kernel_name, const DeviceRequestedFeatures&)
{
	CPUSplitKernelFunction *kernel = new CPUSplitKernelFunction(device);

	kernel->func = device->get_kernel_function<void(*)(KernelGlobals*, KernelData*)>(kernel_name);
	if(!kernel->func) {
		delete kernel;
		return NULL;
	}

	return kernel;
}

int2 CPUSplitKernel::split_kernel_local_size()
{
	return make_int2(1, 1);
}

int2 CPUSplitKernel::split_kernel_global_size(device_memory& /*kg*/, device_memory& /*data*/, DeviceTask *task) {
	return make_int2(64, 1);
}

uint64_t CPUSplitKernel::state_buffer_size(device_memory& kernel_globals, device_memory& /*data*/, size_t num_threads) {
	KernelGlobals *kg = (KernelGlobals*)kernel_globals.device_pointer;

	return split_data_buffer_size(kg, num_threads);
}

unordered_map<string, void*> CPUDevice::kernel_functions;

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

string device_cpu_capabilities(void)
{
	string capabilities = "";
	capabilities += system_cpu_support_sse2() ? "SSE2 " : "";
	capabilities += system_cpu_support_sse3() ? "SSE3 " : "";
	capabilities += system_cpu_support_sse41() ? "SSE41 " : "";
	capabilities += system_cpu_support_avx() ? "AVX " : "";
	capabilities += system_cpu_support_avx2() ? "AVX2" : "";
	if(capabilities[capabilities.size() - 1] == ' ')
		capabilities.resize(capabilities.size() - 1);
	return capabilities;
}

CCL_NAMESPACE_END

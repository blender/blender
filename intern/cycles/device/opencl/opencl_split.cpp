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

#ifdef WITH_OPENCL

#include "device/opencl/opencl.h"

#include "render/buffers.h"

#include "kernel/kernel_types.h"
#include "kernel/split/kernel_split_data_types.h"

#include "device/device_split_kernel.h"

#include "util/util_algorithm.h"
#include "util/util_debug.h"
#include "util/util_logging.h"
#include "util/util_md5.h"
#include "util/util_path.h"
#include "util/util_time.h"

CCL_NAMESPACE_BEGIN

class OpenCLSplitKernel;

namespace {

/* Copy dummy KernelGlobals related to OpenCL from kernel_globals.h to
 * fetch its size.
 */
typedef struct KernelGlobalsDummy {
	ccl_constant KernelData *data;
	ccl_global char *buffers[8];

#define KERNEL_TEX(type, name) \
	TextureInfo name;
#  include "kernel/kernel_textures.h"
#undef KERNEL_TEX
	SplitData split_data;
	SplitParams split_param_data;
} KernelGlobalsDummy;

}  // namespace

static string get_build_options(OpenCLDeviceBase *device, const DeviceRequestedFeatures& requested_features)
{
	string build_options = "-D__SPLIT_KERNEL__ ";
	build_options += requested_features.get_build_options();

	/* Set compute device build option. */
	cl_device_type device_type;
	OpenCLInfo::get_device_type(device->cdDevice, &device_type, &device->ciErr);
	assert(device->ciErr == CL_SUCCESS);
	if(device_type == CL_DEVICE_TYPE_GPU) {
		build_options += " -D__COMPUTE_DEVICE_GPU__";
	}

	return build_options;
}

/* OpenCLDeviceSplitKernel's declaration/definition. */
class OpenCLDeviceSplitKernel : public OpenCLDeviceBase
{
public:
	DeviceSplitKernel *split_kernel;
	OpenCLProgram program_data_init;
	OpenCLProgram program_state_buffer_size;

	OpenCLDeviceSplitKernel(DeviceInfo& info, Stats &stats, bool background_);

	~OpenCLDeviceSplitKernel()
	{
		task_pool.stop();

		/* Release kernels */
		program_data_init.release();

		delete split_kernel;
	}

	virtual bool show_samples() const {
		return true;
	}

	virtual bool load_kernels(const DeviceRequestedFeatures& requested_features,
	                          vector<OpenCLDeviceBase::OpenCLProgram*> &programs)
	{
		bool single_program = OpenCLInfo::use_single_program();
		program_data_init = OpenCLDeviceBase::OpenCLProgram(this,
		                                  single_program ? "split" : "split_data_init",
		                                  single_program ? "kernel_split.cl" : "kernel_data_init.cl",
		                                  get_build_options(this, requested_features));

		program_data_init.add_kernel(ustring("path_trace_data_init"));
		programs.push_back(&program_data_init);

		program_state_buffer_size = OpenCLDeviceBase::OpenCLProgram(this,
		                                  single_program ? "split" : "split_state_buffer_size",
		                                  single_program ? "kernel_split.cl" : "kernel_state_buffer_size.cl",
		                                  get_build_options(this, requested_features));
		program_state_buffer_size.add_kernel(ustring("path_trace_state_buffer_size"));
		programs.push_back(&program_state_buffer_size);

		return split_kernel->load_kernels(requested_features);
	}

	void thread_run(DeviceTask *task)
	{
		flush_texture_buffers();

		if(task->type == DeviceTask::FILM_CONVERT) {
			film_convert(*task, task->buffer, task->rgba_byte, task->rgba_half);
		}
		else if(task->type == DeviceTask::SHADER) {
			shader(*task);
		}
		else if(task->type == DeviceTask::RENDER) {
			RenderTile tile;
			DenoisingTask denoising(this, *task);

			/* Allocate buffer for kernel globals */
			device_only_memory<KernelGlobalsDummy> kgbuffer(this, "kernel_globals");
			kgbuffer.alloc_to_device(1);

			/* Keep rendering tiles until done. */
			while(task->acquire_tile(this, tile)) {
				if(tile.task == RenderTile::PATH_TRACE) {
					assert(tile.task == RenderTile::PATH_TRACE);
					scoped_timer timer(&tile.buffers->render_time);

					split_kernel->path_trace(task,
					                         tile,
					                         kgbuffer,
					                         *const_mem_map["__data"]);

					/* Complete kernel execution before release tile. */
					/* This helps in multi-device render;
					 * The device that reaches the critical-section function
					 * release_tile waits (stalling other devices from entering
					 * release_tile) for all kernels to complete. If device1 (a
					 * slow-render device) reaches release_tile first then it would
					 * stall device2 (a fast-render device) from proceeding to render
					 * next tile.
					 */
					clFinish(cqCommandQueue);
				}
				else if(tile.task == RenderTile::DENOISE) {
					tile.sample = tile.start_sample + tile.num_samples;
					denoise(tile, denoising);
					task->update_progress(&tile, tile.w*tile.h);
				}

				task->release_tile(tile);
			}

			kgbuffer.free();
		}
	}

	bool is_split_kernel()
	{
		return true;
	}

protected:
	/* ** Those guys are for workign around some compiler-specific bugs ** */

	string build_options_for_base_program(
	        const DeviceRequestedFeatures& requested_features)
	{
		return requested_features.get_build_options();
	}

	friend class OpenCLSplitKernel;
	friend class OpenCLSplitKernelFunction;
};

struct CachedSplitMemory {
	int id;
	device_memory *split_data;
	device_memory *ray_state;
	device_memory *queue_index;
	device_memory *use_queues_flag;
	device_memory *work_pools;
	device_ptr *buffer;
};

class OpenCLSplitKernelFunction : public SplitKernelFunction {
public:
	OpenCLDeviceSplitKernel* device;
	OpenCLDeviceBase::OpenCLProgram program;
	CachedSplitMemory& cached_memory;
	int cached_id;

	OpenCLSplitKernelFunction(OpenCLDeviceSplitKernel* device, CachedSplitMemory& cached_memory) :
			device(device), cached_memory(cached_memory), cached_id(cached_memory.id-1)
	{
	}

	~OpenCLSplitKernelFunction()
	{
		program.release();
	}

	virtual bool enqueue(const KernelDimensions& dim, device_memory& kg, device_memory& data)
	{
		if(cached_id != cached_memory.id) {
			cl_uint start_arg_index =
				device->kernel_set_args(program(),
					            0,
					            kg,
					            data,
					            *cached_memory.split_data,
					            *cached_memory.ray_state);

				device->set_kernel_arg_buffers(program(), &start_arg_index);

			start_arg_index +=
				device->kernel_set_args(program(),
					            start_arg_index,
					            *cached_memory.queue_index,
					            *cached_memory.use_queues_flag,
					            *cached_memory.work_pools,
					            *cached_memory.buffer);

			cached_id = cached_memory.id;
		}

		device->ciErr = clEnqueueNDRangeKernel(device->cqCommandQueue,
		                                       program(),
		                                       2,
		                                       NULL,
		                                       dim.global_size,
		                                       dim.local_size,
		                                       0,
		                                       NULL,
		                                       NULL);

		device->opencl_assert_err(device->ciErr, "clEnqueueNDRangeKernel");

		if(device->ciErr != CL_SUCCESS) {
			string message = string_printf("OpenCL error: %s in clEnqueueNDRangeKernel()",
			                               clewErrorString(device->ciErr));
			device->opencl_error(message);
			return false;
		}

		return true;
	}
};

class OpenCLSplitKernel : public DeviceSplitKernel {
	OpenCLDeviceSplitKernel *device;
	CachedSplitMemory cached_memory;
public:
	explicit OpenCLSplitKernel(OpenCLDeviceSplitKernel *device) : DeviceSplitKernel(device), device(device) {
	}

	virtual SplitKernelFunction* get_split_kernel_function(const string& kernel_name,
	                                                       const DeviceRequestedFeatures& requested_features)
	{
		OpenCLSplitKernelFunction* kernel = new OpenCLSplitKernelFunction(device, cached_memory);

		bool single_program = OpenCLInfo::use_single_program();
		kernel->program =
			OpenCLDeviceBase::OpenCLProgram(device,
			                                single_program ? "split" : "split_" + kernel_name,
			                                single_program ? "kernel_split.cl" : "kernel_" + kernel_name + ".cl",
			                                get_build_options(device, requested_features));

		kernel->program.add_kernel(ustring("path_trace_" + kernel_name));
		kernel->program.load();

		if(!kernel->program.is_loaded()) {
			delete kernel;
			return NULL;
		}

		return kernel;
	}

	virtual uint64_t state_buffer_size(device_memory& kg, device_memory& data, size_t num_threads)
	{
		device_vector<uint64_t> size_buffer(device, "size_buffer", MEM_READ_WRITE);
		size_buffer.alloc(1);
		size_buffer.zero_to_device();

		uint threads = num_threads;
		device->kernel_set_args(device->program_state_buffer_size(), 0, kg, data, threads, size_buffer);

		size_t global_size = 64;
		device->ciErr = clEnqueueNDRangeKernel(device->cqCommandQueue,
		                               device->program_state_buffer_size(),
		                               1,
		                               NULL,
		                               &global_size,
		                               NULL,
		                               0,
		                               NULL,
		                               NULL);

		device->opencl_assert_err(device->ciErr, "clEnqueueNDRangeKernel");

		size_buffer.copy_from_device(0, 1, 1);
		size_t size = size_buffer[0];
		size_buffer.free();

		if(device->ciErr != CL_SUCCESS) {
			string message = string_printf("OpenCL error: %s in clEnqueueNDRangeKernel()",
			                               clewErrorString(device->ciErr));
			device->opencl_error(message);
			return 0;
		}

		return size;
	}

	virtual bool enqueue_split_kernel_data_init(const KernelDimensions& dim,
	                                            RenderTile& rtile,
	                                            int num_global_elements,
	                                            device_memory& kernel_globals,
	                                            device_memory& kernel_data,
	                                            device_memory& split_data,
	                                            device_memory& ray_state,
	                                            device_memory& queue_index,
	                                            device_memory& use_queues_flag,
	                                            device_memory& work_pool_wgs
	                                            )
	{
		cl_int dQueue_size = dim.global_size[0] * dim.global_size[1];

		/* Set the range of samples to be processed for every ray in
		 * path-regeneration logic.
		 */
		cl_int start_sample = rtile.start_sample;
		cl_int end_sample = rtile.start_sample + rtile.num_samples;

		cl_uint start_arg_index =
			device->kernel_set_args(device->program_data_init(),
			                0,
			                kernel_globals,
			                kernel_data,
			                split_data,
			                num_global_elements,
			                ray_state);

			device->set_kernel_arg_buffers(device->program_data_init(), &start_arg_index);

		start_arg_index +=
			device->kernel_set_args(device->program_data_init(),
			                start_arg_index,
			                start_sample,
			                end_sample,
			                rtile.x,
			                rtile.y,
			                rtile.w,
			                rtile.h,
			                rtile.offset,
			                rtile.stride,
			                queue_index,
			                dQueue_size,
			                use_queues_flag,
			                work_pool_wgs,
			                rtile.num_samples,
			                rtile.buffer);

		/* Enqueue ckPathTraceKernel_data_init kernel. */
		device->ciErr = clEnqueueNDRangeKernel(device->cqCommandQueue,
		                               device->program_data_init(),
		                               2,
		                               NULL,
		                               dim.global_size,
		                               dim.local_size,
		                               0,
		                               NULL,
		                               NULL);

		device->opencl_assert_err(device->ciErr, "clEnqueueNDRangeKernel");

		if(device->ciErr != CL_SUCCESS) {
			string message = string_printf("OpenCL error: %s in clEnqueueNDRangeKernel()",
			                               clewErrorString(device->ciErr));
			device->opencl_error(message);
			return false;
		}

		cached_memory.split_data = &split_data;
		cached_memory.ray_state = &ray_state;
		cached_memory.queue_index = &queue_index;
		cached_memory.use_queues_flag = &use_queues_flag;
		cached_memory.work_pools = &work_pool_wgs;
		cached_memory.buffer = &rtile.buffer;
		cached_memory.id++;

		return true;
	}

	virtual int2 split_kernel_local_size()
	{
		return make_int2(64, 1);
	}

	virtual int2 split_kernel_global_size(device_memory& kg, device_memory& data, DeviceTask * /*task*/)
	{
		cl_device_type type = OpenCLInfo::get_device_type(device->cdDevice);
		/* Use small global size on CPU devices as it seems to be much faster. */
		if(type == CL_DEVICE_TYPE_CPU) {
			VLOG(1) << "Global size: (64, 64).";
			return make_int2(64, 64);
		}

		cl_ulong max_buffer_size;
		clGetDeviceInfo(device->cdDevice, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &max_buffer_size, NULL);

		if(DebugFlags().opencl.mem_limit) {
			max_buffer_size = min(max_buffer_size,
			                      cl_ulong(DebugFlags().opencl.mem_limit - device->stats.mem_used));
		}

		VLOG(1) << "Maximum device allocation size: "
		        << string_human_readable_number(max_buffer_size) << " bytes. ("
		        << string_human_readable_size(max_buffer_size) << ").";

		/* Limit to 2gb, as we shouldn't need more than that and some devices may support much more. */
		max_buffer_size = min(max_buffer_size / 2, (cl_ulong)2l*1024*1024*1024);

		size_t num_elements = max_elements_for_max_buffer_size(kg, data, max_buffer_size);
		int2 global_size = make_int2(max(round_down((int)sqrt(num_elements), 64), 64), (int)sqrt(num_elements));
		VLOG(1) << "Global size: " << global_size << ".";
		return global_size;
	}
};

OpenCLDeviceSplitKernel::OpenCLDeviceSplitKernel(DeviceInfo& info, Stats &stats, bool background_)
: OpenCLDeviceBase(info, stats, background_)
{
	split_kernel = new OpenCLSplitKernel(this);

	background = background_;
}

Device *opencl_create_split_device(DeviceInfo& info, Stats& stats, bool background)
{
	return new OpenCLDeviceSplitKernel(info, stats, background);
}

CCL_NAMESPACE_END

#endif /* WITH_OPENCL */

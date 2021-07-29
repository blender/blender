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
#  include "util/util_windows.h"
#  include <OSL/oslexec.h>
#endif

#include "device/device.h"
#include "device/device_denoising.h"
#include "device/device_intern.h"
#include "device/device_split_kernel.h"

#include "kernel/kernel.h"
#include "kernel/kernel_compat_cpu.h"
#include "kernel/kernel_types.h"
#include "kernel/split/kernel_split_data.h"
#include "kernel/kernel_globals.h"

#include "kernel/filter/filter.h"

#include "kernel/osl/osl_shader.h"
#include "kernel/osl/osl_globals.h"

#include "render/buffers.h"

#include "util/util_debug.h"
#include "util/util_foreach.h"
#include "util/util_function.h"
#include "util/util_logging.h"
#include "util/util_map.h"
#include "util/util_opengl.h"
#include "util/util_progress.h"
#include "util/util_system.h"
#include "util/util_thread.h"

CCL_NAMESPACE_BEGIN

class CPUDevice;

/* Has to be outside of the class to be shared across template instantiations. */
static const char *logged_architecture = "";

template<typename F>
class KernelFunctions {
public:
	KernelFunctions()
	{
		kernel = (F)NULL;
	}

	KernelFunctions(F kernel_default,
	                F kernel_sse2,
	                F kernel_sse3,
	                F kernel_sse41,
	                F kernel_avx,
	                F kernel_avx2)
	{
		const char *architecture_name = "default";
		kernel = kernel_default;

		/* Silence potential warnings about unused variables
		 * when compiling without some architectures. */
		(void)kernel_sse2;
		(void)kernel_sse3;
		(void)kernel_sse41;
		(void)kernel_avx;
		(void)kernel_avx2;
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
		if(system_cpu_support_avx2()) {
			architecture_name = "AVX2";
			kernel = kernel_avx2;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
		if(system_cpu_support_avx()) {
			architecture_name = "AVX";
			kernel = kernel_avx;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
		if(system_cpu_support_sse41()) {
			architecture_name = "SSE4.1";
			kernel = kernel_sse41;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
		if(system_cpu_support_sse3()) {
			architecture_name = "SSE3";
			kernel = kernel_sse3;
		}
		else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
		if(system_cpu_support_sse2()) {
			architecture_name = "SSE2";
			kernel = kernel_sse2;
		}
#endif

		if(strcmp(architecture_name, logged_architecture) != 0) {
			VLOG(1) << "Will be using " << architecture_name << " kernels.";
			logged_architecture = architecture_name;
		}
	}

	inline F operator()() const {
		assert(kernel);
		return kernel;
	}
protected:
	F kernel;
};

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

	virtual SplitKernelFunction* get_split_kernel_function(const string& kernel_name,
	                                                       const DeviceRequestedFeatures&);
	virtual int2 split_kernel_local_size();
	virtual int2 split_kernel_global_size(device_memory& kg, device_memory& data, DeviceTask *task);
	virtual uint64_t state_buffer_size(device_memory& kg, device_memory& data, size_t num_threads);
};

class CPUDevice : public Device
{
public:
	TaskPool task_pool;
	KernelGlobals kernel_globals;

#ifdef WITH_OSL
	OSLGlobals osl_globals;
#endif

	bool use_split_kernel;

	DeviceRequestedFeatures requested_features;

	KernelFunctions<void(*)(KernelGlobals *, float *, unsigned int *, int, int, int, int, int)>   path_trace_kernel;
	KernelFunctions<void(*)(KernelGlobals *, uchar4 *, float *, float, int, int, int, int)>       convert_to_half_float_kernel;
	KernelFunctions<void(*)(KernelGlobals *, uchar4 *, float *, float, int, int, int, int)>       convert_to_byte_kernel;
	KernelFunctions<void(*)(KernelGlobals *, uint4 *, float4 *, float*, int, int, int, int, int)> shader_kernel;

	KernelFunctions<void(*)(int, TilesInfo*, int, int, float*, float*, float*, float*, float*, int*, int, int, bool)> filter_divide_shadow_kernel;
	KernelFunctions<void(*)(int, TilesInfo*, int, int, int, int, float*, float*, int*, int, int, bool)>               filter_get_feature_kernel;
	KernelFunctions<void(*)(int, int, float*, float*, float*, float*, int*, int)>                                     filter_detect_outliers_kernel;
	KernelFunctions<void(*)(int, int, float*, float*, float*, float*, int*, int)>                                     filter_combine_halves_kernel;

	KernelFunctions<void(*)(int, int, float*, float*, float*, int*, int, int, float, float)> filter_nlm_calc_difference_kernel;
	KernelFunctions<void(*)(float*, float*, int*, int, int)>                                 filter_nlm_blur_kernel;
	KernelFunctions<void(*)(float*, float*, int*, int, int)>                                 filter_nlm_calc_weight_kernel;
	KernelFunctions<void(*)(int, int, float*, float*, float*, float*, int*, int, int)>       filter_nlm_update_output_kernel;
	KernelFunctions<void(*)(float*, float*, int*, int)>                                      filter_nlm_normalize_kernel;

	KernelFunctions<void(*)(float*, int, int, int, float*, int*, int*, int, int, float)>                              filter_construct_transform_kernel;
	KernelFunctions<void(*)(int, int, float*, float*, float*, int*, float*, float3*, int*, int*, int, int, int, int)> filter_nlm_construct_gramian_kernel;
	KernelFunctions<void(*)(int, int, int, int, int, float*, int*, float*, float3*, int*, int)>                       filter_finalize_kernel;

	KernelFunctions<void(*)(KernelGlobals *, ccl_constant KernelData*, ccl_global void*, int, ccl_global char*,
	                       ccl_global uint*, int, int, int, int, int, int, int, int, ccl_global int*, int,
	                       ccl_global char*, ccl_global unsigned int*, unsigned int, ccl_global float*)>        data_init_kernel;
	unordered_map<string, KernelFunctions<void(*)(KernelGlobals*, KernelData*)> > split_kernels;

#define KERNEL_FUNCTIONS(name) \
	      KERNEL_NAME_EVAL(cpu, name), \
	      KERNEL_NAME_EVAL(cpu_sse2, name), \
	      KERNEL_NAME_EVAL(cpu_sse3, name), \
	      KERNEL_NAME_EVAL(cpu_sse41, name), \
	      KERNEL_NAME_EVAL(cpu_avx, name), \
	      KERNEL_NAME_EVAL(cpu_avx2, name)

	CPUDevice(DeviceInfo& info, Stats &stats, bool background)
	: Device(info, stats, background),
#define REGISTER_KERNEL(name) name ## _kernel(KERNEL_FUNCTIONS(name))
	  REGISTER_KERNEL(path_trace),
	  REGISTER_KERNEL(convert_to_half_float),
	  REGISTER_KERNEL(convert_to_byte),
	  REGISTER_KERNEL(shader),
	  REGISTER_KERNEL(filter_divide_shadow),
	  REGISTER_KERNEL(filter_get_feature),
	  REGISTER_KERNEL(filter_detect_outliers),
	  REGISTER_KERNEL(filter_combine_halves),
	  REGISTER_KERNEL(filter_nlm_calc_difference),
	  REGISTER_KERNEL(filter_nlm_blur),
	  REGISTER_KERNEL(filter_nlm_calc_weight),
	  REGISTER_KERNEL(filter_nlm_update_output),
	  REGISTER_KERNEL(filter_nlm_normalize),
	  REGISTER_KERNEL(filter_construct_transform),
	  REGISTER_KERNEL(filter_nlm_construct_gramian),
	  REGISTER_KERNEL(filter_finalize),
	  REGISTER_KERNEL(data_init)
#undef REGISTER_KERNEL
	{

#ifdef WITH_OSL
		kernel_globals.osl = &osl_globals;
#endif
		use_split_kernel = DebugFlags().cpu.split_kernel;
		if(use_split_kernel) {
			VLOG(1) << "Will be using split kernel.";
		}

#define REGISTER_SPLIT_KERNEL(name) split_kernels[#name] = KernelFunctions<void(*)(KernelGlobals*, KernelData*)>(KERNEL_FUNCTIONS(name))
		REGISTER_SPLIT_KERNEL(path_init);
		REGISTER_SPLIT_KERNEL(scene_intersect);
		REGISTER_SPLIT_KERNEL(lamp_emission);
		REGISTER_SPLIT_KERNEL(do_volume);
		REGISTER_SPLIT_KERNEL(queue_enqueue);
		REGISTER_SPLIT_KERNEL(indirect_background);
		REGISTER_SPLIT_KERNEL(shader_setup);
		REGISTER_SPLIT_KERNEL(shader_sort);
		REGISTER_SPLIT_KERNEL(shader_eval);
		REGISTER_SPLIT_KERNEL(holdout_emission_blurring_pathtermination_ao);
		REGISTER_SPLIT_KERNEL(subsurface_scatter);
		REGISTER_SPLIT_KERNEL(direct_lighting);
		REGISTER_SPLIT_KERNEL(shadow_blocked_ao);
		REGISTER_SPLIT_KERNEL(shadow_blocked_dl);
		REGISTER_SPLIT_KERNEL(enqueue_inactive);
		REGISTER_SPLIT_KERNEL(next_iteration_setup);
		REGISTER_SPLIT_KERNEL(indirect_subsurface);
		REGISTER_SPLIT_KERNEL(buffer_update);
#undef REGISTER_SPLIT_KERNEL
#undef KERNEL_FUNCTIONS
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

	virtual device_ptr mem_alloc_sub_ptr(device_memory& mem, int offset, int /*size*/, MemoryType /*type*/)
	{
		return (device_ptr) (((char*) mem.device_pointer) + mem.memory_elements_size(offset));
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
		if(task->type == DeviceTask::RENDER) {
			thread_render(*task);
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

	bool denoising_set_tiles(device_ptr *buffers, DenoisingTask *task)
	{
		mem_alloc("Denoising Tile Info", task->tiles_mem, MEM_READ_ONLY);

		TilesInfo *tiles = (TilesInfo*) task->tiles_mem.data_pointer;
		for(int i = 0; i < 9; i++) {
			tiles->buffers[i] = buffers[i];
		}

		return true;
	}

	bool denoising_non_local_means(device_ptr image_ptr, device_ptr guide_ptr, device_ptr variance_ptr, device_ptr out_ptr,
	                               DenoisingTask *task)
	{
		int4 rect = task->rect;
		int   r   = task->nlm_state.r;
		int   f   = task->nlm_state.f;
		float a   = task->nlm_state.a;
		float k_2 = task->nlm_state.k_2;

		int w = align_up(rect.z-rect.x, 4);
		int h = rect.w-rect.y;

		float *blurDifference = (float*) task->nlm_state.temporary_1_ptr;
		float *difference     = (float*) task->nlm_state.temporary_2_ptr;
		float *weightAccum    = (float*) task->nlm_state.temporary_3_ptr;

		memset(weightAccum, 0, sizeof(float)*w*h);
		memset((float*) out_ptr, 0, sizeof(float)*w*h);

		for(int i = 0; i < (2*r+1)*(2*r+1); i++) {
			int dy = i / (2*r+1) - r;
			int dx = i % (2*r+1) - r;

			int local_rect[4] = {max(0, -dx), max(0, -dy), rect.z-rect.x - max(0, dx), rect.w-rect.y - max(0, dy)};
			filter_nlm_calc_difference_kernel()(dx, dy,
			                                    (float*) guide_ptr,
			                                    (float*) variance_ptr,
			                                    difference,
			                                    local_rect,
			                                    w, 0,
			                                    a, k_2);

			filter_nlm_blur_kernel()       (difference, blurDifference, local_rect, w, f);
			filter_nlm_calc_weight_kernel()(blurDifference, difference, local_rect, w, f);
			filter_nlm_blur_kernel()       (difference, blurDifference, local_rect, w, f);

			filter_nlm_update_output_kernel()(dx, dy,
			                                  blurDifference,
			                                  (float*) image_ptr,
			                                  (float*) out_ptr,
			                                  weightAccum,
			                                  local_rect,
			                                  w, f);
		}

		int local_rect[4] = {0, 0, rect.z-rect.x, rect.w-rect.y};
		filter_nlm_normalize_kernel()((float*) out_ptr, weightAccum, local_rect, w);

		return true;
	}

	bool denoising_construct_transform(DenoisingTask *task)
	{
		for(int y = 0; y < task->filter_area.w; y++) {
			for(int x = 0; x < task->filter_area.z; x++) {
				filter_construct_transform_kernel()((float*) task->buffer.mem.device_pointer,
				                                    x + task->filter_area.x,
				                                    y + task->filter_area.y,
				                                    y*task->filter_area.z + x,
				                                    (float*) task->storage.transform.device_pointer,
				                                    (int*)   task->storage.rank.device_pointer,
				                                    &task->rect.x,
				                                    task->buffer.pass_stride,
				                                    task->radius,
				                                    task->pca_threshold);
			}
		}
		return true;
	}

	bool denoising_reconstruct(device_ptr color_ptr,
	                           device_ptr color_variance_ptr,
	                           device_ptr output_ptr,
	                           DenoisingTask *task)
	{
		mem_zero(task->storage.XtWX);
		mem_zero(task->storage.XtWY);

		float *difference     = (float*) task->reconstruction_state.temporary_1_ptr;
		float *blurDifference = (float*) task->reconstruction_state.temporary_2_ptr;

		int r = task->radius;
		for(int i = 0; i < (2*r+1)*(2*r+1); i++) {
			int dy = i / (2*r+1) - r;
			int dx = i % (2*r+1) - r;

			int local_rect[4] = {max(0, -dx), max(0, -dy),
			                     task->reconstruction_state.source_w - max(0, dx),
			                     task->reconstruction_state.source_h - max(0, dy)};
			filter_nlm_calc_difference_kernel()(dx, dy,
			                                    (float*) color_ptr,
			                                    (float*) color_variance_ptr,
			                                    difference,
			                                    local_rect,
			                                    task->buffer.w,
			                                    task->buffer.pass_stride,
			                                    1.0f,
			                                    task->nlm_k_2);
			filter_nlm_blur_kernel()(difference, blurDifference, local_rect, task->buffer.w, 4);
			filter_nlm_calc_weight_kernel()(blurDifference, difference, local_rect, task->buffer.w, 4);
			filter_nlm_blur_kernel()(difference, blurDifference, local_rect, task->buffer.w, 4);
			filter_nlm_construct_gramian_kernel()(dx, dy,
			                                      blurDifference,
			                                      (float*)  task->buffer.mem.device_pointer,
			                                      (float*)  task->storage.transform.device_pointer,
			                                      (int*)    task->storage.rank.device_pointer,
			                                      (float*)  task->storage.XtWX.device_pointer,
			                                      (float3*) task->storage.XtWY.device_pointer,
			                                      local_rect,
			                                      &task->reconstruction_state.filter_rect.x,
			                                      task->buffer.w,
			                                      task->buffer.h,
			                                      4,
			                                      task->buffer.pass_stride);
		}
		for(int y = 0; y < task->filter_area.w; y++) {
			for(int x = 0; x < task->filter_area.z; x++) {
				filter_finalize_kernel()(x,
				                         y,
				                         y*task->filter_area.z + x,
				                         task->buffer.w,
				                         task->buffer.h,
				                         (float*)  output_ptr,
				                         (int*)    task->storage.rank.device_pointer,
				                         (float*)  task->storage.XtWX.device_pointer,
				                         (float3*) task->storage.XtWY.device_pointer,
				                         &task->reconstruction_state.buffer_params.x,
				                         task->render_buffer.samples);
			}
		}
		return true;
	}

	bool denoising_combine_halves(device_ptr a_ptr, device_ptr b_ptr,
	                              device_ptr mean_ptr, device_ptr variance_ptr,
	                              int r, int4 rect, DenoisingTask * /*task*/)
	{
		for(int y = rect.y; y < rect.w; y++) {
			for(int x = rect.x; x < rect.z; x++) {
				filter_combine_halves_kernel()(x, y,
				                               (float*) mean_ptr,
				                               (float*) variance_ptr,
				                               (float*) a_ptr,
				                               (float*) b_ptr,
				                               &rect.x,
				                               r);
			}
		}
		return true;
	}

	bool denoising_divide_shadow(device_ptr a_ptr, device_ptr b_ptr,
	                             device_ptr sample_variance_ptr, device_ptr sv_variance_ptr,
	                             device_ptr buffer_variance_ptr, DenoisingTask *task)
	{
		for(int y = task->rect.y; y < task->rect.w; y++) {
			for(int x = task->rect.x; x < task->rect.z; x++) {
				filter_divide_shadow_kernel()(task->render_buffer.samples,
				                              task->tiles,
				                              x, y,
				                              (float*) a_ptr,
				                              (float*) b_ptr,
				                              (float*) sample_variance_ptr,
				                              (float*) sv_variance_ptr,
				                              (float*) buffer_variance_ptr,
				                              &task->rect.x,
				                              task->render_buffer.pass_stride,
				                              task->render_buffer.denoising_data_offset,
				                              use_split_kernel);
			}
		}
		return true;
	}

	bool denoising_get_feature(int mean_offset,
	                           int variance_offset,
	                           device_ptr mean_ptr,
	                           device_ptr variance_ptr,
	                           DenoisingTask *task)
	{
		for(int y = task->rect.y; y < task->rect.w; y++) {
			for(int x = task->rect.x; x < task->rect.z; x++) {
				filter_get_feature_kernel()(task->render_buffer.samples,
				                            task->tiles,
				                            mean_offset,
				                            variance_offset,
				                            x, y,
				                            (float*) mean_ptr,
				                            (float*) variance_ptr,
				                            &task->rect.x,
				                            task->render_buffer.pass_stride,
				                            task->render_buffer.denoising_data_offset,
				                            use_split_kernel);
			}
		}
		return true;
	}

	bool denoising_detect_outliers(device_ptr image_ptr,
	                               device_ptr variance_ptr,
	                               device_ptr depth_ptr,
	                               device_ptr output_ptr,
	                               DenoisingTask *task)
	{
		for(int y = task->rect.y; y < task->rect.w; y++) {
			for(int x = task->rect.x; x < task->rect.z; x++) {
				filter_detect_outliers_kernel()(x, y,
				                                (float*) image_ptr,
				                                (float*) variance_ptr,
				                                (float*) depth_ptr,
				                                (float*) output_ptr,
				                                &task->rect.x,
				                                task->buffer.pass_stride);
			}
		}
		return true;
	}

	void path_trace(DeviceTask &task, RenderTile &tile, KernelGlobals *kg)
	{
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
					path_trace_kernel()(kg, render_buffer, rng_state,
					                    sample, x, y, tile.offset, tile.stride);
				}
			}

			tile.sample = sample + 1;

			task.update_progress(&tile, tile.w*tile.h);
		}
	}

	void denoise(DeviceTask &task, RenderTile &tile)
	{
		tile.sample = tile.start_sample + tile.num_samples;

		DenoisingTask denoising(this);

		denoising.functions.construct_transform = function_bind(&CPUDevice::denoising_construct_transform, this, &denoising);
		denoising.functions.reconstruct = function_bind(&CPUDevice::denoising_reconstruct, this, _1, _2, _3, &denoising);
		denoising.functions.divide_shadow = function_bind(&CPUDevice::denoising_divide_shadow, this, _1, _2, _3, _4, _5, &denoising);
		denoising.functions.non_local_means = function_bind(&CPUDevice::denoising_non_local_means, this, _1, _2, _3, _4, &denoising);
		denoising.functions.combine_halves = function_bind(&CPUDevice::denoising_combine_halves, this, _1, _2, _3, _4, _5, _6, &denoising);
		denoising.functions.get_feature = function_bind(&CPUDevice::denoising_get_feature, this, _1, _2, _3, _4, &denoising);
		denoising.functions.detect_outliers = function_bind(&CPUDevice::denoising_detect_outliers, this, _1, _2, _3, _4, &denoising);
		denoising.functions.set_tiles = function_bind(&CPUDevice::denoising_set_tiles, this, _1, &denoising);

		denoising.filter_area = make_int4(tile.x, tile.y, tile.w, tile.h);
		denoising.render_buffer.samples = tile.sample;

		RenderTile rtiles[9];
		rtiles[4] = tile;
		task.map_neighbor_tiles(rtiles, this);
		denoising.tiles_from_rendertiles(rtiles);

		denoising.init_from_devicetask(task);

		denoising.run_denoising();

		task.unmap_neighbor_tiles(rtiles, this);

		task.update_progress(&tile, tile.w*tile.h);
	}

	void thread_render(DeviceTask& task)
	{
		if(task_pool.canceled()) {
			if(task.need_finish_queue == false)
				return;
		}

		/* allocate buffer for kernel globals */
		device_only_memory<KernelGlobals> kgbuffer;
		kgbuffer.resize(1);
		mem_alloc("kernel_globals", kgbuffer, MEM_READ_WRITE);

		KernelGlobals *kg = new ((void*) kgbuffer.device_pointer) KernelGlobals(thread_kernel_globals_init());

		CPUSplitKernel *split_kernel = NULL;
		if(use_split_kernel) {
			split_kernel = new CPUSplitKernel(this);
			requested_features.max_closure = MAX_CLOSURE;
			if(!split_kernel->load_kernels(requested_features)) {
				thread_kernel_globals_free((KernelGlobals*)kgbuffer.device_pointer);
				mem_free(kgbuffer);

				delete split_kernel;
				return;
			}
		}

		RenderTile tile;
		while(task.acquire_tile(this, tile)) {
			if(tile.task == RenderTile::PATH_TRACE) {
				if(use_split_kernel) {
					device_memory data;
					split_kernel->path_trace(&task, tile, kgbuffer, data);
				}
				else {
					path_trace(task, tile, kg);
				}
			}
			else if(tile.task == RenderTile::DENOISE) {
				denoise(task, tile);
			}

			task.release_tile(tile);

			if(task_pool.canceled()) {
				if(task.need_finish_queue == false)
					break;
			}
		}

		thread_kernel_globals_free((KernelGlobals*)kgbuffer.device_pointer);
		kg->~KernelGlobals();
		mem_free(kgbuffer);
		delete split_kernel;
	}

	void thread_film_convert(DeviceTask& task)
	{
		float sample_scale = 1.0f/(task.sample + 1);

		if(task.rgba_half) {
			for(int y = task.y; y < task.y + task.h; y++)
				for(int x = task.x; x < task.x + task.w; x++)
					convert_to_half_float_kernel()(&kernel_globals, (uchar4*)task.rgba_half, (float*)task.buffer,
					                               sample_scale, x, y, task.offset, task.stride);
		}
		else {
			for(int y = task.y; y < task.y + task.h; y++)
				for(int x = task.x; x < task.x + task.w; x++)
					convert_to_byte_kernel()(&kernel_globals, (uchar4*)task.rgba_byte, (float*)task.buffer,
					                         sample_scale, x, y, task.offset, task.stride);

		}
	}

	void thread_shader(DeviceTask& task)
	{
		KernelGlobals kg = kernel_globals;

#ifdef WITH_OSL
		OSLShader::thread_init(&kg, &kernel_globals, &osl_globals);
#endif
		for(int sample = 0; sample < task.num_samples; sample++) {
			for(int x = task.shader_x; x < task.shader_x + task.shader_w; x++)
				shader_kernel()(&kg,
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
	KernelGlobals *kg = (KernelGlobals*)kernel_globals.device_pointer;
	kg->global_size = make_int2(dim.global_size[0], dim.global_size[1]);

	for(int y = 0; y < dim.global_size[1]; y++) {
		for(int x = 0; x < dim.global_size[0]; x++) {
			kg->global_id = make_int2(x, y);

			device->data_init_kernel()((KernelGlobals*)kernel_globals.device_pointer,
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

SplitKernelFunction* CPUSplitKernel::get_split_kernel_function(const string& kernel_name,
                                                               const DeviceRequestedFeatures&)
{
	CPUSplitKernelFunction *kernel = new CPUSplitKernelFunction(device);

	kernel->func = device->split_kernels[kernel_name]();
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

int2 CPUSplitKernel::split_kernel_global_size(device_memory& /*kg*/, device_memory& /*data*/, DeviceTask * /*task*/) {
	return make_int2(1, 1);
}

uint64_t CPUSplitKernel::state_buffer_size(device_memory& kernel_globals, device_memory& /*data*/, size_t num_threads) {
	KernelGlobals *kg = (KernelGlobals*)kernel_globals.device_pointer;

	return split_data_buffer_size(kg, num_threads);
}

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

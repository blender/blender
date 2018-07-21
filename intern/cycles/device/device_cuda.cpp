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

#include <climits>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device/device.h"
#include "device/device_denoising.h"
#include "device/device_intern.h"
#include "device/device_split_kernel.h"

#include "render/buffers.h"

#include "kernel/filter/filter_defines.h"

#ifdef WITH_CUDA_DYNLOAD
#  include "cuew.h"
#else
#  include "util/util_opengl.h"
#  include <cuda.h>
#  include <cudaGL.h>
#endif
#include "util/util_debug.h"
#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_map.h"
#include "util/util_md5.h"
#include "util/util_opengl.h"
#include "util/util_path.h"
#include "util/util_string.h"
#include "util/util_system.h"
#include "util/util_types.h"
#include "util/util_time.h"

#include "kernel/split/kernel_split_data_types.h"

CCL_NAMESPACE_BEGIN

#ifndef WITH_CUDA_DYNLOAD

/* Transparently implement some functions, so majority of the file does not need
 * to worry about difference between dynamically loaded and linked CUDA at all.
 */

namespace {

const char *cuewErrorString(CUresult result)
{
	/* We can only give error code here without major code duplication, that
	 * should be enough since dynamic loading is only being disabled by folks
	 * who knows what they're doing anyway.
	 *
	 * NOTE: Avoid call from several threads.
	 */
	static string error;
	error = string_printf("%d", result);
	return error.c_str();
}

const char *cuewCompilerPath(void)
{
	return CYCLES_CUDA_NVCC_EXECUTABLE;
}

int cuewCompilerVersion(void)
{
	return (CUDA_VERSION / 100) + (CUDA_VERSION % 100 / 10);
}

}  /* namespace */
#endif  /* WITH_CUDA_DYNLOAD */

class CUDADevice;

class CUDASplitKernel : public DeviceSplitKernel {
	CUDADevice *device;
public:
	explicit CUDASplitKernel(CUDADevice *device);

	virtual uint64_t state_buffer_size(device_memory& kg, device_memory& data, size_t num_threads);

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
};

/* Utility to push/pop CUDA context. */
class CUDAContextScope {
public:
	CUDAContextScope(CUDADevice *device);
	~CUDAContextScope();

private:
	CUDADevice *device;
};

class CUDADevice : public Device
{
public:
	DedicatedTaskPool task_pool;
	CUdevice cuDevice;
	CUcontext cuContext;
	CUmodule cuModule, cuFilterModule;
	size_t device_texture_headroom;
	size_t device_working_headroom;
	bool move_texture_to_host;
	size_t map_host_used;
	size_t map_host_limit;
	int can_map_host;
	int cuDevId;
	int cuDevArchitecture;
	bool first_error;
	CUDASplitKernel *split_kernel;

	struct CUDAMem {
		CUDAMem()
		: texobject(0), array(0), map_host_pointer(0), free_map_host(false) {}

		CUtexObject texobject;
		CUarray array;
		void *map_host_pointer;
		bool free_map_host;
	};
	typedef map<device_memory*, CUDAMem> CUDAMemMap;
	CUDAMemMap cuda_mem_map;

	struct PixelMem {
		GLuint cuPBO;
		CUgraphicsResource cuPBOresource;
		GLuint cuTexId;
		int w, h;
	};
	map<device_ptr, PixelMem> pixel_mem_map;

	/* Bindless Textures */
	device_vector<TextureInfo> texture_info;
	bool need_texture_info;

	CUdeviceptr cuda_device_ptr(device_ptr mem)
	{
		return (CUdeviceptr)mem;
	}

	static bool have_precompiled_kernels()
	{
		string cubins_path = path_get("lib");
		return path_exists(cubins_path);
	}

	virtual bool show_samples() const
	{
		/* The CUDADevice only processes one tile at a time, so showing samples is fine. */
		return true;
	}

/*#ifdef NDEBUG
#define cuda_abort()
#else
#define cuda_abort() abort()
#endif*/
	void cuda_error_documentation()
	{
		if(first_error) {
			fprintf(stderr, "\nRefer to the Cycles GPU rendering documentation for possible solutions:\n");
			fprintf(stderr, "https://docs.blender.org/manual/en/dev/render/cycles/gpu_rendering.html\n\n");
			first_error = false;
		}
	}

#define cuda_assert(stmt) \
	{ \
		CUresult result = stmt; \
		\
		if(result != CUDA_SUCCESS) { \
			string message = string_printf("CUDA error: %s in %s, line %d", cuewErrorString(result), #stmt, __LINE__); \
			if(error_msg == "") \
				error_msg = message; \
			fprintf(stderr, "%s\n", message.c_str()); \
			/*cuda_abort();*/ \
			cuda_error_documentation(); \
		} \
	} (void)0

	bool cuda_error_(CUresult result, const string& stmt)
	{
		if(result == CUDA_SUCCESS)
			return false;

		string message = string_printf("CUDA error at %s: %s", stmt.c_str(), cuewErrorString(result));
		if(error_msg == "")
			error_msg = message;
		fprintf(stderr, "%s\n", message.c_str());
		cuda_error_documentation();
		return true;
	}

#define cuda_error(stmt) cuda_error_(stmt, #stmt)

	void cuda_error_message(const string& message)
	{
		if(error_msg == "")
			error_msg = message;
		fprintf(stderr, "%s\n", message.c_str());
		cuda_error_documentation();
	}

	CUDADevice(DeviceInfo& info, Stats &stats, bool background_)
	: Device(info, stats, background_),
	  texture_info(this, "__texture_info", MEM_TEXTURE)
	{
		first_error = true;
		background = background_;

		cuDevId = info.num;
		cuDevice = 0;
		cuContext = 0;

		cuModule = 0;
		cuFilterModule = 0;

		split_kernel = NULL;

		need_texture_info = false;

		device_texture_headroom = 0;
		device_working_headroom = 0;
		move_texture_to_host = false;
		map_host_limit = 0;
		map_host_used = 0;
		can_map_host = 0;

		/* Intialize CUDA. */
		if(cuda_error(cuInit(0)))
			return;

		/* Setup device and context. */
		if(cuda_error(cuDeviceGet(&cuDevice, cuDevId)))
			return;

		/* CU_CTX_MAP_HOST for mapping host memory when out of device memory.
		 * CU_CTX_LMEM_RESIZE_TO_MAX for reserving local memory ahead of render,
		 * so we can predict which memory to map to host. */
		cuda_assert(cuDeviceGetAttribute(&can_map_host, CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY, cuDevice));

		unsigned int ctx_flags = CU_CTX_LMEM_RESIZE_TO_MAX;
		if(can_map_host) {
			ctx_flags |= CU_CTX_MAP_HOST;
			init_host_memory();
		}

		/* Create context. */
		CUresult result;

		if(background) {
			result = cuCtxCreate(&cuContext, ctx_flags, cuDevice);
		}
		else {
			result = cuGLCtxCreate(&cuContext, ctx_flags, cuDevice);

			if(result != CUDA_SUCCESS) {
				result = cuCtxCreate(&cuContext, ctx_flags, cuDevice);
				background = true;
			}
		}

		if(cuda_error_(result, "cuCtxCreate"))
			return;

		int major, minor;
		cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cuDevId);
		cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cuDevId);
		cuDevArchitecture = major*100 + minor*10;

		/* Pop context set by cuCtxCreate. */
		cuCtxPopCurrent(NULL);
	}

	~CUDADevice()
	{
		task_pool.stop();

		delete split_kernel;

		texture_info.free();

		cuda_assert(cuCtxDestroy(cuContext));
	}

	bool support_device(const DeviceRequestedFeatures& /*requested_features*/)
	{
		int major, minor;
		cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cuDevId);
		cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cuDevId);

		/* We only support sm_30 and above */
		if(major < 3) {
			cuda_error_message(string_printf("CUDA device supported only with compute capability 3.0 or up, found %d.%d.", major, minor));
			return false;
		}

		return true;
	}

	bool use_adaptive_compilation()
	{
		return DebugFlags().cuda.adaptive_compile;
	}

	bool use_split_kernel()
	{
		return DebugFlags().cuda.split_kernel;
	}

	/* Common NVCC flags which stays the same regardless of shading model,
	 * kernel sources md5 and only depends on compiler or compilation settings.
	 */
	string compile_kernel_get_common_cflags(
	        const DeviceRequestedFeatures& requested_features,
	        bool filter=false, bool split=false)
	{
		const int machine = system_cpu_bits();
		const string source_path = path_get("source");
		const string include_path = source_path;
		string cflags = string_printf("-m%d "
		                              "--ptxas-options=\"-v\" "
		                              "--use_fast_math "
		                              "-DNVCC "
		                               "-I\"%s\"",
		                              machine,
		                              include_path.c_str());
		if(!filter && use_adaptive_compilation()) {
			cflags += " " + requested_features.get_build_options();
		}
		const char *extra_cflags = getenv("CYCLES_CUDA_EXTRA_CFLAGS");
		if(extra_cflags) {
			cflags += string(" ") + string(extra_cflags);
		}
#ifdef WITH_CYCLES_DEBUG
		cflags += " -D__KERNEL_DEBUG__";
#endif

		if(split) {
			cflags += " -D__SPLIT__";
		}

		return cflags;
	}

	bool compile_check_compiler() {
		const char *nvcc = cuewCompilerPath();
		if(nvcc == NULL) {
			cuda_error_message("CUDA nvcc compiler not found. "
			                   "Install CUDA toolkit in default location.");
			return false;
		}
		const int cuda_version = cuewCompilerVersion();
		VLOG(1) << "Found nvcc " << nvcc
		        << ", CUDA version " << cuda_version
		        << ".";
		const int major = cuda_version / 10, minor = cuda_version % 10;
		if(cuda_version == 0) {
			cuda_error_message("CUDA nvcc compiler version could not be parsed.");
			return false;
		}
		if(cuda_version < 80) {
			printf("Unsupported CUDA version %d.%d detected, "
			       "you need CUDA 8.0 or newer.\n",
			       major, minor);
			return false;
		}
		else if(cuda_version != 80) {
			printf("CUDA version %d.%d detected, build may succeed but only "
			       "CUDA 8.0 is officially supported.\n",
			       major, minor);
		}
		return true;
	}

	string compile_kernel(const DeviceRequestedFeatures& requested_features,
	                      bool filter=false, bool split=false)
	{
		const char *name, *source;
		if(filter) {
			name = "filter";
			source = "filter.cu";
		}
		else if(split) {
			name = "kernel_split";
			source = "kernel_split.cu";
		}
		else {
			name = "kernel";
			source = "kernel.cu";
		}
		/* Compute cubin name. */
		int major, minor;
		cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cuDevId);
		cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cuDevId);

		/* Attempt to use kernel provided with Blender. */
		if(!use_adaptive_compilation()) {
			const string cubin = path_get(string_printf("lib/%s_sm_%d%d.cubin",
			                                            name, major, minor));
			VLOG(1) << "Testing for pre-compiled kernel " << cubin << ".";
			if(path_exists(cubin)) {
				VLOG(1) << "Using precompiled kernel.";
				return cubin;
			}
		}

		const string common_cflags =
		        compile_kernel_get_common_cflags(requested_features, filter, split);

		/* Try to use locally compiled kernel. */
		const string source_path = path_get("source");
		const string kernel_md5 = path_files_md5_hash(source_path);

		/* We include cflags into md5 so changing cuda toolkit or changing other
		 * compiler command line arguments makes sure cubin gets re-built.
		 */
		const string cubin_md5 = util_md5_string(kernel_md5 + common_cflags);

		const string cubin_file = string_printf("cycles_%s_sm%d%d_%s.cubin",
		                                        name, major, minor,
		                                        cubin_md5.c_str());
		const string cubin = path_cache_get(path_join("kernels", cubin_file));
		VLOG(1) << "Testing for locally compiled kernel " << cubin << ".";
		if(path_exists(cubin)) {
			VLOG(1) << "Using locally compiled kernel.";
			return cubin;
		}

#ifdef _WIN32
		if(have_precompiled_kernels()) {
			if(major < 3) {
				cuda_error_message(string_printf(
				        "CUDA device requires compute capability 3.0 or up, "
				        "found %d.%d. Your GPU is not supported.",
				        major, minor));
			}
			else {
				cuda_error_message(string_printf(
				        "CUDA binary kernel for this graphics card compute "
				        "capability (%d.%d) not found.",
				        major, minor));
			}
			return "";
		}
#endif

		/* Compile. */
		if(!compile_check_compiler()) {
			return "";
		}
		const char *nvcc = cuewCompilerPath();
		const string kernel = path_join(
		        path_join(source_path, "kernel"),
		        path_join("kernels",
		                  path_join("cuda", source)));
		double starttime = time_dt();
		printf("Compiling CUDA kernel ...\n");

		path_create_directories(cubin);

		string command = string_printf("\"%s\" "
		                               "-arch=sm_%d%d "
		                               "--cubin \"%s\" "
		                               "-o \"%s\" "
		                               "%s ",
		                               nvcc,
		                               major, minor,
		                               kernel.c_str(),
		                               cubin.c_str(),
		                               common_cflags.c_str());

		printf("%s\n", command.c_str());

		if(system(command.c_str()) == -1) {
			cuda_error_message("Failed to execute compilation command, "
			                   "see console for details.");
			return "";
		}

		/* Verify if compilation succeeded */
		if(!path_exists(cubin)) {
			cuda_error_message("CUDA kernel compilation failed, "
			                   "see console for details.");
			return "";
		}

		printf("Kernel compilation finished in %.2lfs.\n", time_dt() - starttime);

		return cubin;
	}

	bool load_kernels(const DeviceRequestedFeatures& requested_features)
	{
		/* TODO(sergey): Support kernels re-load for CUDA devices.
		 *
		 * Currently re-loading kernel will invalidate memory pointers,
		 * causing problems in cuCtxSynchronize.
		 */
		if(cuFilterModule && cuModule) {
			VLOG(1) << "Skipping kernel reload, not currently supported.";
			return true;
		}

		/* check if cuda init succeeded */
		if(cuContext == 0)
			return false;

		/* check if GPU is supported */
		if(!support_device(requested_features))
			return false;

		/* get kernel */
		string cubin = compile_kernel(requested_features, false, use_split_kernel());
		if(cubin == "")
			return false;

		string filter_cubin = compile_kernel(requested_features, true, false);
		if(filter_cubin == "")
			return false;

		/* open module */
		CUDAContextScope scope(this);

		string cubin_data;
		CUresult result;

		if(path_read_text(cubin, cubin_data))
			result = cuModuleLoadData(&cuModule, cubin_data.c_str());
		else
			result = CUDA_ERROR_FILE_NOT_FOUND;

		if(cuda_error_(result, "cuModuleLoad"))
			cuda_error_message(string_printf("Failed loading CUDA kernel %s.", cubin.c_str()));

		if(path_read_text(filter_cubin, cubin_data))
			result = cuModuleLoadData(&cuFilterModule, cubin_data.c_str());
		else
			result = CUDA_ERROR_FILE_NOT_FOUND;

		if(cuda_error_(result, "cuModuleLoad"))
			cuda_error_message(string_printf("Failed loading CUDA kernel %s.", filter_cubin.c_str()));

		if(result == CUDA_SUCCESS) {
			reserve_local_memory(requested_features);
		}

		return (result == CUDA_SUCCESS);
	}

	void reserve_local_memory(const DeviceRequestedFeatures& requested_features)
	{
		if(use_split_kernel()) {
			/* Split kernel mostly uses global memory and adaptive compilation,
			 * difficult to predict how much is needed currently. */
			return;
		}

		/* Together with CU_CTX_LMEM_RESIZE_TO_MAX, this reserves local memory
		 * needed for kernel launches, so that we can reliably figure out when
		 * to allocate scene data in mapped host memory. */
		CUDAContextScope scope(this);

		size_t total = 0, free_before = 0, free_after = 0;
		cuMemGetInfo(&free_before, &total);

		/* Get kernel function. */
		CUfunction cuPathTrace;

		if(requested_features.use_integrator_branched) {
			cuda_assert(cuModuleGetFunction(&cuPathTrace, cuModule, "kernel_cuda_branched_path_trace"));
		}
		else {
			cuda_assert(cuModuleGetFunction(&cuPathTrace, cuModule, "kernel_cuda_path_trace"));
		}

		cuda_assert(cuFuncSetCacheConfig(cuPathTrace, CU_FUNC_CACHE_PREFER_L1));

		int min_blocks, num_threads_per_block;
		cuda_assert(cuOccupancyMaxPotentialBlockSize(&min_blocks, &num_threads_per_block, cuPathTrace, NULL, 0, 0));

		/* Launch kernel, using just 1 block appears sufficient to reserve
		 * memory for all multiprocessors. It would be good to do this in
		 * parallel for the multi GPU case still to make it faster. */
		CUdeviceptr d_work_tiles = 0;
		uint total_work_size = 0;

		void *args[] = {&d_work_tiles,
		                &total_work_size};

		cuda_assert(cuLaunchKernel(cuPathTrace,
		                           1, 1, 1,
		                           num_threads_per_block, 1, 1,
		                           0, 0, args, 0));

		cuda_assert(cuCtxSynchronize());

		cuMemGetInfo(&free_after, &total);
		VLOG(1) << "Local memory reserved "
		        << string_human_readable_number(free_before - free_after) << " bytes. ("
		        << string_human_readable_size(free_before - free_after) << ")";

#if 0
		/* For testing mapped host memory, fill up device memory. */
		const size_t keep_mb = 1024;

		while(free_after > keep_mb * 1024 * 1024LL) {
			CUdeviceptr tmp;
			cuda_assert(cuMemAlloc(&tmp, 10 * 1024 * 1024LL));
			cuMemGetInfo(&free_after, &total);
		}
#endif
	}

	void init_host_memory()
	{
		/* Limit amount of host mapped memory, because allocating too much can
		 * cause system instability. Leave at least half or 4 GB of system
		 * memory free, whichever is smaller. */
		size_t default_limit = 4 * 1024 * 1024 * 1024LL;
		size_t system_ram = system_physical_ram();

		if(system_ram > 0) {
			if(system_ram / 2 > default_limit) {
				map_host_limit = system_ram - default_limit;
			}
			else {
				map_host_limit = system_ram / 2;
			}
		}
		else {
			VLOG(1) << "Mapped host memory disabled, failed to get system RAM";
			map_host_limit = 0;
		}

		/* Amount of device memory to keep is free after texture memory
		 * and working memory allocations respectively. We set the working
		 * memory limit headroom lower so that some space is left after all
		 * texture memory allocations. */
		device_working_headroom = 32 * 1024 * 1024LL; // 32MB
		device_texture_headroom = 128 * 1024 * 1024LL; // 128MB

		VLOG(1) << "Mapped host memory limit set to "
		        << string_human_readable_number(map_host_limit) << " bytes. ("
		        << string_human_readable_size(map_host_limit) << ")";
	}

	void load_texture_info()
	{
		if(need_texture_info) {
			texture_info.copy_to_device();
			need_texture_info = false;
		}
	}

	void move_textures_to_host(size_t size, bool for_texture)
	{
		/* Signal to reallocate textures in host memory only. */
		move_texture_to_host = true;

		while(size > 0) {
			/* Find suitable memory allocation to move. */
			device_memory *max_mem = NULL;
			size_t max_size = 0;
			bool max_is_image = false;

			foreach(CUDAMemMap::value_type& pair, cuda_mem_map) {
				device_memory& mem = *pair.first;
				CUDAMem *cmem = &pair.second;

				bool is_texture = (mem.type == MEM_TEXTURE) && (&mem != &texture_info);
				bool is_image = is_texture && (mem.data_height > 1);

				/* Can't move this type of memory. */
				if(!is_texture || cmem->array) {
					continue;
				}

				/* Already in host memory. */
				if(cmem->map_host_pointer) {
					continue;
				}

				/* For other textures, only move image textures. */
				if(for_texture && !is_image) {
					continue;
				}

				/* Try to move largest allocation, prefer moving images. */
				if(is_image > max_is_image ||
				   (is_image == max_is_image && mem.device_size > max_size)) {
					max_is_image = is_image;
					max_size = mem.device_size;
					max_mem = &mem;
				}
			}

			/* Move to host memory. This part is mutex protected since
			 * multiple CUDA devices could be moving the memory. The
			 * first one will do it, and the rest will adopt the pointer. */
			if(max_mem) {
				VLOG(1) << "Move memory from device to host: " << max_mem->name;

				static thread_mutex move_mutex;
				thread_scoped_lock lock(move_mutex);

				/* Preserve the original device pointer, in case of multi device
				 * we can't change it because the pointer mapping would break. */
				device_ptr prev_pointer = max_mem->device_pointer;
				size_t prev_size = max_mem->device_size;

				tex_free(*max_mem);
				tex_alloc(*max_mem);
				size = (max_size >= size)? 0: size - max_size;

				max_mem->device_pointer = prev_pointer;
				max_mem->device_size = prev_size;
			}
			else {
				break;
			}
		}

		/* Update texture info array with new pointers. */
		load_texture_info();

		move_texture_to_host = false;
	}

	CUDAMem *generic_alloc(device_memory& mem, size_t pitch_padding = 0)
	{
		CUDAContextScope scope(this);

		CUdeviceptr device_pointer = 0;
		size_t size = mem.memory_size() + pitch_padding;

		CUresult mem_alloc_result = CUDA_ERROR_OUT_OF_MEMORY;
		const char *status = "";

		/* First try allocating in device memory, respecting headroom. We make
		 * an exception for texture info. It is small and frequently accessed,
		 * so treat it as working memory.
		 *
		 * If there is not enough room for working memory, we will try to move
		 * textures to host memory, assuming the performance impact would have
		 * been worse for working memory. */
		bool is_texture = (mem.type == MEM_TEXTURE) && (&mem != &texture_info);
		bool is_image = is_texture && (mem.data_height > 1);

		size_t headroom = (is_texture)? device_texture_headroom:
		                                device_working_headroom;

		size_t total = 0, free = 0;
		cuMemGetInfo(&free, &total);

		/* Move textures to host memory if needed. */
		if(!move_texture_to_host && !is_image && (size + headroom) >= free) {
			move_textures_to_host(size + headroom - free, is_texture);
			cuMemGetInfo(&free, &total);
		}

		/* Allocate in device memory. */
		if(!move_texture_to_host && (size + headroom) < free) {
			mem_alloc_result = cuMemAlloc(&device_pointer, size);
			if(mem_alloc_result == CUDA_SUCCESS) {
				status = " in device memory";
			}
		}

		/* Fall back to mapped host memory if needed and possible. */
		void *map_host_pointer = 0;
		bool free_map_host = false;

		if(mem_alloc_result != CUDA_SUCCESS && can_map_host &&
		   map_host_used + size < map_host_limit) {
			if(mem.shared_pointer) {
				/* Another device already allocated host memory. */
				mem_alloc_result = CUDA_SUCCESS;
				map_host_pointer = mem.shared_pointer;
			}
			else {
				/* Allocate host memory ourselves. */
				mem_alloc_result = cuMemHostAlloc(&map_host_pointer, size,
				                                  CU_MEMHOSTALLOC_DEVICEMAP |
				                                  CU_MEMHOSTALLOC_WRITECOMBINED);
				mem.shared_pointer = map_host_pointer;
				free_map_host = true;
			}

			if(mem_alloc_result == CUDA_SUCCESS) {
				cuda_assert(cuMemHostGetDevicePointer_v2(&device_pointer, mem.shared_pointer, 0));
				map_host_used += size;
				status = " in host memory";

				/* Replace host pointer with our host allocation. Only works if
				 * CUDA memory layout is the same and has no pitch padding. Also
				 * does not work if we move textures to host during a render,
				 * since other devices might be using the memory. */
				if(!move_texture_to_host && pitch_padding == 0 &&
				   mem.host_pointer && mem.host_pointer != mem.shared_pointer) {
					memcpy(mem.shared_pointer, mem.host_pointer, size);
					mem.host_free();
					mem.host_pointer = mem.shared_pointer;
				}
			}
			else {
				status = " failed, out of host memory";
			}
		}
		else if(mem_alloc_result != CUDA_SUCCESS) {
			status = " failed, out of device and host memory";
		}

		if(mem_alloc_result != CUDA_SUCCESS) {
			cuda_assert(mem_alloc_result);
		}

		if(mem.name) {
			VLOG(1) << "Buffer allocate: " << mem.name << ", "
					<< string_human_readable_number(mem.memory_size()) << " bytes. ("
					<< string_human_readable_size(mem.memory_size()) << ")"
					<< status;
		}

		mem.device_pointer = (device_ptr)device_pointer;
		mem.device_size = size;
		stats.mem_alloc(size);

		if(!mem.device_pointer) {
			return NULL;
		}

		/* Insert into map of allocations. */
		CUDAMem *cmem = &cuda_mem_map[&mem];
		cmem->map_host_pointer = map_host_pointer;
		cmem->free_map_host = free_map_host;
		return cmem;
	}

	void generic_copy_to(device_memory& mem)
	{
		if(mem.host_pointer && mem.device_pointer) {
			CUDAContextScope scope(this);

			if(mem.host_pointer != mem.shared_pointer) {
				cuda_assert(cuMemcpyHtoD(cuda_device_ptr(mem.device_pointer),
				                         mem.host_pointer,
				                         mem.memory_size()));
			}
		}
	}

	void generic_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			CUDAContextScope scope(this);
			const CUDAMem& cmem = cuda_mem_map[&mem];

			if(cmem.map_host_pointer) {
				/* Free host memory. */
				if(cmem.free_map_host) {
					cuMemFreeHost(cmem.map_host_pointer);
					if(mem.host_pointer == mem.shared_pointer) {
						mem.host_pointer = 0;
					}
					mem.shared_pointer = 0;
				}

				map_host_used -= mem.device_size;
			}
			else {
				/* Free device memory. */
				cuMemFree(mem.device_pointer);
			}

			stats.mem_free(mem.device_size);
			mem.device_pointer = 0;
			mem.device_size = 0;

			cuda_mem_map.erase(cuda_mem_map.find(&mem));
		}
	}

	void mem_alloc(device_memory& mem)
	{
		if(mem.type == MEM_PIXELS && !background) {
			pixels_alloc(mem);
		}
		else if(mem.type == MEM_TEXTURE) {
			assert(!"mem_alloc not supported for textures.");
		}
		else {
			generic_alloc(mem);
		}
	}

	void mem_copy_to(device_memory& mem)
	{
		if(mem.type == MEM_PIXELS) {
			assert(!"mem_copy_to not supported for pixels.");
		}
		else if(mem.type == MEM_TEXTURE) {
			tex_free(mem);
			tex_alloc(mem);
		}
		else {
			if(!mem.device_pointer) {
				generic_alloc(mem);
			}

			generic_copy_to(mem);
		}
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		if(mem.type == MEM_PIXELS && !background) {
			pixels_copy_from(mem, y, w, h);
		}
		else if(mem.type == MEM_TEXTURE) {
			assert(!"mem_copy_from not supported for textures.");
		}
		else {
			CUDAContextScope scope(this);
			size_t offset = elem*y*w;
			size_t size = elem*w*h;

			if(mem.host_pointer && mem.device_pointer) {
				cuda_assert(cuMemcpyDtoH((uchar*)mem.host_pointer + offset,
										 (CUdeviceptr)(mem.device_pointer + offset), size));
			}
			else if(mem.host_pointer) {
				memset((char*)mem.host_pointer + offset, 0, size);
			}
		}
	}

	void mem_zero(device_memory& mem)
	{
		if(!mem.device_pointer) {
			mem_alloc(mem);
		}

		if(mem.host_pointer) {
			memset(mem.host_pointer, 0, mem.memory_size());
		}

		if(mem.device_pointer &&
		   (!mem.host_pointer || mem.host_pointer != mem.shared_pointer)) {
			CUDAContextScope scope(this);
			cuda_assert(cuMemsetD8(cuda_device_ptr(mem.device_pointer), 0, mem.memory_size()));
		}
	}

	void mem_free(device_memory& mem)
	{
		if(mem.type == MEM_PIXELS && !background) {
			pixels_free(mem);
		}
		else if(mem.type == MEM_TEXTURE) {
			tex_free(mem);
		}
		else {
			generic_free(mem);
		}
	}

	virtual device_ptr mem_alloc_sub_ptr(device_memory& mem, int offset, int /*size*/)
	{
		return (device_ptr) (((char*) mem.device_pointer) + mem.memory_elements_size(offset));
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		CUDAContextScope scope(this);
		CUdeviceptr mem;
		size_t bytes;

		cuda_assert(cuModuleGetGlobal(&mem, &bytes, cuModule, name));
		//assert(bytes == size);
		cuda_assert(cuMemcpyHtoD(mem, host, size));
	}

	void tex_alloc(device_memory& mem)
	{
		CUDAContextScope scope(this);

		/* General variables for both architectures */
		string bind_name = mem.name;
		size_t dsize = datatype_size(mem.data_type);
		size_t size = mem.memory_size();

		CUaddress_mode address_mode = CU_TR_ADDRESS_MODE_WRAP;
		switch(mem.extension) {
			case EXTENSION_REPEAT:
				address_mode = CU_TR_ADDRESS_MODE_WRAP;
				break;
			case EXTENSION_EXTEND:
				address_mode = CU_TR_ADDRESS_MODE_CLAMP;
				break;
			case EXTENSION_CLIP:
				address_mode = CU_TR_ADDRESS_MODE_BORDER;
				break;
			default:
				assert(0);
				break;
		}

		CUfilter_mode filter_mode;
		if(mem.interpolation == INTERPOLATION_CLOSEST) {
			filter_mode = CU_TR_FILTER_MODE_POINT;
		}
		else {
			filter_mode = CU_TR_FILTER_MODE_LINEAR;
		}

		/* Data Storage */
		if(mem.interpolation == INTERPOLATION_NONE) {
			generic_alloc(mem);
			generic_copy_to(mem);

			CUdeviceptr cumem;
			size_t cubytes;

			cuda_assert(cuModuleGetGlobal(&cumem, &cubytes, cuModule, bind_name.c_str()));

			if(cubytes == 8) {
				/* 64 bit device pointer */
				uint64_t ptr = mem.device_pointer;
				cuda_assert(cuMemcpyHtoD(cumem, (void*)&ptr, cubytes));
			}
			else {
				/* 32 bit device pointer */
				uint32_t ptr = (uint32_t)mem.device_pointer;
				cuda_assert(cuMemcpyHtoD(cumem, (void*)&ptr, cubytes));
			}
			return;
		}

		/* Image Texture Storage */
		CUarray_format_enum format;
		switch(mem.data_type) {
			case TYPE_UCHAR: format = CU_AD_FORMAT_UNSIGNED_INT8; break;
			case TYPE_UINT16: format = CU_AD_FORMAT_UNSIGNED_INT16; break;
			case TYPE_UINT: format = CU_AD_FORMAT_UNSIGNED_INT32; break;
			case TYPE_INT: format = CU_AD_FORMAT_SIGNED_INT32; break;
			case TYPE_FLOAT: format = CU_AD_FORMAT_FLOAT; break;
			case TYPE_HALF: format = CU_AD_FORMAT_HALF; break;
			default: assert(0); return;
		}

		CUDAMem *cmem = NULL;
		CUarray array_3d = NULL;
		size_t src_pitch = mem.data_width * dsize * mem.data_elements;
		size_t dst_pitch = src_pitch;

		if(mem.data_depth > 1) {
			/* 3D texture using array, there is no API for linear memory. */
			CUDA_ARRAY3D_DESCRIPTOR desc;

			desc.Width = mem.data_width;
			desc.Height = mem.data_height;
			desc.Depth = mem.data_depth;
			desc.Format = format;
			desc.NumChannels = mem.data_elements;
			desc.Flags = 0;

			VLOG(1) << "Array 3D allocate: " << mem.name << ", "
			        << string_human_readable_number(mem.memory_size()) << " bytes. ("
			        << string_human_readable_size(mem.memory_size()) << ")";

			cuda_assert(cuArray3DCreate(&array_3d, &desc));

			if(!array_3d) {
				return;
			}

			CUDA_MEMCPY3D param;
			memset(&param, 0, sizeof(param));
			param.dstMemoryType = CU_MEMORYTYPE_ARRAY;
			param.dstArray = array_3d;
			param.srcMemoryType = CU_MEMORYTYPE_HOST;
			param.srcHost = mem.host_pointer;
			param.srcPitch = src_pitch;
			param.WidthInBytes = param.srcPitch;
			param.Height = mem.data_height;
			param.Depth = mem.data_depth;

			cuda_assert(cuMemcpy3D(&param));

			mem.device_pointer = (device_ptr)array_3d;
			mem.device_size = size;
			stats.mem_alloc(size);

			cmem = &cuda_mem_map[&mem];
			cmem->texobject = 0;
			cmem->array = array_3d;
		}
		else if(mem.data_height > 0) {
			/* 2D texture, using pitch aligned linear memory. */
			int alignment = 0;
			cuda_assert(cuDeviceGetAttribute(&alignment, CU_DEVICE_ATTRIBUTE_TEXTURE_PITCH_ALIGNMENT, cuDevice));
			dst_pitch = align_up(src_pitch, alignment);
			size_t dst_size = dst_pitch * mem.data_height;

			cmem = generic_alloc(mem, dst_size - mem.memory_size());
			if(!cmem) {
				return;
			}

			CUDA_MEMCPY2D param;
			memset(&param, 0, sizeof(param));
			param.dstMemoryType = CU_MEMORYTYPE_DEVICE;
			param.dstDevice = mem.device_pointer;
			param.dstPitch = dst_pitch;
			param.srcMemoryType = CU_MEMORYTYPE_HOST;
			param.srcHost = mem.host_pointer;
			param.srcPitch = src_pitch;
			param.WidthInBytes = param.srcPitch;
			param.Height = mem.data_height;

			cuda_assert(cuMemcpy2DUnaligned(&param));
		}
		else {
			/* 1D texture, using linear memory. */
			cmem = generic_alloc(mem);
			if(!cmem) {
				return;
			}

			cuda_assert(cuMemcpyHtoD(mem.device_pointer, mem.host_pointer, size));
		}

		/* Kepler+, bindless textures. */
		int flat_slot = 0;
		if(string_startswith(mem.name, "__tex_image")) {
			int pos =  string(mem.name).rfind("_");
			flat_slot = atoi(mem.name + pos + 1);
		}
		else {
			assert(0);
		}

		CUDA_RESOURCE_DESC resDesc;
		memset(&resDesc, 0, sizeof(resDesc));

		if(array_3d) {
			resDesc.resType = CU_RESOURCE_TYPE_ARRAY;
			resDesc.res.array.hArray = array_3d;
			resDesc.flags = 0;
		}
		else if(mem.data_height > 0) {
			resDesc.resType = CU_RESOURCE_TYPE_PITCH2D;
			resDesc.res.pitch2D.devPtr = mem.device_pointer;
			resDesc.res.pitch2D.format = format;
			resDesc.res.pitch2D.numChannels = mem.data_elements;
			resDesc.res.pitch2D.height = mem.data_height;
			resDesc.res.pitch2D.width = mem.data_width;
			resDesc.res.pitch2D.pitchInBytes = dst_pitch;
		}
		else {
			resDesc.resType = CU_RESOURCE_TYPE_LINEAR;
			resDesc.res.linear.devPtr = mem.device_pointer;
			resDesc.res.linear.format = format;
			resDesc.res.linear.numChannels = mem.data_elements;
			resDesc.res.linear.sizeInBytes = mem.device_size;
		}

		CUDA_TEXTURE_DESC texDesc;
		memset(&texDesc, 0, sizeof(texDesc));
		texDesc.addressMode[0] = address_mode;
		texDesc.addressMode[1] = address_mode;
		texDesc.addressMode[2] = address_mode;
		texDesc.filterMode = filter_mode;
		texDesc.flags = CU_TRSF_NORMALIZED_COORDINATES;

		cuda_assert(cuTexObjectCreate(&cmem->texobject, &resDesc, &texDesc, NULL));

		/* Resize once */
		if(flat_slot >= texture_info.size()) {
			/* Allocate some slots in advance, to reduce amount
			 * of re-allocations. */
			texture_info.resize(flat_slot + 128);
		}

		/* Set Mapping and tag that we need to (re-)upload to device */
		TextureInfo& info = texture_info[flat_slot];
		info.data = (uint64_t)cmem->texobject;
		info.cl_buffer = 0;
		info.interpolation = mem.interpolation;
		info.extension = mem.extension;
		info.width = mem.data_width;
		info.height = mem.data_height;
		info.depth = mem.data_depth;
		need_texture_info = true;
	}

	void tex_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			CUDAContextScope scope(this);
			const CUDAMem& cmem = cuda_mem_map[&mem];

			if(cmem.texobject) {
				/* Free bindless texture. */
				cuTexObjectDestroy(cmem.texobject);
			}

			if(cmem.array) {
				/* Free array. */
				cuArrayDestroy(cmem.array);
				stats.mem_free(mem.device_size);
				mem.device_pointer = 0;
				mem.device_size = 0;

				cuda_mem_map.erase(cuda_mem_map.find(&mem));
			}
			else {
				generic_free(mem);
			}
		}
	}

#define CUDA_GET_BLOCKSIZE(func, w, h)                                                                          \
			int threads_per_block;                                                                              \
			cuda_assert(cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, func)); \
			int threads = (int)sqrt((float)threads_per_block);                                                  \
			int xblocks = ((w) + threads - 1)/threads;                                                          \
			int yblocks = ((h) + threads - 1)/threads;

#define CUDA_LAUNCH_KERNEL(func, args)                      \
			cuda_assert(cuLaunchKernel(func,                \
			                           xblocks, yblocks, 1, \
			                           threads, threads, 1, \
			                           0, 0, args, 0));

/* Similar as above, but for 1-dimensional blocks. */
#define CUDA_GET_BLOCKSIZE_1D(func, w, h)                                                                       \
			int threads_per_block;                                                                              \
			cuda_assert(cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, func)); \
			int xblocks = ((w) + threads_per_block - 1)/threads_per_block;                                      \
			int yblocks = h;

#define CUDA_LAUNCH_KERNEL_1D(func, args)                       \
			cuda_assert(cuLaunchKernel(func,                    \
			                           xblocks, yblocks, 1,     \
			                           threads_per_block, 1, 1, \
			                           0, 0, args, 0));

	bool denoising_non_local_means(device_ptr image_ptr, device_ptr guide_ptr, device_ptr variance_ptr, device_ptr out_ptr,
	                               DenoisingTask *task)
	{
		if(have_error())
			return false;

		CUDAContextScope scope(this);

		int stride = task->buffer.stride;
		int w = task->buffer.width;
		int h = task->buffer.h;
		int r = task->nlm_state.r;
		int f = task->nlm_state.f;
		float a = task->nlm_state.a;
		float k_2 = task->nlm_state.k_2;

		int shift_stride = stride*h;
		int num_shifts = (2*r+1)*(2*r+1);
		int mem_size = sizeof(float)*shift_stride*num_shifts;
		int channel_offset = 0;

		device_only_memory<uchar> temporary_mem(this, "Denoising temporary_mem");
		temporary_mem.alloc_to_device(2*mem_size);

		if(have_error())
			return false;

		CUdeviceptr difference     = cuda_device_ptr(temporary_mem.device_pointer);
		CUdeviceptr blurDifference = difference + mem_size;

		CUdeviceptr weightAccum = task->nlm_state.temporary_3_ptr;
		cuda_assert(cuMemsetD8(weightAccum, 0, sizeof(float)*shift_stride));
		cuda_assert(cuMemsetD8(out_ptr, 0, sizeof(float)*shift_stride));

		{
			CUfunction cuNLMCalcDifference, cuNLMBlur, cuNLMCalcWeight, cuNLMUpdateOutput;
			cuda_assert(cuModuleGetFunction(&cuNLMCalcDifference, cuFilterModule, "kernel_cuda_filter_nlm_calc_difference"));
			cuda_assert(cuModuleGetFunction(&cuNLMBlur,           cuFilterModule, "kernel_cuda_filter_nlm_blur"));
			cuda_assert(cuModuleGetFunction(&cuNLMCalcWeight,     cuFilterModule, "kernel_cuda_filter_nlm_calc_weight"));
			cuda_assert(cuModuleGetFunction(&cuNLMUpdateOutput,   cuFilterModule, "kernel_cuda_filter_nlm_update_output"));

			cuda_assert(cuFuncSetCacheConfig(cuNLMCalcDifference, CU_FUNC_CACHE_PREFER_L1));
			cuda_assert(cuFuncSetCacheConfig(cuNLMBlur,           CU_FUNC_CACHE_PREFER_L1));
			cuda_assert(cuFuncSetCacheConfig(cuNLMCalcWeight,     CU_FUNC_CACHE_PREFER_L1));
			cuda_assert(cuFuncSetCacheConfig(cuNLMUpdateOutput,   CU_FUNC_CACHE_PREFER_L1));

			CUDA_GET_BLOCKSIZE_1D(cuNLMCalcDifference, w*h, num_shifts);

			void *calc_difference_args[] = {&guide_ptr, &variance_ptr, &difference, &w, &h, &stride, &shift_stride, &r, &channel_offset, &a, &k_2};
			void *blur_args[]            = {&difference, &blurDifference, &w, &h, &stride, &shift_stride, &r, &f};
			void *calc_weight_args[]     = {&blurDifference, &difference, &w, &h, &stride, &shift_stride, &r, &f};
			void *update_output_args[]   = {&blurDifference, &image_ptr, &out_ptr, &weightAccum, &w, &h, &stride, &shift_stride, &r, &f};

			CUDA_LAUNCH_KERNEL_1D(cuNLMCalcDifference, calc_difference_args);
			CUDA_LAUNCH_KERNEL_1D(cuNLMBlur, blur_args);
			CUDA_LAUNCH_KERNEL_1D(cuNLMCalcWeight, calc_weight_args);
			CUDA_LAUNCH_KERNEL_1D(cuNLMBlur, blur_args);
			CUDA_LAUNCH_KERNEL_1D(cuNLMUpdateOutput, update_output_args);
		}

		temporary_mem.free();

		{
			CUfunction cuNLMNormalize;
			cuda_assert(cuModuleGetFunction(&cuNLMNormalize, cuFilterModule, "kernel_cuda_filter_nlm_normalize"));
			cuda_assert(cuFuncSetCacheConfig(cuNLMNormalize, CU_FUNC_CACHE_PREFER_L1));
			void *normalize_args[] = {&out_ptr, &weightAccum, &w, &h, &stride};
			CUDA_GET_BLOCKSIZE(cuNLMNormalize, w, h);
			CUDA_LAUNCH_KERNEL(cuNLMNormalize, normalize_args);
			cuda_assert(cuCtxSynchronize());
		}

		return !have_error();
	}

	bool denoising_construct_transform(DenoisingTask *task)
	{
		if(have_error())
			return false;

		CUDAContextScope scope(this);

		CUfunction cuFilterConstructTransform;
		cuda_assert(cuModuleGetFunction(&cuFilterConstructTransform, cuFilterModule, "kernel_cuda_filter_construct_transform"));
		cuda_assert(cuFuncSetCacheConfig(cuFilterConstructTransform, CU_FUNC_CACHE_PREFER_SHARED));
		CUDA_GET_BLOCKSIZE(cuFilterConstructTransform,
		                   task->storage.w,
		                   task->storage.h);

		void *args[] = {&task->buffer.mem.device_pointer,
		                &task->storage.transform.device_pointer,
		                &task->storage.rank.device_pointer,
		                &task->filter_area,
		                &task->rect,
		                &task->radius,
		                &task->pca_threshold,
		                &task->buffer.pass_stride};
		CUDA_LAUNCH_KERNEL(cuFilterConstructTransform, args);
		cuda_assert(cuCtxSynchronize());

		return !have_error();
	}

	bool denoising_reconstruct(device_ptr color_ptr,
	                           device_ptr color_variance_ptr,
	                           device_ptr output_ptr,
	                           DenoisingTask *task)
	{
		if(have_error())
			return false;

		CUDAContextScope scope(this);

		mem_zero(task->storage.XtWX);
		mem_zero(task->storage.XtWY);

		int r = task->radius;
		int f = 4;
		float a = 1.0f;
		float k_2 = task->nlm_k_2;

		int w = task->reconstruction_state.source_w;
		int h = task->reconstruction_state.source_h;
		int stride = task->buffer.stride;

		int shift_stride = stride*h;
		int num_shifts = (2*r+1)*(2*r+1);
		int mem_size = sizeof(float)*shift_stride*num_shifts;

		device_only_memory<uchar> temporary_mem(this, "Denoising temporary_mem");
		temporary_mem.alloc_to_device(2*mem_size);

		if(have_error())
			return false;

		CUdeviceptr difference     = cuda_device_ptr(temporary_mem.device_pointer);
		CUdeviceptr blurDifference = difference + mem_size;

		{
			CUfunction cuNLMCalcDifference, cuNLMBlur, cuNLMCalcWeight, cuNLMConstructGramian;
			cuda_assert(cuModuleGetFunction(&cuNLMCalcDifference,   cuFilterModule, "kernel_cuda_filter_nlm_calc_difference"));
			cuda_assert(cuModuleGetFunction(&cuNLMBlur,             cuFilterModule, "kernel_cuda_filter_nlm_blur"));
			cuda_assert(cuModuleGetFunction(&cuNLMCalcWeight,       cuFilterModule, "kernel_cuda_filter_nlm_calc_weight"));
			cuda_assert(cuModuleGetFunction(&cuNLMConstructGramian, cuFilterModule, "kernel_cuda_filter_nlm_construct_gramian"));

			cuda_assert(cuFuncSetCacheConfig(cuNLMCalcDifference,   CU_FUNC_CACHE_PREFER_L1));
			cuda_assert(cuFuncSetCacheConfig(cuNLMBlur,             CU_FUNC_CACHE_PREFER_L1));
			cuda_assert(cuFuncSetCacheConfig(cuNLMCalcWeight,       CU_FUNC_CACHE_PREFER_L1));
			cuda_assert(cuFuncSetCacheConfig(cuNLMConstructGramian, CU_FUNC_CACHE_PREFER_SHARED));

			CUDA_GET_BLOCKSIZE_1D(cuNLMCalcDifference,
			                     task->reconstruction_state.source_w * task->reconstruction_state.source_h,
			                     num_shifts);

			void *calc_difference_args[] = {&color_ptr, &color_variance_ptr, &difference, &w, &h, &stride, &shift_stride, &r, &task->buffer.pass_stride, &a, &k_2};
			void *blur_args[]            = {&difference, &blurDifference, &w, &h, &stride, &shift_stride, &r, &f};
			void *calc_weight_args[]     = {&blurDifference, &difference, &w, &h, &stride, &shift_stride, &r, &f};
			void *construct_gramian_args[] = {&blurDifference,
			                                  &task->buffer.mem.device_pointer,
			                                  &task->storage.transform.device_pointer,
			                                  &task->storage.rank.device_pointer,
			                                  &task->storage.XtWX.device_pointer,
			                                  &task->storage.XtWY.device_pointer,
			                                  &task->reconstruction_state.filter_window,
			                                  &w, &h, &stride,
			                                  &shift_stride, &r,
			                                  &f,
		                                      &task->buffer.pass_stride};

			CUDA_LAUNCH_KERNEL_1D(cuNLMCalcDifference, calc_difference_args);
			CUDA_LAUNCH_KERNEL_1D(cuNLMBlur, blur_args);
			CUDA_LAUNCH_KERNEL_1D(cuNLMCalcWeight, calc_weight_args);
			CUDA_LAUNCH_KERNEL_1D(cuNLMBlur, blur_args);
			CUDA_LAUNCH_KERNEL_1D(cuNLMConstructGramian, construct_gramian_args);
		}

		temporary_mem.free();

		{
			CUfunction cuFinalize;
			cuda_assert(cuModuleGetFunction(&cuFinalize, cuFilterModule, "kernel_cuda_filter_finalize"));
			cuda_assert(cuFuncSetCacheConfig(cuFinalize, CU_FUNC_CACHE_PREFER_L1));
			void *finalize_args[] = {&output_ptr,
					                 &task->storage.rank.device_pointer,
					                 &task->storage.XtWX.device_pointer,
					                 &task->storage.XtWY.device_pointer,
					                 &task->filter_area,
					                 &task->reconstruction_state.buffer_params.x,
					                 &task->render_buffer.samples};
			CUDA_GET_BLOCKSIZE(cuFinalize,
			                   task->reconstruction_state.source_w,
			                   task->reconstruction_state.source_h);
			CUDA_LAUNCH_KERNEL(cuFinalize, finalize_args);
		}

		cuda_assert(cuCtxSynchronize());

		return !have_error();
	}

	bool denoising_combine_halves(device_ptr a_ptr, device_ptr b_ptr,
	                              device_ptr mean_ptr, device_ptr variance_ptr,
	                              int r, int4 rect, DenoisingTask *task)
	{
		if(have_error())
			return false;

		CUDAContextScope scope(this);

		CUfunction cuFilterCombineHalves;
		cuda_assert(cuModuleGetFunction(&cuFilterCombineHalves, cuFilterModule, "kernel_cuda_filter_combine_halves"));
		cuda_assert(cuFuncSetCacheConfig(cuFilterCombineHalves, CU_FUNC_CACHE_PREFER_L1));
		CUDA_GET_BLOCKSIZE(cuFilterCombineHalves,
		                   task->rect.z-task->rect.x,
		                   task->rect.w-task->rect.y);

		void *args[] = {&mean_ptr,
		                &variance_ptr,
		                &a_ptr,
		                &b_ptr,
		                &rect,
		                &r};
		CUDA_LAUNCH_KERNEL(cuFilterCombineHalves, args);
		cuda_assert(cuCtxSynchronize());

		return !have_error();
	}

	bool denoising_divide_shadow(device_ptr a_ptr, device_ptr b_ptr,
	                             device_ptr sample_variance_ptr, device_ptr sv_variance_ptr,
	                             device_ptr buffer_variance_ptr, DenoisingTask *task)
	{
		if(have_error())
			return false;

		CUDAContextScope scope(this);

		CUfunction cuFilterDivideShadow;
		cuda_assert(cuModuleGetFunction(&cuFilterDivideShadow, cuFilterModule, "kernel_cuda_filter_divide_shadow"));
		cuda_assert(cuFuncSetCacheConfig(cuFilterDivideShadow, CU_FUNC_CACHE_PREFER_L1));
		CUDA_GET_BLOCKSIZE(cuFilterDivideShadow,
		                   task->rect.z-task->rect.x,
		                   task->rect.w-task->rect.y);

		void *args[] = {&task->render_buffer.samples,
		                &task->tile_info_mem.device_pointer,
		                &a_ptr,
		                &b_ptr,
		                &sample_variance_ptr,
		                &sv_variance_ptr,
		                &buffer_variance_ptr,
		                &task->rect,
		                &task->render_buffer.pass_stride,
		                &task->render_buffer.offset};
		CUDA_LAUNCH_KERNEL(cuFilterDivideShadow, args);
		cuda_assert(cuCtxSynchronize());

		return !have_error();
	}

	bool denoising_get_feature(int mean_offset,
	                           int variance_offset,
	                           device_ptr mean_ptr,
	                           device_ptr variance_ptr,
	                           DenoisingTask *task)
	{
		if(have_error())
			return false;

		CUDAContextScope scope(this);

		CUfunction cuFilterGetFeature;
		cuda_assert(cuModuleGetFunction(&cuFilterGetFeature, cuFilterModule, "kernel_cuda_filter_get_feature"));
		cuda_assert(cuFuncSetCacheConfig(cuFilterGetFeature, CU_FUNC_CACHE_PREFER_L1));
		CUDA_GET_BLOCKSIZE(cuFilterGetFeature,
		                   task->rect.z-task->rect.x,
		                   task->rect.w-task->rect.y);

		void *args[] = {&task->render_buffer.samples,
		                &task->tile_info_mem.device_pointer,
		                &mean_offset,
		                &variance_offset,
		                &mean_ptr,
		                &variance_ptr,
		                &task->rect,
		                &task->render_buffer.pass_stride,
		                &task->render_buffer.offset};
		CUDA_LAUNCH_KERNEL(cuFilterGetFeature, args);
		cuda_assert(cuCtxSynchronize());

		return !have_error();
	}

	bool denoising_detect_outliers(device_ptr image_ptr,
	                               device_ptr variance_ptr,
	                               device_ptr depth_ptr,
	                               device_ptr output_ptr,
	                               DenoisingTask *task)
	{
		if(have_error())
			return false;

		CUDAContextScope scope(this);

		CUfunction cuFilterDetectOutliers;
		cuda_assert(cuModuleGetFunction(&cuFilterDetectOutliers, cuFilterModule, "kernel_cuda_filter_detect_outliers"));
		cuda_assert(cuFuncSetCacheConfig(cuFilterDetectOutliers, CU_FUNC_CACHE_PREFER_L1));
		CUDA_GET_BLOCKSIZE(cuFilterDetectOutliers,
		                   task->rect.z-task->rect.x,
		                   task->rect.w-task->rect.y);

		void *args[] = {&image_ptr,
		                &variance_ptr,
		                &depth_ptr,
		                &output_ptr,
		                &task->rect,
		                &task->buffer.pass_stride};

		CUDA_LAUNCH_KERNEL(cuFilterDetectOutliers, args);
		cuda_assert(cuCtxSynchronize());

		return !have_error();
	}

	void denoise(RenderTile &rtile, DenoisingTask& denoising)
	{
		denoising.functions.construct_transform = function_bind(&CUDADevice::denoising_construct_transform, this, &denoising);
		denoising.functions.reconstruct = function_bind(&CUDADevice::denoising_reconstruct, this, _1, _2, _3, &denoising);
		denoising.functions.divide_shadow = function_bind(&CUDADevice::denoising_divide_shadow, this, _1, _2, _3, _4, _5, &denoising);
		denoising.functions.non_local_means = function_bind(&CUDADevice::denoising_non_local_means, this, _1, _2, _3, _4, &denoising);
		denoising.functions.combine_halves = function_bind(&CUDADevice::denoising_combine_halves, this, _1, _2, _3, _4, _5, _6, &denoising);
		denoising.functions.get_feature = function_bind(&CUDADevice::denoising_get_feature, this, _1, _2, _3, _4, &denoising);
		denoising.functions.detect_outliers = function_bind(&CUDADevice::denoising_detect_outliers, this, _1, _2, _3, _4, &denoising);

		denoising.filter_area = make_int4(rtile.x, rtile.y, rtile.w, rtile.h);
		denoising.render_buffer.samples = rtile.sample;

		denoising.run_denoising(&rtile);
	}

	void path_trace(DeviceTask& task, RenderTile& rtile, device_vector<WorkTile>& work_tiles)
	{
		scoped_timer timer(&rtile.buffers->render_time);

		if(have_error())
			return;

		CUDAContextScope scope(this);
		CUfunction cuPathTrace;

		/* Get kernel function. */
		if(task.integrator_branched) {
			cuda_assert(cuModuleGetFunction(&cuPathTrace, cuModule, "kernel_cuda_branched_path_trace"));
		}
		else {
			cuda_assert(cuModuleGetFunction(&cuPathTrace, cuModule, "kernel_cuda_path_trace"));
		}

		if(have_error()) {
			return;
		}

		cuda_assert(cuFuncSetCacheConfig(cuPathTrace, CU_FUNC_CACHE_PREFER_L1));

		/* Allocate work tile. */
		work_tiles.alloc(1);

		WorkTile *wtile = work_tiles.data();
		wtile->x = rtile.x;
		wtile->y = rtile.y;
		wtile->w = rtile.w;
		wtile->h = rtile.h;
		wtile->offset = rtile.offset;
		wtile->stride = rtile.stride;
		wtile->buffer = (float*)cuda_device_ptr(rtile.buffer);

		/* Prepare work size. More step samples render faster, but for now we
		 * remain conservative for GPUs connected to a display to avoid driver
		 * timeouts and display freezing. */
		int min_blocks, num_threads_per_block;
		cuda_assert(cuOccupancyMaxPotentialBlockSize(&min_blocks, &num_threads_per_block, cuPathTrace, NULL, 0, 0));
		if(!info.display_device) {
			min_blocks *= 8;
		}

		uint step_samples = divide_up(min_blocks * num_threads_per_block, wtile->w * wtile->h);

		/* Render all samples. */
		int start_sample = rtile.start_sample;
		int end_sample = rtile.start_sample + rtile.num_samples;

		for(int sample = start_sample; sample < end_sample; sample += step_samples) {
			/* Setup and copy work tile to device. */
			wtile->start_sample = sample;
			wtile->num_samples = min(step_samples, end_sample - sample);;
			work_tiles.copy_to_device();

			CUdeviceptr d_work_tiles = cuda_device_ptr(work_tiles.device_pointer);
			uint total_work_size = wtile->w * wtile->h * wtile->num_samples;
			uint num_blocks = divide_up(total_work_size, num_threads_per_block);

			/* Launch kernel. */
			void *args[] = {&d_work_tiles,
			                &total_work_size};

			cuda_assert(cuLaunchKernel(cuPathTrace,
			                           num_blocks, 1, 1,
			                           num_threads_per_block, 1, 1,
			                           0, 0, args, 0));

			cuda_assert(cuCtxSynchronize());

			/* Update progress. */
			rtile.sample = sample + wtile->num_samples;
			task.update_progress(&rtile, rtile.w*rtile.h*wtile->num_samples);

			if(task.get_cancel()) {
				if(task.need_finish_queue == false)
					break;
			}
		}
	}

	void film_convert(DeviceTask& task, device_ptr buffer, device_ptr rgba_byte, device_ptr rgba_half)
	{
		if(have_error())
			return;

		CUDAContextScope scope(this);

		CUfunction cuFilmConvert;
		CUdeviceptr d_rgba = map_pixels((rgba_byte)? rgba_byte: rgba_half);
		CUdeviceptr d_buffer = cuda_device_ptr(buffer);

		/* get kernel function */
		if(rgba_half) {
			cuda_assert(cuModuleGetFunction(&cuFilmConvert, cuModule, "kernel_cuda_convert_to_half_float"));
		}
		else {
			cuda_assert(cuModuleGetFunction(&cuFilmConvert, cuModule, "kernel_cuda_convert_to_byte"));
		}


		float sample_scale = 1.0f/(task.sample + 1);

		/* pass in parameters */
		void *args[] = {&d_rgba,
		                &d_buffer,
		                &sample_scale,
		                &task.x,
		                &task.y,
		                &task.w,
		                &task.h,
		                &task.offset,
		                &task.stride};

		/* launch kernel */
		int threads_per_block;
		cuda_assert(cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuFilmConvert));

		int xthreads = (int)sqrt(threads_per_block);
		int ythreads = (int)sqrt(threads_per_block);
		int xblocks = (task.w + xthreads - 1)/xthreads;
		int yblocks = (task.h + ythreads - 1)/ythreads;

		cuda_assert(cuFuncSetCacheConfig(cuFilmConvert, CU_FUNC_CACHE_PREFER_L1));

		cuda_assert(cuLaunchKernel(cuFilmConvert,
		                           xblocks , yblocks, 1, /* blocks */
		                           xthreads, ythreads, 1, /* threads */
		                           0, 0, args, 0));

		unmap_pixels((rgba_byte)? rgba_byte: rgba_half);

		cuda_assert(cuCtxSynchronize());
	}

	void shader(DeviceTask& task)
	{
		if(have_error())
			return;

		CUDAContextScope scope(this);

		CUfunction cuShader;
		CUdeviceptr d_input = cuda_device_ptr(task.shader_input);
		CUdeviceptr d_output = cuda_device_ptr(task.shader_output);

		/* get kernel function */
		if(task.shader_eval_type >= SHADER_EVAL_BAKE) {
			cuda_assert(cuModuleGetFunction(&cuShader, cuModule, "kernel_cuda_bake"));
		}
		else if(task.shader_eval_type == SHADER_EVAL_DISPLACE) {
			cuda_assert(cuModuleGetFunction(&cuShader, cuModule, "kernel_cuda_displace"));
		}
		else {
			cuda_assert(cuModuleGetFunction(&cuShader, cuModule, "kernel_cuda_background"));
		}

		/* do tasks in smaller chunks, so we can cancel it */
		const int shader_chunk_size = 65536;
		const int start = task.shader_x;
		const int end = task.shader_x + task.shader_w;
		int offset = task.offset;

		bool canceled = false;
		for(int sample = 0; sample < task.num_samples && !canceled; sample++) {
			for(int shader_x = start; shader_x < end; shader_x += shader_chunk_size) {
				int shader_w = min(shader_chunk_size, end - shader_x);

				/* pass in parameters */
				void *args[8];
				int arg = 0;
				args[arg++] = &d_input;
				args[arg++] = &d_output;
				args[arg++] = &task.shader_eval_type;
				if(task.shader_eval_type >= SHADER_EVAL_BAKE) {
					args[arg++] = &task.shader_filter;
				}
				args[arg++] = &shader_x;
				args[arg++] = &shader_w;
				args[arg++] = &offset;
				args[arg++] = &sample;

				/* launch kernel */
				int threads_per_block;
				cuda_assert(cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuShader));

				int xblocks = (shader_w + threads_per_block - 1)/threads_per_block;

				cuda_assert(cuFuncSetCacheConfig(cuShader, CU_FUNC_CACHE_PREFER_L1));
				cuda_assert(cuLaunchKernel(cuShader,
				                           xblocks , 1, 1, /* blocks */
				                           threads_per_block, 1, 1, /* threads */
				                           0, 0, args, 0));

				cuda_assert(cuCtxSynchronize());

				if(task.get_cancel()) {
					canceled = true;
					break;
				}
			}

			task.update_progress(NULL);
		}
	}

	CUdeviceptr map_pixels(device_ptr mem)
	{
		if(!background) {
			PixelMem pmem = pixel_mem_map[mem];
			CUdeviceptr buffer;

			size_t bytes;
			cuda_assert(cuGraphicsMapResources(1, &pmem.cuPBOresource, 0));
			cuda_assert(cuGraphicsResourceGetMappedPointer(&buffer, &bytes, pmem.cuPBOresource));

			return buffer;
		}

		return cuda_device_ptr(mem);
	}

	void unmap_pixels(device_ptr mem)
	{
		if(!background) {
			PixelMem pmem = pixel_mem_map[mem];

			cuda_assert(cuGraphicsUnmapResources(1, &pmem.cuPBOresource, 0));
		}
	}

	void pixels_alloc(device_memory& mem)
	{
		PixelMem pmem;

		pmem.w = mem.data_width;
		pmem.h = mem.data_height;

		CUDAContextScope scope(this);

		glGenBuffers(1, &pmem.cuPBO);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pmem.cuPBO);
		if(mem.data_type == TYPE_HALF)
			glBufferData(GL_PIXEL_UNPACK_BUFFER, pmem.w*pmem.h*sizeof(GLhalf)*4, NULL, GL_DYNAMIC_DRAW);
		else
			glBufferData(GL_PIXEL_UNPACK_BUFFER, pmem.w*pmem.h*sizeof(uint8_t)*4, NULL, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

		glGenTextures(1, &pmem.cuTexId);
		glBindTexture(GL_TEXTURE_2D, pmem.cuTexId);
		if(mem.data_type == TYPE_HALF)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, pmem.w, pmem.h, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pmem.w, pmem.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);

		CUresult result = cuGraphicsGLRegisterBuffer(&pmem.cuPBOresource, pmem.cuPBO, CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);

		if(result == CUDA_SUCCESS) {
			mem.device_pointer = pmem.cuTexId;
			pixel_mem_map[mem.device_pointer] = pmem;

			mem.device_size = mem.memory_size();
			stats.mem_alloc(mem.device_size);

			return;
		}
		else {
			/* failed to register buffer, fallback to no interop */
			glDeleteBuffers(1, &pmem.cuPBO);
			glDeleteTextures(1, &pmem.cuTexId);

			background = true;
		}
	}

	void pixels_copy_from(device_memory& mem, int y, int w, int h)
	{
		PixelMem pmem = pixel_mem_map[mem.device_pointer];

		CUDAContextScope scope(this);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pmem.cuPBO);
		uchar *pixels = (uchar*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_ONLY);
		size_t offset = sizeof(uchar)*4*y*w;
		memcpy((uchar*)mem.host_pointer + offset, pixels + offset, sizeof(uchar)*4*w*h);
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}

	void pixels_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			PixelMem pmem = pixel_mem_map[mem.device_pointer];

			CUDAContextScope scope(this);

			cuda_assert(cuGraphicsUnregisterResource(pmem.cuPBOresource));
			glDeleteBuffers(1, &pmem.cuPBO);
			glDeleteTextures(1, &pmem.cuTexId);

			pixel_mem_map.erase(pixel_mem_map.find(mem.device_pointer));
			mem.device_pointer = 0;

			stats.mem_free(mem.device_size);
			mem.device_size = 0;
		}
	}

	void draw_pixels(
	    device_memory& mem, int y,
	    int w, int h, int width, int height,
	    int dx, int dy, int dw, int dh, bool transparent,
		const DeviceDrawParams &draw_params)
	{
		assert(mem.type == MEM_PIXELS);

		if(!background) {
			const bool use_fallback_shader = (draw_params.bind_display_space_shader_cb == NULL);
			PixelMem pmem = pixel_mem_map[mem.device_pointer];
			float *vpointer;

			CUDAContextScope scope(this);

			/* for multi devices, this assumes the inefficient method that we allocate
			 * all pixels on the device even though we only render to a subset */
			size_t offset = 4*y*w;

			if(mem.data_type == TYPE_HALF)
				offset *= sizeof(GLhalf);
			else
				offset *= sizeof(uint8_t);

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pmem.cuPBO);
			glBindTexture(GL_TEXTURE_2D, pmem.cuTexId);
			if(mem.data_type == TYPE_HALF) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_HALF_FLOAT, (void*)offset);
			}
			else {
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (void*)offset);
			}
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

			if(transparent) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}

			GLint shader_program;
			if(use_fallback_shader) {
				if(!bind_fallback_display_space_shader(dw, dh)) {
					return;
				}
				shader_program = fallback_shader_program;
			}
			else {
				draw_params.bind_display_space_shader_cb();
				glGetIntegerv(GL_CURRENT_PROGRAM, &shader_program);
			}

			if(!vertex_buffer) {
				glGenBuffers(1, &vertex_buffer);
			}

			glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
			/* invalidate old contents - avoids stalling if buffer is still waiting in queue to be rendered */
			glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STREAM_DRAW);

			vpointer = (float *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

			if(vpointer) {
				/* texture coordinate - vertex pair */
				vpointer[0] = 0.0f;
				vpointer[1] = 0.0f;
				vpointer[2] = dx;
				vpointer[3] = dy;

				vpointer[4] = (float)w/(float)pmem.w;
				vpointer[5] = 0.0f;
				vpointer[6] = (float)width + dx;
				vpointer[7] = dy;

				vpointer[8] = (float)w/(float)pmem.w;
				vpointer[9] = (float)h/(float)pmem.h;
				vpointer[10] = (float)width + dx;
				vpointer[11] = (float)height + dy;

				vpointer[12] = 0.0f;
				vpointer[13] = (float)h/(float)pmem.h;
				vpointer[14] = dx;
				vpointer[15] = (float)height + dy;

				glUnmapBuffer(GL_ARRAY_BUFFER);
			}

			GLuint vertex_array_object;
			GLuint position_attribute, texcoord_attribute;

			glGenVertexArrays(1, &vertex_array_object);
			glBindVertexArray(vertex_array_object);

			texcoord_attribute = glGetAttribLocation(shader_program, "texCoord");
			position_attribute = glGetAttribLocation(shader_program, "pos");

			glEnableVertexAttribArray(texcoord_attribute);
			glEnableVertexAttribArray(position_attribute);

			glVertexAttribPointer(texcoord_attribute, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const GLvoid *)0);
			glVertexAttribPointer(position_attribute, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const GLvoid *)(sizeof(float) * 2));

			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

			if(use_fallback_shader) {
				glUseProgram(0);
			}
			else {
				draw_params.unbind_display_space_shader_cb();
			}

			if(transparent) {
				glDisable(GL_BLEND);
			}

			glBindTexture(GL_TEXTURE_2D, 0);

			return;
		}

		Device::draw_pixels(mem, y, w, h, width, height, dx, dy, dw, dh, transparent, draw_params);
	}

	void thread_run(DeviceTask *task)
	{
		CUDAContextScope scope(this);

		if(task->type == DeviceTask::RENDER) {
			DeviceRequestedFeatures requested_features;
			if(use_split_kernel()) {
				if(split_kernel == NULL) {
					split_kernel = new CUDASplitKernel(this);
					split_kernel->load_kernels(requested_features);
				}
			}

			device_vector<WorkTile> work_tiles(this, "work_tiles", MEM_READ_ONLY);

			/* keep rendering tiles until done */
			RenderTile tile;
			DenoisingTask denoising(this, *task);

			while(task->acquire_tile(this, tile)) {
				if(tile.task == RenderTile::PATH_TRACE) {
					if(use_split_kernel()) {
						device_only_memory<uchar> void_buffer(this, "void_buffer");
						split_kernel->path_trace(task, tile, void_buffer, void_buffer);
					}
					else {
						path_trace(*task, tile, work_tiles);
					}
				}
				else if(tile.task == RenderTile::DENOISE) {
					tile.sample = tile.start_sample + tile.num_samples;

					denoise(tile, denoising);

					task->update_progress(&tile, tile.w*tile.h);
				}

				task->release_tile(tile);

				if(task->get_cancel()) {
					if(task->need_finish_queue == false)
						break;
				}
			}

			work_tiles.free();
		}
		else if(task->type == DeviceTask::SHADER) {
			shader(*task);

			cuda_assert(cuCtxSynchronize());
		}
	}

	class CUDADeviceTask : public DeviceTask {
	public:
		CUDADeviceTask(CUDADevice *device, DeviceTask& task)
		: DeviceTask(task)
		{
			run = function_bind(&CUDADevice::thread_run, device, this);
		}
	};

	int get_split_task_count(DeviceTask& /*task*/)
	{
		return 1;
	}

	void task_add(DeviceTask& task)
	{
		CUDAContextScope scope(this);

		/* Load texture info. */
		load_texture_info();

		/* Synchronize all memory copies before executing task. */
		cuda_assert(cuCtxSynchronize());

		if(task.type == DeviceTask::FILM_CONVERT) {
			/* must be done in main thread due to opengl access */
			film_convert(task, task.buffer, task.rgba_byte, task.rgba_half);
		}
		else {
			task_pool.push(new CUDADeviceTask(this, task));
		}
	}

	void task_wait()
	{
		task_pool.wait();
	}

	void task_cancel()
	{
		task_pool.cancel();
	}

	friend class CUDASplitKernelFunction;
	friend class CUDASplitKernel;
	friend class CUDAContextScope;
};

/* redefine the cuda_assert macro so it can be used outside of the CUDADevice class
 * now that the definition of that class is complete
 */
#undef cuda_assert
#define cuda_assert(stmt) \
	{ \
		CUresult result = stmt; \
		\
		if(result != CUDA_SUCCESS) { \
			string message = string_printf("CUDA error: %s in %s", cuewErrorString(result), #stmt); \
			if(device->error_msg == "") \
				device->error_msg = message; \
			fprintf(stderr, "%s\n", message.c_str()); \
			/*cuda_abort();*/ \
			device->cuda_error_documentation(); \
		} \
	} (void)0


/* CUDA context scope. */

CUDAContextScope::CUDAContextScope(CUDADevice *device)
: device(device)
{
	cuda_assert(cuCtxPushCurrent(device->cuContext));
}

CUDAContextScope::~CUDAContextScope()
{
	cuda_assert(cuCtxPopCurrent(NULL));
}

/* split kernel */

class CUDASplitKernelFunction : public SplitKernelFunction{
	CUDADevice* device;
	CUfunction func;
public:
	CUDASplitKernelFunction(CUDADevice *device, CUfunction func) : device(device), func(func) {}

	/* enqueue the kernel, returns false if there is an error */
	bool enqueue(const KernelDimensions &dim, device_memory &/*kg*/, device_memory &/*data*/)
	{
		return enqueue(dim, NULL);
	}

	/* enqueue the kernel, returns false if there is an error */
	bool enqueue(const KernelDimensions &dim, void *args[])
	{
		if(device->have_error())
			return false;

		CUDAContextScope scope(device);

		/* we ignore dim.local_size for now, as this is faster */
		int threads_per_block;
		cuda_assert(cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, func));

		int xblocks = (dim.global_size[0]*dim.global_size[1] + threads_per_block - 1)/threads_per_block;

		cuda_assert(cuFuncSetCacheConfig(func, CU_FUNC_CACHE_PREFER_L1));

		cuda_assert(cuLaunchKernel(func,
		                           xblocks, 1, 1, /* blocks */
		                           threads_per_block, 1, 1, /* threads */
		                           0, 0, args, 0));

		return !device->have_error();
	}
};

CUDASplitKernel::CUDASplitKernel(CUDADevice *device) : DeviceSplitKernel(device), device(device)
{
}

uint64_t CUDASplitKernel::state_buffer_size(device_memory& /*kg*/, device_memory& /*data*/, size_t num_threads)
{
	CUDAContextScope scope(device);

	device_vector<uint64_t> size_buffer(device, "size_buffer", MEM_READ_WRITE);
	size_buffer.alloc(1);
	size_buffer.zero_to_device();

	uint threads = num_threads;
	CUdeviceptr d_size = device->cuda_device_ptr(size_buffer.device_pointer);

	struct args_t {
		uint* num_threads;
		CUdeviceptr* size;
	};

	args_t args = {
		&threads,
		&d_size
	};

	CUfunction state_buffer_size;
	cuda_assert(cuModuleGetFunction(&state_buffer_size, device->cuModule, "kernel_cuda_state_buffer_size"));

	cuda_assert(cuLaunchKernel(state_buffer_size,
	                           1, 1, 1,
	                           1, 1, 1,
	                           0, 0, (void**)&args, 0));

	size_buffer.copy_from_device(0, 1, 1);
	size_t size = size_buffer[0];
	size_buffer.free();

	return size;
}

bool CUDASplitKernel::enqueue_split_kernel_data_init(const KernelDimensions& dim,
                                    RenderTile& rtile,
                                    int num_global_elements,
                                    device_memory& /*kernel_globals*/,
                                    device_memory& /*kernel_data*/,
                                    device_memory& split_data,
                                    device_memory& ray_state,
                                    device_memory& queue_index,
                                    device_memory& use_queues_flag,
                                    device_memory& work_pool_wgs)
{
	CUDAContextScope scope(device);

	CUdeviceptr d_split_data = device->cuda_device_ptr(split_data.device_pointer);
	CUdeviceptr d_ray_state = device->cuda_device_ptr(ray_state.device_pointer);
	CUdeviceptr d_queue_index = device->cuda_device_ptr(queue_index.device_pointer);
	CUdeviceptr d_use_queues_flag = device->cuda_device_ptr(use_queues_flag.device_pointer);
	CUdeviceptr d_work_pool_wgs = device->cuda_device_ptr(work_pool_wgs.device_pointer);

	CUdeviceptr d_buffer = device->cuda_device_ptr(rtile.buffer);

	int end_sample = rtile.start_sample + rtile.num_samples;
	int queue_size = dim.global_size[0] * dim.global_size[1];

	struct args_t {
		CUdeviceptr* split_data_buffer;
		int* num_elements;
		CUdeviceptr* ray_state;
		int* start_sample;
		int* end_sample;
		int* sx;
		int* sy;
		int* sw;
		int* sh;
		int* offset;
		int* stride;
		CUdeviceptr* queue_index;
		int* queuesize;
		CUdeviceptr* use_queues_flag;
		CUdeviceptr* work_pool_wgs;
		int* num_samples;
		CUdeviceptr* buffer;
	};

	args_t args = {
		&d_split_data,
		&num_global_elements,
		&d_ray_state,
		&rtile.start_sample,
		&end_sample,
		&rtile.x,
		&rtile.y,
		&rtile.w,
		&rtile.h,
		&rtile.offset,
		&rtile.stride,
		&d_queue_index,
		&queue_size,
		&d_use_queues_flag,
		&d_work_pool_wgs,
		&rtile.num_samples,
		&d_buffer
	};

	CUfunction data_init;
	cuda_assert(cuModuleGetFunction(&data_init, device->cuModule, "kernel_cuda_path_trace_data_init"));
	if(device->have_error()) {
		return false;
	}

	CUDASplitKernelFunction(device, data_init).enqueue(dim, (void**)&args);

	return !device->have_error();
}

SplitKernelFunction* CUDASplitKernel::get_split_kernel_function(const string& kernel_name,
                                                                const DeviceRequestedFeatures&)
{
	CUDAContextScope scope(device);
	CUfunction func;

	cuda_assert(cuModuleGetFunction(&func, device->cuModule, (string("kernel_cuda_") + kernel_name).data()));
	if(device->have_error()) {
		device->cuda_error_message(string_printf("kernel \"kernel_cuda_%s\" not found in module", kernel_name.data()));
		return NULL;
	}

	return new CUDASplitKernelFunction(device, func);
}

int2 CUDASplitKernel::split_kernel_local_size()
{
	return make_int2(32, 1);
}

int2 CUDASplitKernel::split_kernel_global_size(device_memory& kg, device_memory& data, DeviceTask * /*task*/)
{
	CUDAContextScope scope(device);
	size_t free;
	size_t total;

	cuda_assert(cuMemGetInfo(&free, &total));

	VLOG(1) << "Maximum device allocation size: "
	        << string_human_readable_number(free) << " bytes. ("
	        << string_human_readable_size(free) << ").";

	size_t num_elements = max_elements_for_max_buffer_size(kg, data, free / 2);
	size_t side = round_down((int)sqrt(num_elements), 32);
	int2 global_size = make_int2(side, round_down(num_elements / side, 16));
	VLOG(1) << "Global size: " << global_size << ".";
	return global_size;
}

bool device_cuda_init(void)
{
#ifdef WITH_CUDA_DYNLOAD
	static bool initialized = false;
	static bool result = false;

	if(initialized)
		return result;

	initialized = true;
	int cuew_result = cuewInit(CUEW_INIT_CUDA);
	if(cuew_result == CUEW_SUCCESS) {
		VLOG(1) << "CUEW initialization succeeded";
		if(CUDADevice::have_precompiled_kernels()) {
			VLOG(1) << "Found precompiled kernels";
			result = true;
		}
#ifndef _WIN32
		else if(cuewCompilerPath() != NULL) {
			VLOG(1) << "Found CUDA compiler " << cuewCompilerPath();
			result = true;
		}
		else {
			VLOG(1) << "Neither precompiled kernels nor CUDA compiler was found,"
			        << " unable to use CUDA";
		}
#endif
	}
	else {
		VLOG(1) << "CUEW initialization failed: "
		        << ((cuew_result == CUEW_ERROR_ATEXIT_FAILED)
		            ? "Error setting up atexit() handler"
		            : "Error opening the library");
	}

	return result;
#else  /* WITH_CUDA_DYNLOAD */
	return true;
#endif /* WITH_CUDA_DYNLOAD */
}

Device *device_cuda_create(DeviceInfo& info, Stats &stats, bool background)
{
	return new CUDADevice(info, stats, background);
}

static CUresult device_cuda_safe_init()
{
#ifdef _WIN32
	__try {
		return cuInit(0);
	}
	__except(EXCEPTION_EXECUTE_HANDLER) {
		/* Ignore crashes inside the CUDA driver and hope we can
		 * survive even with corrupted CUDA installs. */
		fprintf(stderr, "Cycles CUDA: driver crashed, continuing without CUDA.\n");
	}

	return CUDA_ERROR_NO_DEVICE;
#else
	return cuInit(0);
#endif
}

void device_cuda_info(vector<DeviceInfo>& devices)
{
	CUresult result = device_cuda_safe_init();
	if(result != CUDA_SUCCESS) {
		if(result != CUDA_ERROR_NO_DEVICE)
			fprintf(stderr, "CUDA cuInit: %s\n", cuewErrorString(result));
		return;
	}

	int count = 0;
	result = cuDeviceGetCount(&count);
	if(result != CUDA_SUCCESS) {
		fprintf(stderr, "CUDA cuDeviceGetCount: %s\n", cuewErrorString(result));
		return;
	}

	vector<DeviceInfo> display_devices;

	for(int num = 0; num < count; num++) {
		char name[256];

		result = cuDeviceGetName(name, 256, num);
		if(result != CUDA_SUCCESS) {
			fprintf(stderr, "CUDA cuDeviceGetName: %s\n", cuewErrorString(result));
			continue;
		}

		int major;
		cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, num);
		if(major < 3) {
			VLOG(1) << "Ignoring device \"" << name
			        << "\", this graphics card is no longer supported.";
			continue;
		}

		DeviceInfo info;

		info.type = DEVICE_CUDA;
		info.description = string(name);
		info.num = num;

		info.advanced_shading = (major >= 3);
		info.has_half_images = (major >= 3);
		info.has_volume_decoupled = false;
		info.bvh_layout_mask = BVH_LAYOUT_BVH2;

		int pci_location[3] = {0, 0, 0};
		cuDeviceGetAttribute(&pci_location[0], CU_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID, num);
		cuDeviceGetAttribute(&pci_location[1], CU_DEVICE_ATTRIBUTE_PCI_BUS_ID, num);
		cuDeviceGetAttribute(&pci_location[2], CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID, num);
		info.id = string_printf("CUDA_%s_%04x:%02x:%02x",
		                        name,
		                        (unsigned int)pci_location[0],
		                        (unsigned int)pci_location[1],
		                        (unsigned int)pci_location[2]);

		/* If device has a kernel timeout and no compute preemption, we assume
		 * it is connected to a display and will freeze the display while doing
		 * computations. */
		int timeout_attr = 0, preempt_attr = 0;
		cuDeviceGetAttribute(&timeout_attr, CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT, num);
		cuDeviceGetAttribute(&preempt_attr, CU_DEVICE_ATTRIBUTE_COMPUTE_PREEMPTION_SUPPORTED, num);

		if(timeout_attr && !preempt_attr) {
			VLOG(1) << "Device is recognized as display.";
			info.description += " (Display)";
			info.display_device = true;
			display_devices.push_back(info);
		}
		else {
			devices.push_back(info);
		}
		VLOG(1) << "Added device \"" << name << "\" with id \"" << info.id << "\".";
	}

	if(!display_devices.empty())
		devices.insert(devices.end(), display_devices.begin(), display_devices.end());
}

string device_cuda_capabilities(void)
{
	CUresult result = device_cuda_safe_init();
	if(result != CUDA_SUCCESS) {
		if(result != CUDA_ERROR_NO_DEVICE) {
			return string("Error initializing CUDA: ") + cuewErrorString(result);
		}
		return "No CUDA device found\n";
	}

	int count;
	result = cuDeviceGetCount(&count);
	if(result != CUDA_SUCCESS) {
		return string("Error getting devices: ") + cuewErrorString(result);
	}

	string capabilities = "";
	for(int num = 0; num < count; num++) {
		char name[256];
		if(cuDeviceGetName(name, 256, num) != CUDA_SUCCESS) {
			continue;
		}
		capabilities += string("\t") + name + "\n";
		int value;
#define GET_ATTR(attr) \
		{ \
			if(cuDeviceGetAttribute(&value, \
			                        CU_DEVICE_ATTRIBUTE_##attr, \
			                        num) == CUDA_SUCCESS) \
			{ \
				capabilities += string_printf("\t\tCU_DEVICE_ATTRIBUTE_" #attr "\t\t\t%d\n", \
				                              value); \
			} \
		} (void)0
		/* TODO(sergey): Strip all attributes which are not useful for us
		 * or does not depend on the driver.
		 */
		GET_ATTR(MAX_THREADS_PER_BLOCK);
		GET_ATTR(MAX_BLOCK_DIM_X);
		GET_ATTR(MAX_BLOCK_DIM_Y);
		GET_ATTR(MAX_BLOCK_DIM_Z);
		GET_ATTR(MAX_GRID_DIM_X);
		GET_ATTR(MAX_GRID_DIM_Y);
		GET_ATTR(MAX_GRID_DIM_Z);
		GET_ATTR(MAX_SHARED_MEMORY_PER_BLOCK);
		GET_ATTR(SHARED_MEMORY_PER_BLOCK);
		GET_ATTR(TOTAL_CONSTANT_MEMORY);
		GET_ATTR(WARP_SIZE);
		GET_ATTR(MAX_PITCH);
		GET_ATTR(MAX_REGISTERS_PER_BLOCK);
		GET_ATTR(REGISTERS_PER_BLOCK);
		GET_ATTR(CLOCK_RATE);
		GET_ATTR(TEXTURE_ALIGNMENT);
		GET_ATTR(GPU_OVERLAP);
		GET_ATTR(MULTIPROCESSOR_COUNT);
		GET_ATTR(KERNEL_EXEC_TIMEOUT);
		GET_ATTR(INTEGRATED);
		GET_ATTR(CAN_MAP_HOST_MEMORY);
		GET_ATTR(COMPUTE_MODE);
		GET_ATTR(MAXIMUM_TEXTURE1D_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_HEIGHT);
		GET_ATTR(MAXIMUM_TEXTURE3D_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE3D_HEIGHT);
		GET_ATTR(MAXIMUM_TEXTURE3D_DEPTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_LAYERED_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_LAYERED_HEIGHT);
		GET_ATTR(MAXIMUM_TEXTURE2D_LAYERED_LAYERS);
		GET_ATTR(MAXIMUM_TEXTURE2D_ARRAY_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_ARRAY_HEIGHT);
		GET_ATTR(MAXIMUM_TEXTURE2D_ARRAY_NUMSLICES);
		GET_ATTR(SURFACE_ALIGNMENT);
		GET_ATTR(CONCURRENT_KERNELS);
		GET_ATTR(ECC_ENABLED);
		GET_ATTR(TCC_DRIVER);
		GET_ATTR(MEMORY_CLOCK_RATE);
		GET_ATTR(GLOBAL_MEMORY_BUS_WIDTH);
		GET_ATTR(L2_CACHE_SIZE);
		GET_ATTR(MAX_THREADS_PER_MULTIPROCESSOR);
		GET_ATTR(ASYNC_ENGINE_COUNT);
		GET_ATTR(UNIFIED_ADDRESSING);
		GET_ATTR(MAXIMUM_TEXTURE1D_LAYERED_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE1D_LAYERED_LAYERS);
		GET_ATTR(CAN_TEX2D_GATHER);
		GET_ATTR(MAXIMUM_TEXTURE2D_GATHER_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_GATHER_HEIGHT);
		GET_ATTR(MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE);
		GET_ATTR(MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE);
		GET_ATTR(MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE);
		GET_ATTR(TEXTURE_PITCH_ALIGNMENT);
		GET_ATTR(MAXIMUM_TEXTURECUBEMAP_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS);
		GET_ATTR(MAXIMUM_SURFACE1D_WIDTH);
		GET_ATTR(MAXIMUM_SURFACE2D_WIDTH);
		GET_ATTR(MAXIMUM_SURFACE2D_HEIGHT);
		GET_ATTR(MAXIMUM_SURFACE3D_WIDTH);
		GET_ATTR(MAXIMUM_SURFACE3D_HEIGHT);
		GET_ATTR(MAXIMUM_SURFACE3D_DEPTH);
		GET_ATTR(MAXIMUM_SURFACE1D_LAYERED_WIDTH);
		GET_ATTR(MAXIMUM_SURFACE1D_LAYERED_LAYERS);
		GET_ATTR(MAXIMUM_SURFACE2D_LAYERED_WIDTH);
		GET_ATTR(MAXIMUM_SURFACE2D_LAYERED_HEIGHT);
		GET_ATTR(MAXIMUM_SURFACE2D_LAYERED_LAYERS);
		GET_ATTR(MAXIMUM_SURFACECUBEMAP_WIDTH);
		GET_ATTR(MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH);
		GET_ATTR(MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS);
		GET_ATTR(MAXIMUM_TEXTURE1D_LINEAR_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_LINEAR_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_LINEAR_HEIGHT);
		GET_ATTR(MAXIMUM_TEXTURE2D_LINEAR_PITCH);
		GET_ATTR(MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH);
		GET_ATTR(MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT);
		GET_ATTR(COMPUTE_CAPABILITY_MAJOR);
		GET_ATTR(COMPUTE_CAPABILITY_MINOR);
		GET_ATTR(MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH);
		GET_ATTR(STREAM_PRIORITIES_SUPPORTED);
		GET_ATTR(GLOBAL_L1_CACHE_SUPPORTED);
		GET_ATTR(LOCAL_L1_CACHE_SUPPORTED);
		GET_ATTR(MAX_SHARED_MEMORY_PER_MULTIPROCESSOR);
		GET_ATTR(MAX_REGISTERS_PER_MULTIPROCESSOR);
		GET_ATTR(MANAGED_MEMORY);
		GET_ATTR(MULTI_GPU_BOARD);
		GET_ATTR(MULTI_GPU_BOARD_GROUP_ID);
#undef GET_ATTR
		capabilities += "\n";
	}

	return capabilities;
}

CCL_NAMESPACE_END

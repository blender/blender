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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device.h"
#include "device_intern.h"

#include "buffers.h"

#include "util_cuda.h"
#include "util_debug.h"
#include "util_map.h"
#include "util_opengl.h"
#include "util_path.h"
#include "util_system.h"
#include "util_types.h"
#include "util_time.h"

CCL_NAMESPACE_BEGIN

class CUDADevice : public Device
{
public:
	DedicatedTaskPool task_pool;
	CUdevice cuDevice;
	CUcontext cuContext;
	CUmodule cuModule;
	CUstream cuStream;
	CUevent tileDone;
	map<device_ptr, bool> tex_interp_map;
	int cuDevId;
	int cuDevArchitecture;
	bool first_error;
	bool use_texture_storage;
	unsigned int target_update_frequency;

	struct PixelMem {
		GLuint cuPBO;
		CUgraphicsResource cuPBOresource;
		GLuint cuTexId;
		int w, h;
	};

	map<device_ptr, PixelMem> pixel_mem_map;

	CUdeviceptr cuda_device_ptr(device_ptr mem)
	{
		return (CUdeviceptr)mem;
	}

	static const char *cuda_error_string(CUresult result)
	{
		switch(result) {
			case CUDA_SUCCESS: return "No errors";
			case CUDA_ERROR_INVALID_VALUE: return "Invalid value";
			case CUDA_ERROR_OUT_OF_MEMORY: return "Out of memory";
			case CUDA_ERROR_NOT_INITIALIZED: return "Driver not initialized";
			case CUDA_ERROR_DEINITIALIZED: return "Driver deinitialized";

			case CUDA_ERROR_NO_DEVICE: return "No CUDA-capable device available";
			case CUDA_ERROR_INVALID_DEVICE: return "Invalid device";

			case CUDA_ERROR_INVALID_IMAGE: return "Invalid kernel image";
			case CUDA_ERROR_INVALID_CONTEXT: return "Invalid context";
			case CUDA_ERROR_CONTEXT_ALREADY_CURRENT: return "Context already current";
			case CUDA_ERROR_MAP_FAILED: return "Map failed";
			case CUDA_ERROR_UNMAP_FAILED: return "Unmap failed";
			case CUDA_ERROR_ARRAY_IS_MAPPED: return "Array is mapped";
			case CUDA_ERROR_ALREADY_MAPPED: return "Already mapped";
			case CUDA_ERROR_NO_BINARY_FOR_GPU: return "No binary for GPU";
			case CUDA_ERROR_ALREADY_ACQUIRED: return "Already acquired";
			case CUDA_ERROR_NOT_MAPPED: return "Not mapped";
			case CUDA_ERROR_NOT_MAPPED_AS_ARRAY: return "Mapped resource not available for access as an array";
			case CUDA_ERROR_NOT_MAPPED_AS_POINTER: return "Mapped resource not available for access as a pointer";
			case CUDA_ERROR_ECC_UNCORRECTABLE: return "Uncorrectable ECC error detected";
			case CUDA_ERROR_UNSUPPORTED_LIMIT: return "CUlimit not supported by device";

			case CUDA_ERROR_INVALID_SOURCE: return "Invalid source";
			case CUDA_ERROR_FILE_NOT_FOUND: return "File not found";
			case CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND: return "Link to a shared object failed to resolve";
			case CUDA_ERROR_SHARED_OBJECT_INIT_FAILED: return "Shared object initialization failed";

			case CUDA_ERROR_INVALID_HANDLE: return "Invalid handle";

			case CUDA_ERROR_NOT_FOUND: return "Not found";

			case CUDA_ERROR_NOT_READY: return "CUDA not ready";

			case CUDA_ERROR_LAUNCH_FAILED: return "Launch failed";
			case CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES: return "Launch exceeded resources";
			case CUDA_ERROR_LAUNCH_TIMEOUT: return "Launch exceeded timeout";
			case CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING: return "Launch with incompatible texturing";

			case CUDA_ERROR_UNKNOWN: return "Unknown error";

			default: return "Unknown CUDA error value";
		}
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
			fprintf(stderr, "http://wiki.blender.org/index.php/Doc:2.6/Manual/Render/Cycles/GPU_Rendering\n\n");
			first_error = false;
		}
	}

#define cuda_assert(stmt) \
	{ \
		CUresult result = stmt; \
		\
		if(result != CUDA_SUCCESS) { \
			string message = string_printf("CUDA error: %s in %s", cuda_error_string(result), #stmt); \
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

		string message = string_printf("CUDA error at %s: %s", stmt.c_str(), cuda_error_string(result));
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

	void cuda_push_context()
	{
		cuda_assert(cuCtxSetCurrent(cuContext));
	}

	void cuda_pop_context()
	{
		cuda_assert(cuCtxSetCurrent(NULL));
	}

	CUDADevice(DeviceInfo& info, Stats &stats, bool background_)
	: Device(info, stats, background_)
	{
		first_error = true;
		background = background_;
		use_texture_storage = true;
		/* we try an update / sync every 1000 ms */
		target_update_frequency = 1000;

		cuDevId = info.num;
		cuDevice = 0;
		cuContext = 0;

		/* intialize */
		if(cuda_error(cuInit(0)))
			return;

		/* setup device and context */
		if(cuda_error(cuDeviceGet(&cuDevice, cuDevId)))
			return;

		CUresult result;

		if(background) {
			result = cuCtxCreate(&cuContext, 0, cuDevice);
		}
		else {
			result = cuGLCtxCreate(&cuContext, 0, cuDevice);

			if(result != CUDA_SUCCESS) {
				result = cuCtxCreate(&cuContext, 0, cuDevice);
				background = true;
			}
		}

		if(cuda_error_(result, "cuCtxCreate"))
			return;

		cuda_assert(cuStreamCreate(&cuStream, 0));
		cuda_assert(cuEventCreate(&tileDone, 0x1));

		int major, minor;
		cuDeviceComputeCapability(&major, &minor, cuDevId);
		cuDevArchitecture = major*100 + minor*10;

		/* In order to use full 6GB of memory on Titan cards, use arrays instead
		 * of textures. On earlier cards this seems slower, but on Titan it is
		 * actually slightly faster in tests. */
		use_texture_storage = (cuDevArchitecture < 300);

		cuda_pop_context();
	}

	~CUDADevice()
	{
		task_pool.stop();

		cuda_assert(cuEventDestroy(tileDone));
		cuda_assert(cuStreamDestroy(cuStream));
		cuda_assert(cuCtxDestroy(cuContext));
	}

	bool support_device(bool experimental, bool branched)
	{
		int major, minor;
		cuDeviceComputeCapability(&major, &minor, cuDevId);
		
		/* We only support sm_20 and above */
		if(major < 2) {
			cuda_error_message(string_printf("CUDA device supported only with compute capability 2.0 or up, found %d.%d.", major, minor));
			return false;
		}
		
		return true;
	}

	string compile_kernel()
	{
		/* compute cubin name */
		int major, minor;
		cuDeviceComputeCapability(&major, &minor, cuDevId);

		/* attempt to use kernel provided with blender */
		string cubin = path_get(string_printf("lib/kernel_sm_%d%d.cubin", major, minor));
		if(path_exists(cubin))
			return cubin;

		/* not found, try to use locally compiled kernel */
		string kernel_path = path_get("kernel");
		string md5 = path_files_md5_hash(kernel_path);

		cubin = string_printf("cycles_kernel_sm%d%d_%s.cubin", major, minor, md5.c_str());
		cubin = path_user_get(path_join("cache", cubin));

		/* if exists already, use it */
		if(path_exists(cubin))
			return cubin;

#ifdef _WIN32
		if(cuHavePrecompiledKernels()) {
			if(major < 2)
				cuda_error_message(string_printf("CUDA device requires compute capability 2.0 or up, found %d.%d. Your GPU is not supported.", major, minor));
			else
				cuda_error_message(string_printf("CUDA binary kernel for this graphics card compute capability (%d.%d) not found.", major, minor));
			return "";
		}
#endif

		/* if not, find CUDA compiler */
		string nvcc = cuCompilerPath();

		if(nvcc == "") {
			cuda_error_message("CUDA nvcc compiler not found. Install CUDA toolkit in default location.");
			return "";
		}

		int cuda_version = cuCompilerVersion();

		if(cuda_version == 0) {
			cuda_error_message("CUDA nvcc compiler version could not be parsed.");
			return "";
		}
		if(cuda_version < 50) {
			printf("Unsupported CUDA version %d.%d detected, you need CUDA 5.0.\n", cuda_version/10, cuda_version%10);
			return "";
		}

		else if(cuda_version > 50)
			printf("CUDA version %d.%d detected, build may succeed but only CUDA 5.0 is officially supported.\n", cuda_version/10, cuda_version%10);

		/* compile */
		string kernel = path_join(kernel_path, "kernel.cu");
		string include = kernel_path;
		const int machine = system_cpu_bits();

		double starttime = time_dt();
		printf("Compiling CUDA kernel ...\n");

		path_create_directories(cubin);

		string command = string_printf("\"%s\" -arch=sm_%d%d -m%d --cubin \"%s\" "
			"-o \"%s\" --ptxas-options=\"-v\" -I\"%s\" -DNVCC -D__KERNEL_CUDA_VERSION__=%d",
			nvcc.c_str(), major, minor, machine, kernel.c_str(), cubin.c_str(), include.c_str(), cuda_version);

		printf("%s\n", command.c_str());

		if(system(command.c_str()) == -1) {
			cuda_error_message("Failed to execute compilation command, see console for details.");
			return "";
		}

		/* verify if compilation succeeded */
		if(!path_exists(cubin)) {
			cuda_error_message("CUDA kernel compilation failed, see console for details.");
			return "";
		}

		printf("Kernel compilation finished in %.2lfs.\n", time_dt() - starttime);

		return cubin;
	}

	bool load_kernels(bool experimental)
	{
		/* check if cuda init succeeded */
		if(cuContext == 0)
			return false;
		
		/* check if GPU is supported */
		if(!support_device(experimental, false))
			return false;

		/* get kernel */
		string cubin = compile_kernel();

		if(cubin == "")
			return false;

		/* open module */
		cuda_push_context();

		string cubin_data;
		CUresult result;

		if (path_read_text(cubin, cubin_data))
			result = cuModuleLoadData(&cuModule, cubin_data.c_str());
		else
			result = CUDA_ERROR_FILE_NOT_FOUND;

		if(cuda_error_(result, "cuModuleLoad"))
			cuda_error_message(string_printf("Failed loading CUDA kernel %s.", cubin.c_str()));

		cuda_pop_context();

		return (result == CUDA_SUCCESS);
	}

	void mem_alloc(device_memory& mem, MemoryType type)
	{
		cuda_push_context();
		CUdeviceptr device_pointer;
		size_t size = mem.memory_size();
		cuda_assert(cuMemAlloc(&device_pointer, size));
		mem.device_pointer = (device_ptr)device_pointer;
		stats.mem_alloc(size);
		cuda_pop_context();
	}

	void mem_copy_to(device_memory& mem)
	{
		cuda_push_context();
		if(mem.device_pointer)
			cuda_assert(cuMemcpyHtoD(cuda_device_ptr(mem.device_pointer), (void*)mem.data_pointer, mem.memory_size()));
		cuda_pop_context();
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		size_t offset = elem*y*w;
		size_t size = elem*w*h;

		cuda_push_context();
		if(mem.device_pointer) {
			cuda_assert(cuMemcpyDtoH((uchar*)mem.data_pointer + offset,
			                         (CUdeviceptr)((uchar*)mem.device_pointer + offset), size));
		}
		else {
			memset((char*)mem.data_pointer + offset, 0, size);
		}
		cuda_pop_context();
	}

	void mem_zero(device_memory& mem)
	{
		memset((void*)mem.data_pointer, 0, mem.memory_size());

		cuda_push_context();
		if(mem.device_pointer)
			cuda_assert(cuMemsetD8(cuda_device_ptr(mem.device_pointer), 0, mem.memory_size()));
		cuda_pop_context();
	}

	void mem_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			cuda_push_context();
			cuda_assert(cuMemFree(cuda_device_ptr(mem.device_pointer)));
			cuda_pop_context();

			mem.device_pointer = 0;

			stats.mem_free(mem.memory_size());
		}
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		CUdeviceptr mem;
		size_t bytes;

		cuda_push_context();
		cuda_assert(cuModuleGetGlobal(&mem, &bytes, cuModule, name));
		//assert(bytes == size);
		cuda_assert(cuMemcpyHtoD(mem, host, size));
		cuda_pop_context();
	}

	void tex_alloc(const char *name, device_memory& mem, InterpolationType interpolation, bool periodic)
	{
		/* todo: support 3D textures, only CPU for now */

		/* determine format */
		CUarray_format_enum format;
		size_t dsize = datatype_size(mem.data_type);
		size_t size = mem.memory_size();
		bool use_texture = (interpolation != INTERPOLATION_NONE) || use_texture_storage;

		if(use_texture) {

			switch(mem.data_type) {
				case TYPE_UCHAR: format = CU_AD_FORMAT_UNSIGNED_INT8; break;
				case TYPE_UINT: format = CU_AD_FORMAT_UNSIGNED_INT32; break;
				case TYPE_INT: format = CU_AD_FORMAT_SIGNED_INT32; break;
				case TYPE_FLOAT: format = CU_AD_FORMAT_FLOAT; break;
				default: assert(0); return;
			}

			CUtexref texref = NULL;

			cuda_push_context();
			cuda_assert(cuModuleGetTexRef(&texref, cuModule, name));

			if(!texref) {
				cuda_pop_context();
				return;
			}

			if(interpolation != INTERPOLATION_NONE) {
				CUarray handle = NULL;
				CUDA_ARRAY_DESCRIPTOR desc;

				desc.Width = mem.data_width;
				desc.Height = mem.data_height;
				desc.Format = format;
				desc.NumChannels = mem.data_elements;

				cuda_assert(cuArrayCreate(&handle, &desc));

				if(!handle) {
					cuda_pop_context();
					return;
				}

				if(mem.data_height > 1) {
					CUDA_MEMCPY2D param;
					memset(&param, 0, sizeof(param));
					param.dstMemoryType = CU_MEMORYTYPE_ARRAY;
					param.dstArray = handle;
					param.srcMemoryType = CU_MEMORYTYPE_HOST;
					param.srcHost = (void*)mem.data_pointer;
					param.srcPitch = mem.data_width*dsize*mem.data_elements;
					param.WidthInBytes = param.srcPitch;
					param.Height = mem.data_height;

					cuda_assert(cuMemcpy2D(&param));
				}
				else
					cuda_assert(cuMemcpyHtoA(handle, 0, (void*)mem.data_pointer, size));

				cuda_assert(cuTexRefSetArray(texref, handle, CU_TRSA_OVERRIDE_FORMAT));

				if(interpolation == INTERPOLATION_CLOSEST) {
					cuda_assert(cuTexRefSetFilterMode(texref, CU_TR_FILTER_MODE_POINT));
				}
				else if (interpolation == INTERPOLATION_LINEAR) {
					cuda_assert(cuTexRefSetFilterMode(texref, CU_TR_FILTER_MODE_LINEAR));
				}
				else {/* CUBIC and SMART are unsupported for CUDA */
					cuda_assert(cuTexRefSetFilterMode(texref, CU_TR_FILTER_MODE_LINEAR));
				}
				cuda_assert(cuTexRefSetFlags(texref, CU_TRSF_NORMALIZED_COORDINATES));

				mem.device_pointer = (device_ptr)handle;

				stats.mem_alloc(size);
			}
			else {
				cuda_pop_context();

				mem_alloc(mem, MEM_READ_ONLY);
				mem_copy_to(mem);

				cuda_push_context();

				cuda_assert(cuTexRefSetAddress(NULL, texref, cuda_device_ptr(mem.device_pointer), size));
				cuda_assert(cuTexRefSetFilterMode(texref, CU_TR_FILTER_MODE_POINT));
				cuda_assert(cuTexRefSetFlags(texref, CU_TRSF_READ_AS_INTEGER));
			}

			if(periodic) {
				cuda_assert(cuTexRefSetAddressMode(texref, 0, CU_TR_ADDRESS_MODE_WRAP));
				cuda_assert(cuTexRefSetAddressMode(texref, 1, CU_TR_ADDRESS_MODE_WRAP));
			}
			else {
				cuda_assert(cuTexRefSetAddressMode(texref, 0, CU_TR_ADDRESS_MODE_CLAMP));
				cuda_assert(cuTexRefSetAddressMode(texref, 1, CU_TR_ADDRESS_MODE_CLAMP));
			}
			cuda_assert(cuTexRefSetFormat(texref, format, mem.data_elements));

			cuda_pop_context();
		}
		else {
			mem_alloc(mem, MEM_READ_ONLY);
			mem_copy_to(mem);

			cuda_push_context();

			CUdeviceptr cumem;
			size_t cubytes;

			cuda_assert(cuModuleGetGlobal(&cumem, &cubytes, cuModule, name));

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

			cuda_pop_context();
		}

		tex_interp_map[mem.device_pointer] = (interpolation != INTERPOLATION_NONE);
	}

	void tex_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			if(tex_interp_map[mem.device_pointer]) {
				cuda_push_context();
				cuArrayDestroy((CUarray)mem.device_pointer);
				cuda_pop_context();

				tex_interp_map.erase(tex_interp_map.find(mem.device_pointer));
				mem.device_pointer = 0;

				stats.mem_free(mem.memory_size());
			}
			else {
				tex_interp_map.erase(tex_interp_map.find(mem.device_pointer));
				mem_free(mem);
			}
		}
	}

	void path_trace(RenderTile& rtile, int sample, bool branched)
	{
		if(have_error())
			return;

		cuda_push_context();

		CUfunction cuPathTrace;
		CUdeviceptr d_buffer = cuda_device_ptr(rtile.buffer);
		CUdeviceptr d_rng_state = cuda_device_ptr(rtile.rng_state);

		/* get kernel function */
		if(branched && support_device(true, branched)) {
			cuda_assert(cuModuleGetFunction(&cuPathTrace, cuModule, "kernel_cuda_branched_path_trace"));
		}
		else {
			cuda_assert(cuModuleGetFunction(&cuPathTrace, cuModule, "kernel_cuda_path_trace"));
		}

		if(have_error())
			return;
	
		/* pass in parameters */
		int offset = 0;
		
		cuda_assert(cuParamSetv(cuPathTrace, offset, &d_buffer, sizeof(d_buffer)));
		offset += sizeof(d_buffer);

		cuda_assert(cuParamSetv(cuPathTrace, offset, &d_rng_state, sizeof(d_rng_state)));
		offset += sizeof(d_rng_state);

		offset = align_up(offset, __alignof(sample));

		cuda_assert(cuParamSeti(cuPathTrace, offset, sample));
		offset += sizeof(sample);

		cuda_assert(cuParamSeti(cuPathTrace, offset, rtile.x));
		offset += sizeof(rtile.x);

		cuda_assert(cuParamSeti(cuPathTrace, offset, rtile.y));
		offset += sizeof(rtile.y);

		cuda_assert(cuParamSeti(cuPathTrace, offset, rtile.w));
		offset += sizeof(rtile.w);

		cuda_assert(cuParamSeti(cuPathTrace, offset, rtile.h));
		offset += sizeof(rtile.h);

		cuda_assert(cuParamSeti(cuPathTrace, offset, rtile.offset));
		offset += sizeof(rtile.offset);

		cuda_assert(cuParamSeti(cuPathTrace, offset, rtile.stride));
		offset += sizeof(rtile.stride);

		cuda_assert(cuParamSetSize(cuPathTrace, offset));

		/* launch kernel */
		int threads_per_block;
		cuda_assert(cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuPathTrace));

		/*int num_registers;
		cuda_assert(cuFuncGetAttribute(&num_registers, CU_FUNC_ATTRIBUTE_NUM_REGS, cuPathTrace));

		printf("threads_per_block %d\n", threads_per_block);
		printf("num_registers %d\n", num_registers);*/

		int xthreads = (int)sqrt((float)threads_per_block);
		int ythreads = (int)sqrt((float)threads_per_block);
		int xblocks = (rtile.w + xthreads - 1)/xthreads;
		int yblocks = (rtile.h + ythreads - 1)/ythreads;

		cuda_assert(cuFuncSetCacheConfig(cuPathTrace, CU_FUNC_CACHE_PREFER_L1));
		cuda_assert(cuFuncSetBlockShape(cuPathTrace, xthreads, ythreads, 1));

		if(info.display_device) {
			/* don't use async for device used for display, locks up UI too much */
			cuda_assert(cuLaunchGrid(cuPathTrace, xblocks, yblocks));
			cuda_assert(cuCtxSynchronize());
		}
		else {
			cuda_assert(cuLaunchGridAsync(cuPathTrace, xblocks, yblocks, cuStream));
		}

		cuda_pop_context();
	}

	void film_convert(DeviceTask& task, device_ptr buffer, device_ptr rgba_byte, device_ptr rgba_half)
	{
		if(have_error())
			return;

		cuda_push_context();

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

		/* pass in parameters */
		int offset = 0;

		cuda_assert(cuParamSetv(cuFilmConvert, offset, &d_rgba, sizeof(d_rgba)));
		offset += sizeof(d_rgba);
		
		cuda_assert(cuParamSetv(cuFilmConvert, offset, &d_buffer, sizeof(d_buffer)));
		offset += sizeof(d_buffer);

		float sample_scale = 1.0f/(task.sample + 1);
		offset = align_up(offset, __alignof(sample_scale));

		cuda_assert(cuParamSetf(cuFilmConvert, offset, sample_scale));
		offset += sizeof(sample_scale);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.x));
		offset += sizeof(task.x);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.y));
		offset += sizeof(task.y);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.w));
		offset += sizeof(task.w);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.h));
		offset += sizeof(task.h);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.offset));
		offset += sizeof(task.offset);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.stride));
		offset += sizeof(task.stride);

		cuda_assert(cuParamSetSize(cuFilmConvert, offset));

		/* launch kernel */
		int threads_per_block;
		cuda_assert(cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuFilmConvert));

		int xthreads = (int)sqrt((float)threads_per_block);
		int ythreads = (int)sqrt((float)threads_per_block);
		int xblocks = (task.w + xthreads - 1)/xthreads;
		int yblocks = (task.h + ythreads - 1)/ythreads;

		cuda_assert(cuFuncSetCacheConfig(cuFilmConvert, CU_FUNC_CACHE_PREFER_L1));
		cuda_assert(cuFuncSetBlockShape(cuFilmConvert, xthreads, ythreads, 1));
		cuda_assert(cuLaunchGrid(cuFilmConvert, xblocks, yblocks));

		unmap_pixels((rgba_byte)? rgba_byte: rgba_half);

		cuda_pop_context();
	}

	void shader(DeviceTask& task)
	{
		if(have_error())
			return;

		cuda_push_context();

		CUfunction cuShader;
		CUdeviceptr d_input = cuda_device_ptr(task.shader_input);
		CUdeviceptr d_output = cuda_device_ptr(task.shader_output);

		/* get kernel function */
		cuda_assert(cuModuleGetFunction(&cuShader, cuModule, "kernel_cuda_shader"));

		/* do tasks in smaller chunks, so we can cancel it */
		const int shader_chunk_size = 65536;
		const int start = task.shader_x;
		const int end = task.shader_x + task.shader_w;

		for(int shader_x = start; shader_x < end; shader_x += shader_chunk_size) {
			if(task.get_cancel())
				break;

			/* pass in parameters */
			int offset = 0;

			cuda_assert(cuParamSetv(cuShader, offset, &d_input, sizeof(d_input)));
			offset += sizeof(d_input);

			cuda_assert(cuParamSetv(cuShader, offset, &d_output, sizeof(d_output)));
			offset += sizeof(d_output);

			int shader_eval_type = task.shader_eval_type;
			offset = align_up(offset, __alignof(shader_eval_type));

			cuda_assert(cuParamSeti(cuShader, offset, task.shader_eval_type));
			offset += sizeof(task.shader_eval_type);

			cuda_assert(cuParamSeti(cuShader, offset, shader_x));
			offset += sizeof(shader_x);

			cuda_assert(cuParamSetSize(cuShader, offset));

			/* launch kernel */
			int threads_per_block;
			cuda_assert(cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuShader));

			int shader_w = min(shader_chunk_size, end - shader_x);
			int xblocks = (shader_w + threads_per_block - 1)/threads_per_block;

			cuda_assert(cuFuncSetCacheConfig(cuShader, CU_FUNC_CACHE_PREFER_L1));
			cuda_assert(cuFuncSetBlockShape(cuShader, threads_per_block, 1, 1));
			cuda_assert(cuLaunchGrid(cuShader, xblocks, 1));

			cuda_assert(cuCtxSynchronize());
		}

		cuda_pop_context();
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
		if(!background) {
			PixelMem pmem;

			pmem.w = mem.data_width;
			pmem.h = mem.data_height;

			cuda_push_context();

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
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, pmem.w, pmem.h, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
			else
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pmem.w, pmem.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glBindTexture(GL_TEXTURE_2D, 0);
			
			CUresult result = cuGraphicsGLRegisterBuffer(&pmem.cuPBOresource, pmem.cuPBO, CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);

			if(result == CUDA_SUCCESS) {
				cuda_pop_context();

				mem.device_pointer = pmem.cuTexId;
				pixel_mem_map[mem.device_pointer] = pmem;

				stats.mem_alloc(mem.memory_size());

				return;
			}
			else {
				/* failed to register buffer, fallback to no interop */
				glDeleteBuffers(1, &pmem.cuPBO);
				glDeleteTextures(1, &pmem.cuTexId);

				cuda_pop_context();

				background = true;
			}
		}

		Device::pixels_alloc(mem);
	}

	void pixels_copy_from(device_memory& mem, int y, int w, int h)
	{
		if(!background) {
			PixelMem pmem = pixel_mem_map[mem.device_pointer];

			cuda_push_context();

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pmem.cuPBO);
			uchar *pixels = (uchar*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_ONLY);
			size_t offset = sizeof(uchar)*4*y*w;
			memcpy((uchar*)mem.data_pointer + offset, pixels + offset, sizeof(uchar)*4*w*h);
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

			cuda_pop_context();

			return;
		}

		Device::pixels_copy_from(mem, y, w, h);
	}

	void pixels_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			if(!background) {
				PixelMem pmem = pixel_mem_map[mem.device_pointer];

				cuda_push_context();

				cuda_assert(cuGraphicsUnregisterResource(pmem.cuPBOresource));
				glDeleteBuffers(1, &pmem.cuPBO);
				glDeleteTextures(1, &pmem.cuTexId);

				cuda_pop_context();

				pixel_mem_map.erase(pixel_mem_map.find(mem.device_pointer));
				mem.device_pointer = 0;

				stats.mem_free(mem.memory_size());

				return;
			}

			Device::pixels_free(mem);
		}
	}

	void draw_pixels(device_memory& mem, int y, int w, int h, int dy, int width, int height, bool transparent,
		const DeviceDrawParams &draw_params)
	{
		if(!background) {
			PixelMem pmem = pixel_mem_map[mem.device_pointer];

			cuda_push_context();

			/* for multi devices, this assumes the ineffecient method that we allocate
			 * all pixels on the device even though we only render to a subset */
			size_t offset = 4*y*w;

			if(mem.data_type == TYPE_HALF)
				offset *= sizeof(GLhalf);
			else
				offset *= sizeof(uint8_t);

			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pmem.cuPBO);
			glBindTexture(GL_TEXTURE_2D, pmem.cuTexId);
			if(mem.data_type == TYPE_HALF)
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_HALF_FLOAT, (void*)offset);
			else
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (void*)offset);
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
			
			glEnable(GL_TEXTURE_2D);
			
			if(transparent) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}

			glColor3f(1.0f, 1.0f, 1.0f);

			if(draw_params.bind_display_space_shader_cb) {
				draw_params.bind_display_space_shader_cb();
			}

			glPushMatrix();
			glTranslatef(0.0f, (float)dy, 0.0f);
				
			glBegin(GL_QUADS);
			
			glTexCoord2f(0.0f, 0.0f);
			glVertex2f(0.0f, 0.0f);
			glTexCoord2f((float)w/(float)pmem.w, 0.0f);
			glVertex2f((float)width, 0.0f);
			glTexCoord2f((float)w/(float)pmem.w, (float)h/(float)pmem.h);
			glVertex2f((float)width, (float)height);
			glTexCoord2f(0.0f, (float)h/(float)pmem.h);
			glVertex2f(0.0f, (float)height);

			glEnd();

			glPopMatrix();

			if(draw_params.unbind_display_space_shader_cb) {
				draw_params.unbind_display_space_shader_cb();
			}

			if(transparent)
				glDisable(GL_BLEND);
			
			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_TEXTURE_2D);

			cuda_pop_context();

			return;
		}

		Device::draw_pixels(mem, y, w, h, dy, width, height, transparent, draw_params);
	}

	void thread_run(DeviceTask *task)
	{
		if(task->type == DeviceTask::PATH_TRACE) {
			RenderTile tile;
			
			bool branched = task->integrator_branched;
			
			/* keep rendering tiles until done */
			while(task->acquire_tile(this, tile)) {
				int start_sample = tile.start_sample;
				int end_sample = tile.start_sample + tile.num_samples;

				boost::posix_time::ptime start_time(boost::posix_time::microsec_clock::local_time());
				boost::posix_time::ptime last_time = start_time;
				int sync_sample = 10;

				for(int sample = start_sample; sample < end_sample; sample++) {
					if (task->get_cancel()) {
						if(task->need_finish_queue == false)
							break;
					}

					path_trace(tile, sample, branched);

					tile.sample = sample + 1;
					task->update_progress(tile);

					if(!info.display_device && sample == sync_sample) {
						cuda_push_context();
						cuda_assert(cuEventRecord(tileDone, cuStream));
						cuda_assert(cuEventSynchronize(tileDone));

						/* Do some time keeping to find out if we need to sync less */
						boost::posix_time::ptime current_time(boost::posix_time::microsec_clock::local_time());
						boost::posix_time::time_duration sample_duration = current_time - last_time;

						long msec = sample_duration.total_milliseconds();
						float scaling_factor = (float)target_update_frequency / (float)msec;

						/* sync at earliest next sample and probably later */
						sync_sample = (sample + 1) + sync_sample * (int)ceil(scaling_factor);

						sync_sample = min(end_sample - 1, sync_sample); // make sure we sync the last sample always

						last_time = current_time;
						cuda_pop_context();
					}
				}

				task->release_tile(tile);
			}
		}
		else if(task->type == DeviceTask::SHADER) {
			shader(*task);

			cuda_push_context();
			cuda_assert(cuCtxSynchronize());
			cuda_pop_context();
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

	void task_add(DeviceTask& task)
	{
		if(task.type == DeviceTask::FILM_CONVERT) {
			/* must be done in main thread due to opengl access */
			film_convert(task, task.buffer, task.rgba_byte, task.rgba_half);

			cuda_push_context();
			cuda_assert(cuCtxSynchronize());
			cuda_pop_context();
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
};

Device *device_cuda_create(DeviceInfo& info, Stats &stats, bool background)
{
	return new CUDADevice(info, stats, background);
}

void device_cuda_info(vector<DeviceInfo>& devices)
{
	CUresult result;
	int count = 0;

	result = cuInit(0);
	if(result != CUDA_SUCCESS) {
		if(result != CUDA_ERROR_NO_DEVICE)
			fprintf(stderr, "CUDA cuInit: %s\n", CUDADevice::cuda_error_string(result));
		return;
	}

	result = cuDeviceGetCount(&count);
	if(result != CUDA_SUCCESS) {
		fprintf(stderr, "CUDA cuDeviceGetCount: %s\n", CUDADevice::cuda_error_string(result));
		return;
	}
	
	vector<DeviceInfo> display_devices;
	
	for(int num = 0; num < count; num++) {
		char name[256];
		int attr;
		
		if(cuDeviceGetName(name, 256, num) != CUDA_SUCCESS)
			continue;

		DeviceInfo info;

		info.type = DEVICE_CUDA;
		info.description = string(name);
		info.id = string_printf("CUDA_%d", num);
		info.num = num;

		int major, minor;
		cuDeviceComputeCapability(&major, &minor, num);
		info.advanced_shading = (major >= 2);
		info.pack_images = false;

		/* if device has a kernel timeout, assume it is used for display */
		if(cuDeviceGetAttribute(&attr, CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT, num) == CUDA_SUCCESS && attr == 1) {
			info.display_device = true;
			display_devices.push_back(info);
		}
		else
			devices.push_back(info);
	}

	if(!display_devices.empty())
		devices.insert(devices.end(), display_devices.begin(), display_devices.end());
}

CCL_NAMESPACE_END


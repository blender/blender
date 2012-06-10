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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device.h"
#include "device_intern.h"

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
	CUdevice cuDevice;
	CUcontext cuContext;
	CUmodule cuModule;
	map<device_ptr, bool> tex_interp_map;
	int cuDevId;

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

	const char *cuda_error_string(CUresult result)
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

#ifdef NDEBUG
#define cuda_abort()
#else
#define cuda_abort() abort()
#endif

#define cuda_assert(stmt) \
	{ \
		CUresult result = stmt; \
		\
		if(result != CUDA_SUCCESS) { \
			string message = string_printf("CUDA error: %s in %s", cuda_error_string(result), #stmt); \
			if(error_msg == "") \
				error_msg = message; \
			fprintf(stderr, "%s\n", message.c_str()); \
			cuda_abort(); \
		} \
	}

	bool cuda_error(CUresult result)
	{
		if(result == CUDA_SUCCESS)
			return false;

		string message = string_printf("CUDA error: %s", cuda_error_string(result));
		if(error_msg == "")
			error_msg = message;
		fprintf(stderr, "%s\n", message.c_str());
		return true;
	}

	void cuda_error(const string& message)
	{
		if(error_msg == "")
			error_msg = message;
		fprintf(stderr, "%s\n", message.c_str());
	}

	void cuda_push_context()
	{
		cuda_assert(cuCtxSetCurrent(cuContext))
	}

	void cuda_pop_context()
	{
		cuda_assert(cuCtxSetCurrent(NULL));
	}

	CUDADevice(DeviceInfo& info, bool background_)
	{
		background = background_;

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

		if(cuda_error(result))
			return;

		cuda_pop_context();
	}

	~CUDADevice()
	{
		cuda_push_context();
		cuda_assert(cuCtxDetach(cuContext))
	}

	bool support_device(bool experimental)
	{
		if(!experimental) {
			int major, minor;
			cuDeviceComputeCapability(&major, &minor, cuDevId);

			if(major <= 1 && minor <= 2) {
				cuda_error(string_printf("CUDA device supported only with compute capability 1.3 or up, found %d.%d.", major, minor));
				return false;
			}
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

#if defined(WITH_CUDA_BINARIES) && defined(_WIN32)
		if(major <= 1 && minor <= 2)
			cuda_error(string_printf("CUDA device supported only compute capability 1.3 or up, found %d.%d.", major, minor));
		else
			cuda_error(string_printf("CUDA binary kernel for this graphics card compute capability (%d.%d) not found.", major, minor));
		return "";
#else
		/* if not, find CUDA compiler */
		string nvcc = cuCompilerPath();

		if(nvcc == "") {
			cuda_error("CUDA nvcc compiler not found. Install CUDA toolkit in default location.");
			return "";
		}

		/* compile */
		string kernel = path_join(kernel_path, "kernel.cu");
		string include = kernel_path;
		const int machine = system_cpu_bits();
		const int maxreg = 24;

		double starttime = time_dt();
		printf("Compiling CUDA kernel ...\n");

		path_create_directories(cubin);

		string command = string_printf("\"%s\" -arch=sm_%d%d -m%d --cubin \"%s\" "
			"-o \"%s\" --ptxas-options=\"-v\" --maxrregcount=%d --opencc-options -OPT:Olimit=0 -I\"%s\" -DNVCC",
			nvcc.c_str(), major, minor, machine, kernel.c_str(), cubin.c_str(), maxreg, include.c_str());

		if(system(command.c_str()) == -1) {
			cuda_error("Failed to execute compilation command, see console for details.");
			return "";
		}

		/* verify if compilation succeeded */
		if(!path_exists(cubin)) {
			cuda_error("CUDA kernel compilation failed, see console for details.");
			return "";
		}

		printf("Kernel compilation finished in %.2lfs.\n", time_dt() - starttime);

		return cubin;
#endif
	}

	bool load_kernels(bool experimental)
	{
		/* check if cuda init succeeded */
		if(cuContext == 0)
			return false;

		if(!support_device(experimental))
			return false;

		/* get kernel */
		string cubin = compile_kernel();

		if(cubin == "")
			return false;

		/* open module */
		cuda_push_context();

		CUresult result = cuModuleLoad(&cuModule, cubin.c_str());
		if(cuda_error(result))
			cuda_error(string_printf("Failed loading CUDA kernel %s.", cubin.c_str()));

		cuda_pop_context();

		return (result == CUDA_SUCCESS);
	}

	void mem_alloc(device_memory& mem, MemoryType type)
	{
		cuda_push_context();
		CUdeviceptr device_pointer;
		cuda_assert(cuMemAlloc(&device_pointer, mem.memory_size()))
		mem.device_pointer = (device_ptr)device_pointer;
		cuda_pop_context();
	}

	void mem_copy_to(device_memory& mem)
	{
		cuda_push_context();
		cuda_assert(cuMemcpyHtoD(cuda_device_ptr(mem.device_pointer), (void*)mem.data_pointer, mem.memory_size()))
		cuda_pop_context();
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		size_t offset = elem*y*w;
		size_t size = elem*w*h;

		cuda_push_context();
		cuda_assert(cuMemcpyDtoH((uchar*)mem.data_pointer + offset,
			(CUdeviceptr)((uchar*)mem.device_pointer + offset), size))
		cuda_pop_context();
	}

	void mem_zero(device_memory& mem)
	{
		memset((void*)mem.data_pointer, 0, mem.memory_size());

		cuda_push_context();
		cuda_assert(cuMemsetD8(cuda_device_ptr(mem.device_pointer), 0, mem.memory_size()))
		cuda_pop_context();
	}

	void mem_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			cuda_push_context();
			cuda_assert(cuMemFree(cuda_device_ptr(mem.device_pointer)))
			cuda_pop_context();

			mem.device_pointer = 0;
		}
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		CUdeviceptr mem;
		size_t bytes;

		cuda_push_context();
		cuda_assert(cuModuleGetGlobal(&mem, &bytes, cuModule, name))
		//assert(bytes == size);
		cuda_assert(cuMemcpyHtoD(mem, host, size))
		cuda_pop_context();
	}

	void tex_alloc(const char *name, device_memory& mem, bool interpolation, bool periodic)
	{
		/* determine format */
		CUarray_format_enum format;
		size_t dsize = datatype_size(mem.data_type);
		size_t size = mem.memory_size();

		switch(mem.data_type) {
			case TYPE_UCHAR: format = CU_AD_FORMAT_UNSIGNED_INT8; break;
			case TYPE_UINT: format = CU_AD_FORMAT_UNSIGNED_INT32; break;
			case TYPE_INT: format = CU_AD_FORMAT_SIGNED_INT32; break;
			case TYPE_FLOAT: format = CU_AD_FORMAT_FLOAT; break;
			default: assert(0); return;
		}

		CUtexref texref;

		cuda_push_context();
		cuda_assert(cuModuleGetTexRef(&texref, cuModule, name))

		if(interpolation) {
			CUarray handle;
			CUDA_ARRAY_DESCRIPTOR desc;

			desc.Width = mem.data_width;
			desc.Height = mem.data_height;
			desc.Format = format;
			desc.NumChannels = mem.data_elements;

			cuda_assert(cuArrayCreate(&handle, &desc))

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

				cuda_assert(cuMemcpy2D(&param))
			}
			else
				cuda_assert(cuMemcpyHtoA(handle, 0, (void*)mem.data_pointer, size))

			cuda_assert(cuTexRefSetArray(texref, handle, CU_TRSA_OVERRIDE_FORMAT))

			cuda_assert(cuTexRefSetFilterMode(texref, CU_TR_FILTER_MODE_LINEAR))
			cuda_assert(cuTexRefSetFlags(texref, CU_TRSF_NORMALIZED_COORDINATES))

			mem.device_pointer = (device_ptr)handle;
		}
		else {
			cuda_pop_context();

			mem_alloc(mem, MEM_READ_ONLY);
			mem_copy_to(mem);

			cuda_push_context();

			cuda_assert(cuTexRefSetAddress(NULL, texref, cuda_device_ptr(mem.device_pointer), size))
			cuda_assert(cuTexRefSetFilterMode(texref, CU_TR_FILTER_MODE_POINT))
			cuda_assert(cuTexRefSetFlags(texref, CU_TRSF_READ_AS_INTEGER))
		}

		if(periodic) {
			cuda_assert(cuTexRefSetAddressMode(texref, 0, CU_TR_ADDRESS_MODE_WRAP))
			cuda_assert(cuTexRefSetAddressMode(texref, 1, CU_TR_ADDRESS_MODE_WRAP))
		}
		else {
			cuda_assert(cuTexRefSetAddressMode(texref, 0, CU_TR_ADDRESS_MODE_CLAMP))
			cuda_assert(cuTexRefSetAddressMode(texref, 1, CU_TR_ADDRESS_MODE_CLAMP))
		}
		cuda_assert(cuTexRefSetFormat(texref, format, mem.data_elements))

		cuda_pop_context();

		tex_interp_map[mem.device_pointer] = interpolation;
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
			}
			else {
				tex_interp_map.erase(tex_interp_map.find(mem.device_pointer));
				mem_free(mem);
			}
		}
	}

	void path_trace(DeviceTask& task)
	{
		cuda_push_context();

		CUfunction cuPathTrace;
		CUdeviceptr d_buffer = cuda_device_ptr(task.buffer);
		CUdeviceptr d_rng_state = cuda_device_ptr(task.rng_state);

		/* get kernel function */
		cuda_assert(cuModuleGetFunction(&cuPathTrace, cuModule, "kernel_cuda_path_trace"))
		
		/* pass in parameters */
		int offset = 0;
		
		cuda_assert(cuParamSetv(cuPathTrace, offset, &d_buffer, sizeof(d_buffer)))
		offset += sizeof(d_buffer);

		cuda_assert(cuParamSetv(cuPathTrace, offset, &d_rng_state, sizeof(d_rng_state)))
		offset += sizeof(d_rng_state);

		int sample = task.sample;
		offset = align_up(offset, __alignof(sample));

		cuda_assert(cuParamSeti(cuPathTrace, offset, task.sample))
		offset += sizeof(task.sample);

		cuda_assert(cuParamSeti(cuPathTrace, offset, task.x))
		offset += sizeof(task.x);

		cuda_assert(cuParamSeti(cuPathTrace, offset, task.y))
		offset += sizeof(task.y);

		cuda_assert(cuParamSeti(cuPathTrace, offset, task.w))
		offset += sizeof(task.w);

		cuda_assert(cuParamSeti(cuPathTrace, offset, task.h))
		offset += sizeof(task.h);

		cuda_assert(cuParamSeti(cuPathTrace, offset, task.offset))
		offset += sizeof(task.offset);

		cuda_assert(cuParamSeti(cuPathTrace, offset, task.stride))
		offset += sizeof(task.stride);

		cuda_assert(cuParamSetSize(cuPathTrace, offset))

		/* launch kernel: todo find optimal size, cache config for fermi */
#ifndef __APPLE__
		int xthreads = 16;
		int ythreads = 16;
#else
		int xthreads = 8;
		int ythreads = 8;
#endif
		int xblocks = (task.w + xthreads - 1)/xthreads;
		int yblocks = (task.h + ythreads - 1)/ythreads;

		cuda_assert(cuFuncSetCacheConfig(cuPathTrace, CU_FUNC_CACHE_PREFER_L1))
		cuda_assert(cuFuncSetBlockShape(cuPathTrace, xthreads, ythreads, 1))
		cuda_assert(cuLaunchGrid(cuPathTrace, xblocks, yblocks))

		cuda_pop_context();
	}

	void tonemap(DeviceTask& task)
	{
		cuda_push_context();

		CUfunction cuFilmConvert;
		CUdeviceptr d_rgba = map_pixels(task.rgba);
		CUdeviceptr d_buffer = cuda_device_ptr(task.buffer);

		/* get kernel function */
		cuda_assert(cuModuleGetFunction(&cuFilmConvert, cuModule, "kernel_cuda_tonemap"))

		/* pass in parameters */
		int offset = 0;

		cuda_assert(cuParamSetv(cuFilmConvert, offset, &d_rgba, sizeof(d_rgba)))
		offset += sizeof(d_rgba);
		
		cuda_assert(cuParamSetv(cuFilmConvert, offset, &d_buffer, sizeof(d_buffer)))
		offset += sizeof(d_buffer);

		int sample = task.sample;
		offset = align_up(offset, __alignof(sample));

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.sample))
		offset += sizeof(task.sample);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.resolution))
		offset += sizeof(task.resolution);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.x))
		offset += sizeof(task.x);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.y))
		offset += sizeof(task.y);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.w))
		offset += sizeof(task.w);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.h))
		offset += sizeof(task.h);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.offset))
		offset += sizeof(task.offset);

		cuda_assert(cuParamSeti(cuFilmConvert, offset, task.stride))
		offset += sizeof(task.stride);

		cuda_assert(cuParamSetSize(cuFilmConvert, offset))

		/* launch kernel: todo find optimal size, cache config for fermi */
#ifndef __APPLE__
		int xthreads = 16;
		int ythreads = 16;
#else
		int xthreads = 8;
		int ythreads = 8;
#endif
		int xblocks = (task.w + xthreads - 1)/xthreads;
		int yblocks = (task.h + ythreads - 1)/ythreads;

		cuda_assert(cuFuncSetCacheConfig(cuFilmConvert, CU_FUNC_CACHE_PREFER_L1))
		cuda_assert(cuFuncSetBlockShape(cuFilmConvert, xthreads, ythreads, 1))
		cuda_assert(cuLaunchGrid(cuFilmConvert, xblocks, yblocks))

		unmap_pixels(task.rgba);

		cuda_pop_context();
	}

	void shader(DeviceTask& task)
	{
		cuda_push_context();

		CUfunction cuDisplace;
		CUdeviceptr d_input = cuda_device_ptr(task.shader_input);
		CUdeviceptr d_offset = cuda_device_ptr(task.shader_output);

		/* get kernel function */
		cuda_assert(cuModuleGetFunction(&cuDisplace, cuModule, "kernel_cuda_shader"))
		
		/* pass in parameters */
		int offset = 0;
		
		cuda_assert(cuParamSetv(cuDisplace, offset, &d_input, sizeof(d_input)))
		offset += sizeof(d_input);

		cuda_assert(cuParamSetv(cuDisplace, offset, &d_offset, sizeof(d_offset)))
		offset += sizeof(d_offset);

		int shader_eval_type = task.shader_eval_type;
		offset = align_up(offset, __alignof(shader_eval_type));

		cuda_assert(cuParamSeti(cuDisplace, offset, task.shader_eval_type))
		offset += sizeof(task.shader_eval_type);

		cuda_assert(cuParamSeti(cuDisplace, offset, task.shader_x))
		offset += sizeof(task.shader_x);

		cuda_assert(cuParamSetSize(cuDisplace, offset))

		/* launch kernel: todo find optimal size, cache config for fermi */
#ifndef __APPLE__
		int xthreads = 16;
#else
		int xthreads = 8;
#endif
		int xblocks = (task.shader_w + xthreads - 1)/xthreads;

		cuda_assert(cuFuncSetCacheConfig(cuDisplace, CU_FUNC_CACHE_PREFER_L1))
		cuda_assert(cuFuncSetBlockShape(cuDisplace, xthreads, 1, 1))
		cuda_assert(cuLaunchGrid(cuDisplace, xblocks, 1))

		cuda_pop_context();
	}

	CUdeviceptr map_pixels(device_ptr mem)
	{
		if(!background) {
			PixelMem pmem = pixel_mem_map[mem];
			CUdeviceptr buffer;
			
			size_t bytes;
			cuda_assert(cuGraphicsMapResources(1, &pmem.cuPBOresource, 0))
			cuda_assert(cuGraphicsResourceGetMappedPointer(&buffer, &bytes, pmem.cuPBOresource))
			
			return buffer;
		}

		return cuda_device_ptr(mem);
	}

	void unmap_pixels(device_ptr mem)
	{
		if(!background) {
			PixelMem pmem = pixel_mem_map[mem];

			cuda_assert(cuGraphicsUnmapResources(1, &pmem.cuPBOresource, 0))
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
			glBufferData(GL_PIXEL_UNPACK_BUFFER, pmem.w*pmem.h*sizeof(GLfloat)*3, NULL, GL_DYNAMIC_DRAW);
			
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			
			glGenTextures(1, &pmem.cuTexId);
			glBindTexture(GL_TEXTURE_2D, pmem.cuTexId);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pmem.w, pmem.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glBindTexture(GL_TEXTURE_2D, 0);
			
			CUresult result = cuGraphicsGLRegisterBuffer(&pmem.cuPBOresource, pmem.cuPBO, CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);

			if(!cuda_error(result)) {
				cuda_pop_context();

				mem.device_pointer = pmem.cuTexId;
				pixel_mem_map[mem.device_pointer] = pmem;

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

				cuda_assert(cuGraphicsUnregisterResource(pmem.cuPBOresource))
				glDeleteBuffers(1, &pmem.cuPBO);
				glDeleteTextures(1, &pmem.cuTexId);

				cuda_pop_context();

				pixel_mem_map.erase(pixel_mem_map.find(mem.device_pointer));
				mem.device_pointer = 0;

				return;
			}

			Device::pixels_free(mem);
		}
	}

	void draw_pixels(device_memory& mem, int y, int w, int h, int dy, int width, int height, bool transparent)
	{
		if(!background) {
			PixelMem pmem = pixel_mem_map[mem.device_pointer];

			cuda_push_context();

			/* for multi devices, this assumes the ineffecient method that we allocate
			 * all pixels on the device even though we only render to a subset */
			size_t offset = sizeof(uint8_t)*4*y*w;

			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pmem.cuPBO);
			glBindTexture(GL_TEXTURE_2D, pmem.cuTexId);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (void*)offset);
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
			
			glEnable(GL_TEXTURE_2D);
			
			if(transparent) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}

			glColor3f(1.0f, 1.0f, 1.0f);

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

			if(transparent)
				glDisable(GL_BLEND);
			
			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_TEXTURE_2D);

			cuda_pop_context();

			return;
		}

		Device::draw_pixels(mem, y, w, h, dy, width, height, transparent);
	}

	void task_add(DeviceTask& task)
	{
		if(task.type == DeviceTask::TONEMAP)
			tonemap(task);
		else if(task.type == DeviceTask::PATH_TRACE)
			path_trace(task);
		else if(task.type == DeviceTask::SHADER)
			shader(task);
	}

	void task_wait()
	{
		cuda_push_context();

		cuda_assert(cuCtxSynchronize())

		cuda_pop_context();
	}

	void task_cancel()
	{
	}
};

Device *device_cuda_create(DeviceInfo& info, bool background)
{
	return new CUDADevice(info, background);
}

void device_cuda_info(vector<DeviceInfo>& devices)
{
	int count = 0;

	if(cuInit(0) != CUDA_SUCCESS)
		return;
	if(cuDeviceGetCount(&count) != CUDA_SUCCESS)
		return;
	
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


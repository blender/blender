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

#ifdef WITH_OPENCL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device.h"
#include "device_intern.h"

#include "buffers.h"

#include "clew.h"

#include "util_foreach.h"
#include "util_map.h"
#include "util_math.h"
#include "util_md5.h"
#include "util_opengl.h"
#include "util_path.h"
#include "util_time.h"

CCL_NAMESPACE_BEGIN

#define CL_MEM_PTR(p) ((cl_mem)(uintptr_t)(p))

static cl_device_type opencl_device_type()
{
	char *device = getenv("CYCLES_OPENCL_TEST");

	if(device) {
		if(strcmp(device, "ALL") == 0)
			return CL_DEVICE_TYPE_ALL;
		else if(strcmp(device, "DEFAULT") == 0)
			return CL_DEVICE_TYPE_DEFAULT;
		else if(strcmp(device, "CPU") == 0)
			return CL_DEVICE_TYPE_CPU;
		else if(strcmp(device, "GPU") == 0)
			return CL_DEVICE_TYPE_GPU;
		else if(strcmp(device, "ACCELERATOR") == 0)
			return CL_DEVICE_TYPE_ACCELERATOR;
	}

	return CL_DEVICE_TYPE_ALL;
}

static bool opencl_kernel_use_debug()
{
	return (getenv("CYCLES_OPENCL_DEBUG") != NULL);
}

static bool opencl_kernel_use_advanced_shading(const string& platform)
{
	/* keep this in sync with kernel_types.h! */
	if(platform == "NVIDIA CUDA")
		return true;
	else if(platform == "Apple")
		return false;
	else if(platform == "AMD Accelerated Parallel Processing")
		return false;
	else if(platform == "Intel(R) OpenCL")
		return true;

	return false;
}

static string opencl_kernel_build_options(const string& platform, const string *debug_src = NULL)
{
	string build_options = " -cl-fast-relaxed-math ";

	if(platform == "NVIDIA CUDA")
		build_options += "-D__KERNEL_OPENCL_NVIDIA__ -cl-nv-maxrregcount=32 -cl-nv-verbose ";

	else if(platform == "Apple")
		build_options += "-D__KERNEL_OPENCL_APPLE__ ";

	else if(platform == "AMD Accelerated Parallel Processing")
		build_options += "-D__KERNEL_OPENCL_AMD__ ";

	else if(platform == "Intel(R) OpenCL") {
		build_options += "-D__KERNEL_OPENCL_INTEL_CPU__";

		/* options for gdb source level kernel debugging. this segfaults on linux currently */
		if(opencl_kernel_use_debug() && debug_src)
			build_options += "-g -s \"" + *debug_src + "\"";
	}

	if(opencl_kernel_use_debug())
		build_options += "-D__KERNEL_OPENCL_DEBUG__ ";

#ifdef WITH_CYCLES_DEBUG
	build_options += "-D__KERNEL_DEBUG__ ";
#endif

	return build_options;
}

/* thread safe cache for contexts and programs */
class OpenCLCache
{
	struct Slot
	{
		thread_mutex *mutex;
		cl_context context;
		cl_program program;

		Slot() : mutex(NULL), context(NULL), program(NULL) {}

		Slot(const Slot &rhs)
			: mutex(rhs.mutex)
			, context(rhs.context)
			, program(rhs.program)
		{
			/* copy can only happen in map insert, assert that */
			assert(mutex == NULL);
		}

		~Slot()
		{
			delete mutex;
			mutex = NULL;
		}
	};

	/* key is combination of platform ID and device ID */
	typedef pair<cl_platform_id, cl_device_id> PlatformDevicePair;

	/* map of Slot objects */
	typedef map<PlatformDevicePair, Slot> CacheMap;
	CacheMap cache;

	thread_mutex cache_lock;

	/* lazy instantiate */
	static OpenCLCache &global_instance()
	{
		static OpenCLCache instance;
		return instance;
	}

	OpenCLCache()
	{
	}

	~OpenCLCache()
	{
		/* Intel OpenCL bug raises SIGABRT due to pure virtual call
		 * so this is disabled. It's not necessary to free objects
		 * at process exit anyway.
		 * http://software.intel.com/en-us/forums/topic/370083#comments */

		//flush();
	}

	/* lookup something in the cache. If this returns NULL, slot_locker
	 * will be holding a lock for the cache. slot_locker should refer to a
	 * default constructed thread_scoped_lock */
	template<typename T>
	static T get_something(cl_platform_id platform, cl_device_id device,
		T Slot::*member, thread_scoped_lock &slot_locker)
	{
		assert(platform != NULL);

		OpenCLCache &self = global_instance();

		thread_scoped_lock cache_lock(self.cache_lock);

		pair<CacheMap::iterator,bool> ins = self.cache.insert(
			CacheMap::value_type(PlatformDevicePair(platform, device), Slot()));

		Slot &slot = ins.first->second;

		/* create slot lock only while holding cache lock */
		if(!slot.mutex)
			slot.mutex = new thread_mutex;

		/* need to unlock cache before locking slot, to allow store to complete */
		cache_lock.unlock();

		/* lock the slot */
		slot_locker = thread_scoped_lock(*slot.mutex);

		/* If the thing isn't cached */
		if(slot.*member == NULL) {
			/* return with the caller's lock holder holding the slot lock */
			return NULL;
		}

		/* the item was already cached, release the slot lock */
		slot_locker.unlock();

		return slot.*member;
	}

	/* store something in the cache. you MUST have tried to get the item before storing to it */
	template<typename T>
	static void store_something(cl_platform_id platform, cl_device_id device, T thing,
		T Slot::*member, thread_scoped_lock &slot_locker)
	{
		assert(platform != NULL);
		assert(device != NULL);
		assert(thing != NULL);

		OpenCLCache &self = global_instance();

		thread_scoped_lock cache_lock(self.cache_lock);
		CacheMap::iterator i = self.cache.find(PlatformDevicePair(platform, device));
		cache_lock.unlock();

		Slot &slot = i->second;

		/* sanity check */
		assert(i != self.cache.end());
		assert(slot.*member == NULL);

		slot.*member = thing;

		/* unlock the slot */
		slot_locker.unlock();
	}

public:
	/* see get_something comment */
	static cl_context get_context(cl_platform_id platform, cl_device_id device,
		thread_scoped_lock &slot_locker)
	{
		cl_context context = get_something<cl_context>(platform, device, &Slot::context, slot_locker);

		if(!context)
			return NULL;

		/* caller is going to release it when done with it, so retain it */
		cl_int ciErr = clRetainContext(context);
		assert(ciErr == CL_SUCCESS);
		(void)ciErr;

		return context;
	}

	/* see get_something comment */
	static cl_program get_program(cl_platform_id platform, cl_device_id device,
		thread_scoped_lock &slot_locker)
	{
		cl_program program = get_something<cl_program>(platform, device, &Slot::program, slot_locker);

		if(!program)
			return NULL;

		/* caller is going to release it when done with it, so retain it */
		cl_int ciErr = clRetainProgram(program);
		assert(ciErr == CL_SUCCESS);
		(void)ciErr;

		return program;
	}

	/* see store_something comment */
	static void store_context(cl_platform_id platform, cl_device_id device, cl_context context,
		thread_scoped_lock &slot_locker)
	{
		store_something<cl_context>(platform, device, context, &Slot::context, slot_locker);

		/* increment reference count in OpenCL.
		 * The caller is going to release the object when done with it. */
		cl_int ciErr = clRetainContext(context);
		assert(ciErr == CL_SUCCESS);
		(void)ciErr;
	}

	/* see store_something comment */
	static void store_program(cl_platform_id platform, cl_device_id device, cl_program program,
		thread_scoped_lock &slot_locker)
	{
		store_something<cl_program>(platform, device, program, &Slot::program, slot_locker);

		/* increment reference count in OpenCL.
		 * The caller is going to release the object when done with it. */
		cl_int ciErr = clRetainProgram(program);
		assert(ciErr == CL_SUCCESS);
		(void)ciErr;
	}

	/* discard all cached contexts and programs
	 * the parameter is a temporary workaround. See OpenCLCache::~OpenCLCache */
	static void flush()
	{
		OpenCLCache &self = global_instance();
		thread_scoped_lock cache_lock(self.cache_lock);

		foreach(CacheMap::value_type &item, self.cache) {
			if(item.second.program != NULL)
				clReleaseProgram(item.second.program);
			if(item.second.context != NULL)
				clReleaseContext(item.second.context);
		}

		self.cache.clear();
	}
};

class OpenCLDevice : public Device
{
public:
	DedicatedTaskPool task_pool;
	cl_context cxContext;
	cl_command_queue cqCommandQueue;
	cl_platform_id cpPlatform;
	cl_device_id cdDevice;
	cl_program cpProgram;
	cl_kernel ckPathTraceKernel;
	cl_kernel ckFilmConvertByteKernel;
	cl_kernel ckFilmConvertHalfFloatKernel;
	cl_kernel ckShaderKernel;
	cl_kernel ckBakeKernel;
	cl_int ciErr;

	typedef map<string, device_vector<uchar>*> ConstMemMap;
	typedef map<string, device_ptr> MemMap;

	ConstMemMap const_mem_map;
	MemMap mem_map;
	device_ptr null_mem;

	bool device_initialized;
	string platform_name;

	bool opencl_error(cl_int err)
	{
		if(err != CL_SUCCESS) {
			string message = string_printf("OpenCL error (%d): %s", err, clewErrorString(err));
			if(error_msg == "")
				error_msg = message;
			fprintf(stderr, "%s\n", message.c_str());
			return true;
		}

		return false;
	}

	void opencl_error(const string& message)
	{
		if(error_msg == "")
			error_msg = message;
		fprintf(stderr, "%s\n", message.c_str());
	}

#define opencl_assert(stmt) \
	{ \
		cl_int err = stmt; \
		\
		if(err != CL_SUCCESS) { \
			string message = string_printf("OpenCL error: %s in %s", clewErrorString(err), #stmt); \
			if(error_msg == "") \
				error_msg = message; \
			fprintf(stderr, "%s\n", message.c_str()); \
		} \
	} (void)0

	void opencl_assert_err(cl_int err, const char* where)
	{
		if(err != CL_SUCCESS) {
			string message = string_printf("OpenCL error (%d): %s in %s", err, clewErrorString(err), where);
			if(error_msg == "")
				error_msg = message;
			fprintf(stderr, "%s\n", message.c_str());
#ifndef NDEBUG
			abort();
#endif
		}
	}

	OpenCLDevice(DeviceInfo& info, Stats &stats, bool background_)
	: Device(info, stats, background_)
	{
		cpPlatform = NULL;
		cdDevice = NULL;
		cxContext = NULL;
		cqCommandQueue = NULL;
		cpProgram = NULL;
		ckPathTraceKernel = NULL;
		ckFilmConvertByteKernel = NULL;
		ckFilmConvertHalfFloatKernel = NULL;
		ckShaderKernel = NULL;
		ckBakeKernel = NULL;
		null_mem = 0;
		device_initialized = false;

		/* setup platform */
		cl_uint num_platforms;

		ciErr = clGetPlatformIDs(0, NULL, &num_platforms);
		if(opencl_error(ciErr))
			return;

		if(num_platforms == 0) {
			opencl_error("OpenCL: no platforms found.");
			return;
		}

		vector<cl_platform_id> platforms(num_platforms, NULL);

		ciErr = clGetPlatformIDs(num_platforms, &platforms[0], NULL);
		if(opencl_error(ciErr)) {
			fprintf(stderr, "clGetPlatformIDs failed \n");
			return;
		}

		int num_base = 0;
		int total_devices = 0;

		for (int platform = 0; platform < num_platforms; platform++) {
			cl_uint num_devices;

			if(opencl_error(clGetDeviceIDs(platforms[platform], opencl_device_type(), 0, NULL, &num_devices)))
				return;

			total_devices += num_devices;

			if(info.num - num_base >= num_devices) {
				/* num doesn't refer to a device in this platform */
				num_base += num_devices;
				continue;
			}

			/* device is in this platform */
			cpPlatform = platforms[platform];

			/* get devices */
			vector<cl_device_id> device_ids(num_devices, NULL);

			if(opencl_error(clGetDeviceIDs(cpPlatform, opencl_device_type(), num_devices, &device_ids[0], NULL))) {
				fprintf(stderr, "clGetDeviceIDs failed \n");
				return;
			}

			cdDevice = device_ids[info.num - num_base];

			char name[256];
			clGetPlatformInfo(cpPlatform, CL_PLATFORM_NAME, sizeof(name), &name, NULL);
			platform_name = name;

			break;
		}

		if(total_devices == 0) {
			opencl_error("OpenCL: no devices found.");
			return;
		}
		else if(!cdDevice) {
			opencl_error("OpenCL: specified device not found.");
			return;
		}

		{
			/* try to use cached context */
			thread_scoped_lock cache_locker;
			cxContext = OpenCLCache::get_context(cpPlatform, cdDevice, cache_locker);

			if(cxContext == NULL) {
				/* create context properties array to specify platform */
				const cl_context_properties context_props[] = {
					CL_CONTEXT_PLATFORM, (cl_context_properties)cpPlatform,
					0, 0
				};

				/* create context */
				cxContext = clCreateContext(context_props, 1, &cdDevice,
					context_notify_callback, cdDevice, &ciErr);

				if(opencl_error(ciErr)) {
					opencl_error("OpenCL: clCreateContext failed");
					return;
				}

				/* cache it */
				OpenCLCache::store_context(cpPlatform, cdDevice, cxContext, cache_locker);
			}
		}

		cqCommandQueue = clCreateCommandQueue(cxContext, cdDevice, 0, &ciErr);
		if(opencl_error(ciErr))
			return;

		null_mem = (device_ptr)clCreateBuffer(cxContext, CL_MEM_READ_ONLY, 1, NULL, &ciErr);
		if(opencl_error(ciErr))
			return;

		fprintf(stderr,"Device init succes\n");
		device_initialized = true;
	}

	static void CL_CALLBACK context_notify_callback(const char *err_info,
		const void *private_info, size_t cb, void *user_data)
	{
		char name[256];
		clGetDeviceInfo((cl_device_id)user_data, CL_DEVICE_NAME, sizeof(name), &name, NULL);

		fprintf(stderr, "OpenCL error (%s): %s\n", name, err_info);
	}

	bool opencl_version_check()
	{
		char version[256];

		int major, minor, req_major = 1, req_minor = 1;

		clGetPlatformInfo(cpPlatform, CL_PLATFORM_VERSION, sizeof(version), &version, NULL);

		if(sscanf(version, "OpenCL %d.%d", &major, &minor) < 2) {
			opencl_error(string_printf("OpenCL: failed to parse platform version string (%s).", version));
			return false;
		}

		if(!((major == req_major && minor >= req_minor) || (major > req_major))) {
			opencl_error(string_printf("OpenCL: platform version 1.1 or later required, found %d.%d", major, minor));
			return false;
		}

		clGetDeviceInfo(cdDevice, CL_DEVICE_OPENCL_C_VERSION, sizeof(version), &version, NULL);

		if(sscanf(version, "OpenCL C %d.%d", &major, &minor) < 2) {
			opencl_error(string_printf("OpenCL: failed to parse OpenCL C version string (%s).", version));
			return false;
		}

		if(!((major == req_major && minor >= req_minor) || (major > req_major))) {
			opencl_error(string_printf("OpenCL: C version 1.1 or later required, found %d.%d", major, minor));
			return false;
		}

		return true;
	}

	bool load_binary(const string& kernel_path, const string& clbin, const string *debug_src = NULL)
	{
		/* read binary into memory */
		vector<uint8_t> binary;

		if(!path_read_binary(clbin, binary)) {
			opencl_error(string_printf("OpenCL failed to read cached binary %s.", clbin.c_str()));
			return false;
		}

		/* create program */
		cl_int status;
		size_t size = binary.size();
		const uint8_t *bytes = &binary[0];

		cpProgram = clCreateProgramWithBinary(cxContext, 1, &cdDevice,
			&size, &bytes, &status, &ciErr);

		if(opencl_error(status) || opencl_error(ciErr)) {
			opencl_error(string_printf("OpenCL failed create program from cached binary %s.", clbin.c_str()));
			return false;
		}

		if(!build_kernel(kernel_path, debug_src))
			return false;

		return true;
	}

	bool save_binary(const string& clbin)
	{
		size_t size = 0;
		clGetProgramInfo(cpProgram, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &size, NULL);

		if(!size)
			return false;

		vector<uint8_t> binary(size);
		uint8_t *bytes = &binary[0];

		clGetProgramInfo(cpProgram, CL_PROGRAM_BINARIES, sizeof(uint8_t*), &bytes, NULL);

		if(!path_write_binary(clbin, binary)) {
			opencl_error(string_printf("OpenCL failed to write cached binary %s.", clbin.c_str()));
			return false;
		}

		return true;
	}

	bool build_kernel(const string& kernel_path, const string *debug_src = NULL)
	{
		string build_options = opencl_kernel_build_options(platform_name, debug_src);
	
		ciErr = clBuildProgram(cpProgram, 0, NULL, build_options.c_str(), NULL, NULL);

		/* show warnings even if build is successful */
		size_t ret_val_size = 0;

		clGetProgramBuildInfo(cpProgram, cdDevice, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);

		if(ret_val_size > 1) {
			vector<char> build_log(ret_val_size+1);
			clGetProgramBuildInfo(cpProgram, cdDevice, CL_PROGRAM_BUILD_LOG, ret_val_size, &build_log[0], NULL);

			build_log[ret_val_size] = '\0';
			fprintf(stderr, "OpenCL kernel build output:\n");
			fprintf(stderr, "%s\n", &build_log[0]);
		}

		if(ciErr != CL_SUCCESS) {
			opencl_error("OpenCL build failed: errors in console");
			return false;
		}

		return true;
	}

	bool compile_kernel(const string& kernel_path, const string& kernel_md5, const string *debug_src = NULL)
	{
		/* we compile kernels consisting of many files. unfortunately opencl
		 * kernel caches do not seem to recognize changes in included files.
		 * so we force recompile on changes by adding the md5 hash of all files */
		string source = "#include \"kernel.cl\" // " + kernel_md5 + "\n";
		source = path_source_replace_includes(source, kernel_path);

		if(debug_src)
			path_write_text(*debug_src, source);

		size_t source_len = source.size();
		const char *source_str = source.c_str();

		cpProgram = clCreateProgramWithSource(cxContext, 1, &source_str, &source_len, &ciErr);

		if(opencl_error(ciErr))
			return false;

		double starttime = time_dt();
		printf("Compiling OpenCL kernel ...\n");

		if(!build_kernel(kernel_path, debug_src))
			return false;

		printf("Kernel compilation finished in %.2lfs.\n", time_dt() - starttime);

		return true;
	}

	string device_md5_hash()
	{
		MD5Hash md5;
		char version[256], driver[256], name[256], vendor[256];

		clGetPlatformInfo(cpPlatform, CL_PLATFORM_VENDOR, sizeof(vendor), &vendor, NULL);
		clGetDeviceInfo(cdDevice, CL_DEVICE_VERSION, sizeof(version), &version, NULL);
		clGetDeviceInfo(cdDevice, CL_DEVICE_NAME, sizeof(name), &name, NULL);
		clGetDeviceInfo(cdDevice, CL_DRIVER_VERSION, sizeof(driver), &driver, NULL);

		md5.append((uint8_t*)vendor, strlen(vendor));
		md5.append((uint8_t*)version, strlen(version));
		md5.append((uint8_t*)name, strlen(name));
		md5.append((uint8_t*)driver, strlen(driver));

		string options = opencl_kernel_build_options(platform_name);
		md5.append((uint8_t*)options.c_str(), options.size());

		return md5.get_hex();
	}

	bool load_kernels(bool experimental)
	{
		/* verify if device was initialized */
		if(!device_initialized) {
			fprintf(stderr, "OpenCL: failed to initialize device.\n");
			return false;
		}

		/* try to use cached kernel */
		thread_scoped_lock cache_locker;
		cpProgram = OpenCLCache::get_program(cpPlatform, cdDevice, cache_locker);

		if(!cpProgram) {
			/* verify we have right opencl version */
			if(!opencl_version_check())
				return false;

			/* md5 hash to detect changes */
			string kernel_path = path_get("kernel");
			string kernel_md5 = path_files_md5_hash(kernel_path);
			string device_md5 = device_md5_hash();

			/* path to cached binary */
			string clbin = string_printf("cycles_kernel_%s_%s.clbin", device_md5.c_str(), kernel_md5.c_str());
			clbin = path_user_get(path_join("cache", clbin));

			/* path to preprocessed source for debugging */
			string clsrc, *debug_src = NULL;

			if(opencl_kernel_use_debug()) {
				clsrc = string_printf("cycles_kernel_%s_%s.cl", device_md5.c_str(), kernel_md5.c_str());
				clsrc = path_user_get(path_join("cache", clsrc));
				debug_src = &clsrc;
			}

			/* if exists already, try use it */
			if(path_exists(clbin) && load_binary(kernel_path, clbin, debug_src)) {
				/* kernel loaded from binary */
			}
			else {
				/* if does not exist or loading binary failed, compile kernel */
				if(!compile_kernel(kernel_path, kernel_md5, debug_src))
					return false;

				/* save binary for reuse */
				if(!save_binary(clbin))
					return false;
			}

			/* cache the program */
			OpenCLCache::store_program(cpPlatform, cdDevice, cpProgram, cache_locker);
		}

		/* find kernels */
		ckPathTraceKernel = clCreateKernel(cpProgram, "kernel_ocl_path_trace", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckFilmConvertByteKernel = clCreateKernel(cpProgram, "kernel_ocl_convert_to_byte", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckFilmConvertHalfFloatKernel = clCreateKernel(cpProgram, "kernel_ocl_convert_to_half_float", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckShaderKernel = clCreateKernel(cpProgram, "kernel_ocl_shader", &ciErr);
		if(opencl_error(ciErr))
			return false;

		ckBakeKernel = clCreateKernel(cpProgram, "kernel_ocl_bake", &ciErr);
		if(opencl_error(ciErr))
			return false;

		return true;
	}

	~OpenCLDevice()
	{
		task_pool.stop();

		if(null_mem)
			clReleaseMemObject(CL_MEM_PTR(null_mem));

		ConstMemMap::iterator mt;
		for(mt = const_mem_map.begin(); mt != const_mem_map.end(); mt++) {
			mem_free(*(mt->second));
			delete mt->second;
		}

		if(ckPathTraceKernel)
			clReleaseKernel(ckPathTraceKernel);  
		if(ckFilmConvertByteKernel)
			clReleaseKernel(ckFilmConvertByteKernel);  
		if(ckFilmConvertHalfFloatKernel)
			clReleaseKernel(ckFilmConvertHalfFloatKernel);  
		if(cpProgram)
			clReleaseProgram(cpProgram);
		if(cqCommandQueue)
			clReleaseCommandQueue(cqCommandQueue);
		if(cxContext)
			clReleaseContext(cxContext);
	}

	void mem_alloc(device_memory& mem, MemoryType type)
	{
		size_t size = mem.memory_size();

		cl_mem_flags mem_flag;
		void *mem_ptr = NULL;

		if(type == MEM_READ_ONLY)
			mem_flag = CL_MEM_READ_ONLY;
		else if(type == MEM_WRITE_ONLY)
			mem_flag = CL_MEM_WRITE_ONLY;
		else
			mem_flag = CL_MEM_READ_WRITE;

		mem.device_pointer = (device_ptr)clCreateBuffer(cxContext, mem_flag, size, mem_ptr, &ciErr);

		opencl_assert_err(ciErr, "clCreateBuffer");

		stats.mem_alloc(size);
		mem.device_size = size;
	}

	void mem_copy_to(device_memory& mem)
	{
		/* this is blocking */
		size_t size = mem.memory_size();
		opencl_assert(clEnqueueWriteBuffer(cqCommandQueue, CL_MEM_PTR(mem.device_pointer), CL_TRUE, 0, size, (void*)mem.data_pointer, 0, NULL, NULL));
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		size_t offset = elem*y*w;
		size_t size = elem*w*h;

		opencl_assert(clEnqueueReadBuffer(cqCommandQueue, CL_MEM_PTR(mem.device_pointer), CL_TRUE, offset, size, (uchar*)mem.data_pointer + offset, 0, NULL, NULL));
	}

	void mem_zero(device_memory& mem)
	{
		if(mem.device_pointer) {
			memset((void*)mem.data_pointer, 0, mem.memory_size());
			mem_copy_to(mem);
		}
	}

	void mem_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			opencl_assert(clReleaseMemObject(CL_MEM_PTR(mem.device_pointer)));
			mem.device_pointer = 0;

			stats.mem_free(mem.device_size);
			mem.device_size = 0;
		}
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		ConstMemMap::iterator i = const_mem_map.find(name);

		if(i == const_mem_map.end()) {
			device_vector<uchar> *data = new device_vector<uchar>();
			data->copy((uchar*)host, size);

			mem_alloc(*data, MEM_READ_ONLY);
			i = const_mem_map.insert(ConstMemMap::value_type(name, data)).first;
		}
		else {
			device_vector<uchar> *data = i->second;
			data->copy((uchar*)host, size);
		}

		mem_copy_to(*i->second);
	}

	void tex_alloc(const char *name, device_memory& mem, InterpolationType interpolation, bool periodic)
	{
		mem_alloc(mem, MEM_READ_ONLY);
		mem_copy_to(mem);
		assert(mem_map.find(name) == mem_map.end());
		mem_map.insert(MemMap::value_type(name, mem.device_pointer));
	}

	void tex_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			foreach(const MemMap::value_type& value, mem_map) {
				if(value.second == mem.device_pointer) {
					mem_map.erase(value.first);
					break;
				}
			}

			mem_free(mem);
		}
	}

	size_t global_size_round_up(int group_size, int global_size)
	{
		int r = global_size % group_size;
		return global_size + ((r == 0)? 0: group_size - r);
	}

	void enqueue_kernel(cl_kernel kernel, size_t w, size_t h)
	{
		size_t workgroup_size, max_work_items[3];

		clGetKernelWorkGroupInfo(kernel, cdDevice,
			CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &workgroup_size, NULL);
		clGetDeviceInfo(cdDevice,
			CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t)*3, max_work_items, NULL);
	
		/* try to divide evenly over 2 dimensions */
		size_t sqrt_workgroup_size = max((size_t)sqrt((double)workgroup_size), 1);
		size_t local_size[2] = {sqrt_workgroup_size, sqrt_workgroup_size};

		/* some implementations have max size 1 on 2nd dimension */
		if(local_size[1] > max_work_items[1]) {
			local_size[0] = workgroup_size/max_work_items[1];
			local_size[1] = max_work_items[1];
		}

		size_t global_size[2] = {global_size_round_up(local_size[0], w), global_size_round_up(local_size[1], h)};

		/* run kernel */
		opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, kernel, 2, NULL, global_size, NULL, 0, NULL, NULL));
		opencl_assert(clFlush(cqCommandQueue));
	}

	void path_trace(RenderTile& rtile, int sample)
	{
		/* cast arguments to cl types */
		cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
		cl_mem d_buffer = CL_MEM_PTR(rtile.buffer);
		cl_mem d_rng_state = CL_MEM_PTR(rtile.rng_state);
		cl_int d_x = rtile.x;
		cl_int d_y = rtile.y;
		cl_int d_w = rtile.w;
		cl_int d_h = rtile.h;
		cl_int d_sample = sample;
		cl_int d_offset = rtile.offset;
		cl_int d_stride = rtile.stride;

		/* sample arguments */
		cl_uint narg = 0;

		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_buffer), (void*)&d_buffer));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_rng_state), (void*)&d_rng_state));

#define KERNEL_TEX(type, ttype, name) \
	set_kernel_arg_mem(ckPathTraceKernel, &narg, #name);
#include "kernel_textures.h"

		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_sample), (void*)&d_sample));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_x), (void*)&d_x));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_y), (void*)&d_y));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_w), (void*)&d_w));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_h), (void*)&d_h));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_offset), (void*)&d_offset));
		opencl_assert(clSetKernelArg(ckPathTraceKernel, narg++, sizeof(d_stride), (void*)&d_stride));

		enqueue_kernel(ckPathTraceKernel, d_w, d_h);
	}

	void set_kernel_arg_mem(cl_kernel kernel, cl_uint *narg, const char *name)
	{
		cl_mem ptr;

		MemMap::iterator i = mem_map.find(name);
		if(i != mem_map.end()) {
			ptr = CL_MEM_PTR(i->second);
		}
		else {
			/* work around NULL not working, even though the spec says otherwise */
			ptr = CL_MEM_PTR(null_mem);
		}
		
		opencl_assert(clSetKernelArg(kernel, (*narg)++, sizeof(ptr), (void*)&ptr));
	}

	void film_convert(DeviceTask& task, device_ptr buffer, device_ptr rgba_byte, device_ptr rgba_half)
	{
		/* cast arguments to cl types */
		cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
		cl_mem d_rgba = (rgba_byte)? CL_MEM_PTR(rgba_byte): CL_MEM_PTR(rgba_half);
		cl_mem d_buffer = CL_MEM_PTR(buffer);
		cl_int d_x = task.x;
		cl_int d_y = task.y;
		cl_int d_w = task.w;
		cl_int d_h = task.h;
		cl_float d_sample_scale = 1.0f/(task.sample + 1);
		cl_int d_offset = task.offset;
		cl_int d_stride = task.stride;

		/* sample arguments */
		cl_uint narg = 0;


		cl_kernel ckFilmConvertKernel = (rgba_byte)? ckFilmConvertByteKernel: ckFilmConvertHalfFloatKernel;

		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_data), (void*)&d_data));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_rgba), (void*)&d_rgba));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_buffer), (void*)&d_buffer));

#define KERNEL_TEX(type, ttype, name) \
	set_kernel_arg_mem(ckFilmConvertKernel, &narg, #name);
#include "kernel_textures.h"

		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_sample_scale), (void*)&d_sample_scale));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_x), (void*)&d_x));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_y), (void*)&d_y));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_w), (void*)&d_w));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_h), (void*)&d_h));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_offset), (void*)&d_offset));
		opencl_assert(clSetKernelArg(ckFilmConvertKernel, narg++, sizeof(d_stride), (void*)&d_stride));



		enqueue_kernel(ckFilmConvertKernel, d_w, d_h);
	}

	void shader(DeviceTask& task)
	{
		/* cast arguments to cl types */
		cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
		cl_mem d_input = CL_MEM_PTR(task.shader_input);
		cl_mem d_output = CL_MEM_PTR(task.shader_output);
		cl_int d_shader_eval_type = task.shader_eval_type;
		cl_int d_shader_x = task.shader_x;
		cl_int d_shader_w = task.shader_w;
		cl_int d_offset = task.offset;

		/* sample arguments */
		cl_uint narg = 0;

		cl_kernel kernel;

		if(task.shader_eval_type >= SHADER_EVAL_BAKE)
			kernel = ckBakeKernel;
		else
			kernel = ckShaderKernel;

		for(int sample = 0; sample < task.num_samples; sample++) {

			if(task.get_cancel())
				break;

			cl_int d_sample = sample;

			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_data), (void*)&d_data));
			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_input), (void*)&d_input));
			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_output), (void*)&d_output));

#define KERNEL_TEX(type, ttype, name) \
		set_kernel_arg_mem(kernel, &narg, #name);
#include "kernel_textures.h"

			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_shader_eval_type), (void*)&d_shader_eval_type));
			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_shader_x), (void*)&d_shader_x));
			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_shader_w), (void*)&d_shader_w));
			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_offset), (void*)&d_offset));
			opencl_assert(clSetKernelArg(kernel, narg++, sizeof(d_sample), (void*)&d_sample));

			enqueue_kernel(kernel, task.shader_w, 1);

			task.update_progress(NULL);
		}
	}

	void thread_run(DeviceTask *task)
	{
		if(task->type == DeviceTask::FILM_CONVERT) {
			film_convert(*task, task->buffer, task->rgba_byte, task->rgba_half);
		}
		else if(task->type == DeviceTask::SHADER) {
			shader(*task);
		}
		else if(task->type == DeviceTask::PATH_TRACE) {
			RenderTile tile;
			
			/* keep rendering tiles until done */
			while(task->acquire_tile(this, tile)) {
				int start_sample = tile.start_sample;
				int end_sample = tile.start_sample + tile.num_samples;

				for(int sample = start_sample; sample < end_sample; sample++) {
					if(task->get_cancel()) {
						if(task->need_finish_queue == false)
							break;
					}

					path_trace(tile, sample);

					tile.sample = sample + 1;

					task->update_progress(&tile);
				}

				task->release_tile(tile);
			}
		}
	}

	class OpenCLDeviceTask : public DeviceTask {
	public:
		OpenCLDeviceTask(OpenCLDevice *device, DeviceTask& task)
		: DeviceTask(task)
		{
			run = function_bind(&OpenCLDevice::thread_run, device, this);
		}
	};

	int get_split_task_count(DeviceTask& task)
	{
		return 1;
	}

	void task_add(DeviceTask& task)
	{
		task_pool.push(new OpenCLDeviceTask(this, task));
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

Device *device_opencl_create(DeviceInfo& info, Stats &stats, bool background)
{
	return new OpenCLDevice(info, stats, background);
}

bool device_opencl_init(void) {
	static bool initialized = false;
	static bool result = false;

	if (initialized)
		return result;

	initialized = true;

	// OpenCL disabled for now, only works with this environment variable set
	if(!getenv("CYCLES_OPENCL_TEST")) {
		result = false;
	}
	else {
		result = clewInit() == CLEW_SUCCESS;
	}

	return result;
}

void device_opencl_info(vector<DeviceInfo>& devices)
{
	vector<cl_device_id> device_ids;
	cl_uint num_devices = 0;
	vector<cl_platform_id> platform_ids;
	cl_uint num_platforms = 0;

	/* get devices */
	if(clGetPlatformIDs(0, NULL, &num_platforms) != CL_SUCCESS || num_platforms == 0)
		return;
	
	platform_ids.resize(num_platforms);

	if(clGetPlatformIDs(num_platforms, &platform_ids[0], NULL) != CL_SUCCESS)
		return;

	/* devices are numbered consecutively across platforms */
	int num_base = 0;

	for (int platform = 0; platform < num_platforms; platform++, num_base += num_devices) {
		num_devices = 0;
		if(clGetDeviceIDs(platform_ids[platform], opencl_device_type(), 0, NULL, &num_devices) != CL_SUCCESS || num_devices == 0)
			continue;

		device_ids.resize(num_devices);

		if(clGetDeviceIDs(platform_ids[platform], opencl_device_type(), num_devices, &device_ids[0], NULL) != CL_SUCCESS)
			continue;

		char pname[256];
		clGetPlatformInfo(platform_ids[platform], CL_PLATFORM_NAME, sizeof(pname), &pname, NULL);
		string platform_name = pname;

		/* add devices */
		for(int num = 0; num < num_devices; num++) {
			cl_device_id device_id = device_ids[num];
			char name[1024] = "\0";

			if(clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(name), &name, NULL) != CL_SUCCESS)
				continue;

			DeviceInfo info;

			info.type = DEVICE_OPENCL;
			info.description = string(name);
			info.num = num_base + num;
			info.id = string_printf("OPENCL_%d", info.num);
			/* we don't know if it's used for display, but assume it is */
			info.display_device = true;
			info.advanced_shading = opencl_kernel_use_advanced_shading(platform_name);
			info.pack_images = true;

			devices.push_back(info);
		}
	}
}

CCL_NAMESPACE_END

#endif /* WITH_OPENCL */


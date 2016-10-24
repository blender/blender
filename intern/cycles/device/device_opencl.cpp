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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clew.h"

#include "device.h"
#include "device_intern.h"

#include "buffers.h"

#include "util_debug.h"
#include "util_foreach.h"
#include "util_logging.h"
#include "util_map.h"
#include "util_math.h"
#include "util_md5.h"
#include "util_opengl.h"
#include "util_path.h"
#include "util_time.h"

CCL_NAMESPACE_BEGIN

#define CL_MEM_PTR(p) ((cl_mem)(uintptr_t)(p))

/* Macro declarations used with split kernel */

/* Macro to enable/disable work-stealing */
#define __WORK_STEALING__

#define SPLIT_KERNEL_LOCAL_SIZE_X 64
#define SPLIT_KERNEL_LOCAL_SIZE_Y 1

/* This value may be tuned according to the scene we are rendering.
 *
 * Modifying PATH_ITER_INC_FACTOR value proportional to number of expected
 * ray-bounces will improve performance.
 */
#define PATH_ITER_INC_FACTOR 8

/* When allocate global memory in chunks. We may not be able to
 * allocate exactly "CL_DEVICE_MAX_MEM_ALLOC_SIZE" bytes in chunks;
 * Since some bytes may be needed for aligning chunks of memory;
 * This is the amount of memory that we dedicate for that purpose.
 */
#define DATA_ALLOCATION_MEM_FACTOR 5000000 //5MB

struct OpenCLPlatformDevice {
	OpenCLPlatformDevice(cl_platform_id platform_id,
	                     const string& platform_name,
	                     cl_device_id device_id,
	                     cl_device_type device_type,
	                     const string& device_name)
	  : platform_id(platform_id),
	    platform_name(platform_name),
	    device_id(device_id),
	    device_type(device_type),
	    device_name(device_name) {}
	cl_platform_id platform_id;
	string platform_name;
	cl_device_id device_id;
	cl_device_type device_type;
	string device_name;
};

namespace {

cl_device_type opencl_device_type()
{
	switch(DebugFlags().opencl.device_type)
	{
		case DebugFlags::OpenCL::DEVICE_NONE:
			return 0;
		case DebugFlags::OpenCL::DEVICE_ALL:
			return CL_DEVICE_TYPE_ALL;
		case DebugFlags::OpenCL::DEVICE_DEFAULT:
			return CL_DEVICE_TYPE_DEFAULT;
		case DebugFlags::OpenCL::DEVICE_CPU:
			return CL_DEVICE_TYPE_CPU;
		case DebugFlags::OpenCL::DEVICE_GPU:
			return CL_DEVICE_TYPE_GPU;
		case DebugFlags::OpenCL::DEVICE_ACCELERATOR:
			return CL_DEVICE_TYPE_ACCELERATOR;
		default:
			return CL_DEVICE_TYPE_ALL;
	}
}

inline bool opencl_kernel_use_debug()
{
	return DebugFlags().opencl.debug;
}

bool opencl_kernel_use_advanced_shading(const string& platform)
{
	/* keep this in sync with kernel_types.h! */
	if(platform == "NVIDIA CUDA")
		return true;
	else if(platform == "Apple")
		return true;
	else if(platform == "AMD Accelerated Parallel Processing")
		return true;
	else if(platform == "Intel(R) OpenCL")
		return true;
	/* Make sure officially unsupported OpenCL platforms
	 * does not set up to use advanced shading.
	 */
	return false;
}

bool opencl_kernel_use_split(const string& platform_name,
                             const cl_device_type device_type)
{
	if(DebugFlags().opencl.kernel_type == DebugFlags::OpenCL::KERNEL_SPLIT) {
		VLOG(1) << "Forcing split kernel to use.";
		return true;
	}
	if(DebugFlags().opencl.kernel_type == DebugFlags::OpenCL::KERNEL_MEGA) {
		VLOG(1) << "Forcing mega kernel to use.";
		return false;
	}
	/* TODO(sergey): Replace string lookups with more enum-like API,
	 * similar to device/vendor checks blender's gpu.
	 */
	if(platform_name == "AMD Accelerated Parallel Processing" &&
	   device_type == CL_DEVICE_TYPE_GPU)
	{
		return true;
	}
	return false;
}

bool opencl_device_supported(const string& platform_name,
                             const cl_device_id device_id)
{
	cl_device_type device_type;
	clGetDeviceInfo(device_id,
	                CL_DEVICE_TYPE,
	                sizeof(cl_device_type),
	                &device_type,
	                NULL);
	if(platform_name == "AMD Accelerated Parallel Processing" &&
	   device_type == CL_DEVICE_TYPE_GPU)
	{
		return true;
	}
	if(platform_name == "Apple" && device_type == CL_DEVICE_TYPE_GPU) {
		return true;
	}
	return false;
}

bool opencl_platform_version_check(cl_platform_id platform,
                                   string *error = NULL)
{
	const int req_major = 1, req_minor = 1;
	int major, minor;
	char version[256];
	clGetPlatformInfo(platform,
	                  CL_PLATFORM_VERSION,
	                  sizeof(version),
	                  &version,
	                  NULL);
	if(sscanf(version, "OpenCL %d.%d", &major, &minor) < 2) {
		if(error != NULL) {
			*error = string_printf("OpenCL: failed to parse platform version string (%s).", version);
		}
		return false;
	}
	if(!((major == req_major && minor >= req_minor) || (major > req_major))) {
		if(error != NULL) {
			*error = string_printf("OpenCL: platform version 1.1 or later required, found %d.%d", major, minor);
		}
		return false;
	}
	if(error != NULL) {
		*error = "";
	}
	return true;
}

bool opencl_device_version_check(cl_device_id device,
                                 string *error = NULL)
{
	const int req_major = 1, req_minor = 1;
	int major, minor;
	char version[256];
	clGetDeviceInfo(device,
	                CL_DEVICE_OPENCL_C_VERSION,
	                sizeof(version),
	                &version,
	                NULL);
	if(sscanf(version, "OpenCL C %d.%d", &major, &minor) < 2) {
		if(error != NULL) {
			*error = string_printf("OpenCL: failed to parse OpenCL C version string (%s).", version);
		}
		return false;
	}
	if(!((major == req_major && minor >= req_minor) || (major > req_major))) {
		if(error != NULL) {
			*error = string_printf("OpenCL: C version 1.1 or later required, found %d.%d", major, minor);
		}
		return false;
	}
	if(error != NULL) {
		*error = "";
	}
	return true;
}

void opencl_get_usable_devices(vector<OpenCLPlatformDevice> *usable_devices)
{
	const bool force_all_platforms =
		(DebugFlags().opencl.kernel_type != DebugFlags::OpenCL::KERNEL_DEFAULT);
	const cl_device_type device_type = opencl_device_type();
	static bool first_time = true;
#define FIRST_VLOG(severity) if(first_time) VLOG(severity)

	usable_devices->clear();

	if(device_type == 0) {
		FIRST_VLOG(2) << "OpenCL devices are forced to be disabled.";
		first_time = false;
		return;
	}

	vector<cl_device_id> device_ids;
	cl_uint num_devices = 0;
	vector<cl_platform_id> platform_ids;
	cl_uint num_platforms = 0;

	/* Get devices. */
	if(clGetPlatformIDs(0, NULL, &num_platforms) != CL_SUCCESS ||
	   num_platforms == 0)
	{
		FIRST_VLOG(2) << "No OpenCL platforms were found.";
		first_time = false;
		return;
	}
	platform_ids.resize(num_platforms);
	if(clGetPlatformIDs(num_platforms, &platform_ids[0], NULL) != CL_SUCCESS) {
		FIRST_VLOG(2) << "Failed to fetch platform IDs from the driver..";
		first_time = false;
		return;
	}
	/* Devices are numbered consecutively across platforms. */
	for(int platform = 0; platform < num_platforms; platform++) {
		cl_platform_id platform_id = platform_ids[platform];
		char pname[256];
		if(clGetPlatformInfo(platform_id,
		                     CL_PLATFORM_NAME,
		                     sizeof(pname),
		                     &pname,
		                     NULL) != CL_SUCCESS)
		{
			FIRST_VLOG(2) << "Failed to get platform name, ignoring.";
			continue;
		}
		string platform_name = pname;
		FIRST_VLOG(2) << "Enumerating devices for platform "
		              << platform_name << ".";
		if(!opencl_platform_version_check(platform_id)) {
			FIRST_VLOG(2) << "Ignoring platform " << platform_name
			              << " due to too old compiler version.";
			continue;
		}
		num_devices = 0;
		if(clGetDeviceIDs(platform_id,
		                  device_type,
		                  0,
		                  NULL,
		                  &num_devices) != CL_SUCCESS || num_devices == 0)
		{
			FIRST_VLOG(2) << "Ignoring platform " << platform_name
			              << ", failed to fetch number of devices.";
			continue;
		}
		device_ids.resize(num_devices);
		if(clGetDeviceIDs(platform_id,
		                  device_type,
		                  num_devices,
		                  &device_ids[0],
		                  NULL) != CL_SUCCESS)
		{
			FIRST_VLOG(2) << "Ignoring platform " << platform_name
			              << ", failed to fetch devices list.";
			continue;
		}
		for(int num = 0; num < num_devices; num++) {
			cl_device_id device_id = device_ids[num];
			char device_name[1024] = "\0";
			if(clGetDeviceInfo(device_id,
			                   CL_DEVICE_NAME,
			                   sizeof(device_name),
			                   &device_name,
			                   NULL) != CL_SUCCESS)
			{
				FIRST_VLOG(2) << "Failed to fetch device name, ignoring.";
				continue;
			}
			if(!opencl_device_version_check(device_id)) {
				FIRST_VLOG(2) << "Ignoring device " << device_name
				              << " due to old compiler version.";
				continue;
			}
			if(force_all_platforms ||
			   opencl_device_supported(platform_name, device_id))
			{
				cl_device_type device_type;
				if(clGetDeviceInfo(device_id,
				                   CL_DEVICE_TYPE,
				                   sizeof(cl_device_type),
				                   &device_type,
				                   NULL) != CL_SUCCESS)
				{
					FIRST_VLOG(2) << "Ignoring device " << device_name
					              << ", failed to fetch device type.";
					continue;
				}
				FIRST_VLOG(2) << "Adding new device " << device_name << ".";
				usable_devices->push_back(OpenCLPlatformDevice(platform_id,
				                                               platform_name,
				                                               device_id,
				                                               device_type,
				                                               device_name));
			}
			else {
				FIRST_VLOG(2) << "Ignoring device " << device_name
				              << ", not officially supported yet.";
			}
		}
	}
	first_time = false;
}

}  /* namespace */

/* Thread safe cache for contexts and programs.
 *
 * TODO(sergey): Make it more generous, so it can contain any type of program
 * without hardcoding possible program types in the slot.
 */
class OpenCLCache
{
	struct Slot
	{
		thread_mutex *mutex;
		cl_context context;
		/* cl_program for shader, bake, film_convert kernels (used in OpenCLDeviceBase) */
		cl_program ocl_dev_base_program;
		/* cl_program for megakernel (used in OpenCLDeviceMegaKernel) */
		cl_program ocl_dev_megakernel_program;

		Slot() : mutex(NULL),
		         context(NULL),
		         ocl_dev_base_program(NULL),
		         ocl_dev_megakernel_program(NULL) {}

		Slot(const Slot& rhs)
		    : mutex(rhs.mutex),
		      context(rhs.context),
		      ocl_dev_base_program(rhs.ocl_dev_base_program),
		      ocl_dev_megakernel_program(rhs.ocl_dev_megakernel_program)
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
	static T get_something(cl_platform_id platform,
	                       cl_device_id device,
	                       T Slot::*member,
	                       thread_scoped_lock& slot_locker)
	{
		assert(platform != NULL);

		OpenCLCache& self = global_instance();

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
	static void store_something(cl_platform_id platform,
	                            cl_device_id device,
	                            T thing,
	                            T Slot::*member,
	                            thread_scoped_lock& slot_locker)
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

	enum ProgramName {
		OCL_DEV_BASE_PROGRAM,
		OCL_DEV_MEGAKERNEL_PROGRAM,
	};

	/* see get_something comment */
	static cl_context get_context(cl_platform_id platform,
	                              cl_device_id device,
	                              thread_scoped_lock& slot_locker)
	{
		cl_context context = get_something<cl_context>(platform,
		                                               device,
		                                               &Slot::context,
		                                               slot_locker);

		if(!context)
			return NULL;

		/* caller is going to release it when done with it, so retain it */
		cl_int ciErr = clRetainContext(context);
		assert(ciErr == CL_SUCCESS);
		(void)ciErr;

		return context;
	}

	/* see get_something comment */
	static cl_program get_program(cl_platform_id platform,
	                              cl_device_id device,
	                              ProgramName program_name,
	                              thread_scoped_lock& slot_locker)
	{
		cl_program program = NULL;

		switch(program_name) {
			case OCL_DEV_BASE_PROGRAM:
				/* Get program related to OpenCLDeviceBase */
				program = get_something<cl_program>(platform,
				                                    device,
				                                    &Slot::ocl_dev_base_program,
				                                    slot_locker);
				break;
			case OCL_DEV_MEGAKERNEL_PROGRAM:
				/* Get program related to megakernel */
				program = get_something<cl_program>(platform,
				                                    device,
				                                    &Slot::ocl_dev_megakernel_program,
				                                    slot_locker);
				break;
		default:
			assert(!"Invalid program name");
		}

		if(!program)
			return NULL;

		/* caller is going to release it when done with it, so retain it */
		cl_int ciErr = clRetainProgram(program);
		assert(ciErr == CL_SUCCESS);
		(void)ciErr;

		return program;
	}

	/* see store_something comment */
	static void store_context(cl_platform_id platform,
	                          cl_device_id device,
	                          cl_context context,
	                          thread_scoped_lock& slot_locker)
	{
		store_something<cl_context>(platform,
		                            device,
		                            context,
		                            &Slot::context,
		                            slot_locker);

		/* increment reference count in OpenCL.
		 * The caller is going to release the object when done with it. */
		cl_int ciErr = clRetainContext(context);
		assert(ciErr == CL_SUCCESS);
		(void)ciErr;
	}

	/* see store_something comment */
	static void store_program(cl_platform_id platform,
	                          cl_device_id device,
	                          cl_program program,
	                          ProgramName program_name,
	                          thread_scoped_lock& slot_locker)
	{
		switch(program_name) {
			case OCL_DEV_BASE_PROGRAM:
				store_something<cl_program>(platform,
				                            device,
				                            program,
				                            &Slot::ocl_dev_base_program,
				                            slot_locker);
				break;
			case OCL_DEV_MEGAKERNEL_PROGRAM:
				store_something<cl_program>(platform,
				                            device,
				                            program,
				                            &Slot::ocl_dev_megakernel_program,
				                            slot_locker);
				break;
			default:
				assert(!"Invalid program name\n");
				return;
		}

		/* Increment reference count in OpenCL.
		 * The caller is going to release the object when done with it.
		 */
		cl_int ciErr = clRetainProgram(program);
		assert(ciErr == CL_SUCCESS);
		(void)ciErr;
	}

	/* Discard all cached contexts and programs.  */
	static void flush()
	{
		OpenCLCache &self = global_instance();
		thread_scoped_lock cache_lock(self.cache_lock);

		foreach(CacheMap::value_type &item, self.cache) {
			if(item.second.ocl_dev_base_program != NULL)
				clReleaseProgram(item.second.ocl_dev_base_program);
			if(item.second.ocl_dev_megakernel_program != NULL)
				clReleaseProgram(item.second.ocl_dev_megakernel_program);
			if(item.second.context != NULL)
				clReleaseContext(item.second.context);
		}

		self.cache.clear();
	}
};

class OpenCLDeviceBase : public Device
{
public:
	DedicatedTaskPool task_pool;
	cl_context cxContext;
	cl_command_queue cqCommandQueue;
	cl_platform_id cpPlatform;
	cl_device_id cdDevice;
	cl_program cpProgram;
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

	OpenCLDeviceBase(DeviceInfo& info, Stats &stats, bool background_)
	: Device(info, stats, background_)
	{
		cpPlatform = NULL;
		cdDevice = NULL;
		cxContext = NULL;
		cqCommandQueue = NULL;
		cpProgram = NULL;
		ckFilmConvertByteKernel = NULL;
		ckFilmConvertHalfFloatKernel = NULL;
		ckShaderKernel = NULL;
		ckBakeKernel = NULL;
		null_mem = 0;
		device_initialized = false;

		vector<OpenCLPlatformDevice> usable_devices;
		opencl_get_usable_devices(&usable_devices);
		if(usable_devices.size() == 0) {
			opencl_error("OpenCL: no devices found.");
			return;
		}
		assert(info.num < usable_devices.size());
		OpenCLPlatformDevice& platform_device = usable_devices[info.num];
		cpPlatform = platform_device.platform_id;
		cdDevice = platform_device.device_id;
		platform_name = platform_device.platform_name;
		VLOG(2) << "Creating new Cycles device for OpenCL platform "
		        << platform_name << ", device "
		        << platform_device.device_name << ".";

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

		fprintf(stderr, "Device init success\n");
		device_initialized = true;
	}

	static void CL_CALLBACK context_notify_callback(const char *err_info,
		const void * /*private_info*/, size_t /*cb*/, void *user_data)
	{
		char name[256];
		clGetDeviceInfo((cl_device_id)user_data, CL_DEVICE_NAME, sizeof(name), &name, NULL);

		fprintf(stderr, "OpenCL error (%s): %s\n", name, err_info);
	}

	bool opencl_version_check()
	{
		string error;
		if(!opencl_platform_version_check(cpPlatform, &error)) {
			opencl_error(error);
			return false;
		}
		if(!opencl_device_version_check(cdDevice, &error)) {
			opencl_error(error);
			return false;
		}
		return true;
	}

	bool load_binary(const string& /*kernel_path*/,
	                 const string& clbin,
	                 const string& custom_kernel_build_options,
	                 cl_program *program,
	                 const string *debug_src = NULL)
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

		*program = clCreateProgramWithBinary(cxContext, 1, &cdDevice,
			&size, &bytes, &status, &ciErr);

		if(opencl_error(status) || opencl_error(ciErr)) {
			opencl_error(string_printf("OpenCL failed create program from cached binary %s.", clbin.c_str()));
			return false;
		}

		if(!build_kernel(program, custom_kernel_build_options, debug_src))
			return false;

		return true;
	}

	bool save_binary(cl_program *program, const string& clbin)
	{
		size_t size = 0;
		clGetProgramInfo(*program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &size, NULL);

		if(!size)
			return false;

		vector<uint8_t> binary(size);
		uint8_t *bytes = &binary[0];

		clGetProgramInfo(*program, CL_PROGRAM_BINARIES, sizeof(uint8_t*), &bytes, NULL);

		if(!path_write_binary(clbin, binary)) {
			opencl_error(string_printf("OpenCL failed to write cached binary %s.", clbin.c_str()));
			return false;
		}

		return true;
	}

	bool build_kernel(cl_program *kernel_program,
	                  const string& custom_kernel_build_options,
	                  const string *debug_src = NULL)
	{
		string build_options;
		build_options = kernel_build_options(debug_src) + custom_kernel_build_options;

		ciErr = clBuildProgram(*kernel_program, 0, NULL, build_options.c_str(), NULL, NULL);

		/* show warnings even if build is successful */
		size_t ret_val_size = 0;

		clGetProgramBuildInfo(*kernel_program, cdDevice, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);

		if(ret_val_size > 1) {
			vector<char> build_log(ret_val_size + 1);
			clGetProgramBuildInfo(*kernel_program, cdDevice, CL_PROGRAM_BUILD_LOG, ret_val_size, &build_log[0], NULL);

			build_log[ret_val_size] = '\0';
			/* Skip meaningless empty output from the NVidia compiler. */
			if(!(ret_val_size == 2 && build_log[0] == '\n')) {
				fprintf(stderr, "OpenCL kernel build output:\n");
				fprintf(stderr, "%s\n", &build_log[0]);
			}
		}

		if(ciErr != CL_SUCCESS) {
			opencl_error("OpenCL build failed: errors in console");
			fprintf(stderr, "Build error: %s\n", clewErrorString(ciErr));
			return false;
		}

		return true;
	}

	bool compile_kernel(const string& kernel_name,
	                    const string& kernel_path,
	                    const string& source,
	                    const string& custom_kernel_build_options,
	                    cl_program *kernel_program,
	                    const string *debug_src = NULL)
	{
		/* We compile kernels consisting of many files. unfortunately OpenCL
		 * kernel caches do not seem to recognize changes in included files.
		 * so we force recompile on changes by adding the md5 hash of all files.
		 */
		string inlined_source = path_source_replace_includes(source,
		                                                     kernel_path);

		if(debug_src) {
			path_write_text(*debug_src, inlined_source);
		}

		size_t source_len = inlined_source.size();
		const char *source_str = inlined_source.c_str();

		*kernel_program = clCreateProgramWithSource(cxContext,
		                                            1,
		                                            &source_str,
		                                            &source_len,
		                                            &ciErr);

		if(opencl_error(ciErr)) {
			return false;
		}

		double starttime = time_dt();
		printf("Compiling %s OpenCL kernel ...\n", kernel_name.c_str());
		/* TODO(sergey): Report which kernel is being compiled
		 * as well (megakernel or which of split kernels etc..).
		 */
		printf("Build flags: %s\n", custom_kernel_build_options.c_str());

		if(!build_kernel(kernel_program, custom_kernel_build_options, debug_src))
			return false;

		printf("Kernel compilation finished in %.2lfs.\n", time_dt() - starttime);

		return true;
	}

	string device_md5_hash(string kernel_custom_build_options = "")
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

		string options = kernel_build_options();
		options += kernel_custom_build_options;
		md5.append((uint8_t*)options.c_str(), options.size());

		return md5.get_hex();
	}

	bool load_kernels(const DeviceRequestedFeatures& requested_features)
	{
		/* Verify if device was initialized. */
		if(!device_initialized) {
			fprintf(stderr, "OpenCL: failed to initialize device.\n");
			return false;
		}

		/* Try to use cached kernel. */
		thread_scoped_lock cache_locker;
		cpProgram = load_cached_kernel(requested_features,
		                               OpenCLCache::OCL_DEV_BASE_PROGRAM,
		                               cache_locker);

		if(!cpProgram) {
			VLOG(2) << "No cached OpenCL kernel.";

			/* Verify we have right opencl version. */
			if(!opencl_version_check())
				return false;

			string build_flags = build_options_for_base_program(requested_features);

			/* Calculate md5 hashes to detect changes. */
			string kernel_path = path_get("kernel");
			string kernel_md5 = path_files_md5_hash(kernel_path);
			string device_md5 = device_md5_hash(build_flags);

			/* Path to cached binary.
			 *
			 * TODO(sergey): Seems we could de-duplicate all this string_printf()
			 * calls with some utility function which will give file name for a
			 * given hashes..
			 */
			string clbin = string_printf("cycles_kernel_%s_%s.clbin",
			                             device_md5.c_str(),
			                             kernel_md5.c_str());
			clbin = path_user_get(path_join("cache", clbin));

			/* path to preprocessed source for debugging */
			string clsrc, *debug_src = NULL;

			if(opencl_kernel_use_debug()) {
				clsrc = string_printf("cycles_kernel_%s_%s.cl",
				                      device_md5.c_str(),
				                      kernel_md5.c_str());
				clsrc = path_user_get(path_join("cache", clsrc));
				debug_src = &clsrc;
			}

			/* If binary kernel exists already, try use it. */
			if(path_exists(clbin) && load_binary(kernel_path,
			                                     clbin,
			                                     build_flags,
			                                     &cpProgram))
			{
				/* Kernel loaded from binary, nothing to do. */
				VLOG(2) << "Loaded kernel from " << clbin << ".";
			}
			else {
				VLOG(2) << "Kernel file " << clbin << " either doesn't exist or failed to be loaded by driver.";
				string init_kernel_source = "#include \"kernels/opencl/kernel.cl\" // " + kernel_md5 + "\n";

				/* If does not exist or loading binary failed, compile kernel. */
				if(!compile_kernel("base_kernel",
				                   kernel_path,
				                   init_kernel_source,
				                   build_flags,
				                   &cpProgram,
				                   debug_src))
				{
					return false;
				}

				/* Save binary for reuse. */
				if(!save_binary(&cpProgram, clbin)) {
					return false;
				}
			}

			/* Cache the program. */
			store_cached_kernel(cpPlatform,
			                    cdDevice,
			                    cpProgram,
			                    OpenCLCache::OCL_DEV_BASE_PROGRAM,
			                    cache_locker);
		}
		else {
			VLOG(2) << "Found cached OpenCL kernel.";
		}

		/* Find kernels. */
#define FIND_KERNEL(kernel_var, kernel_name) \
		do { \
			kernel_var = clCreateKernel(cpProgram, "kernel_ocl_" kernel_name, &ciErr); \
			if(opencl_error(ciErr)) \
				return false; \
		} while(0)

		FIND_KERNEL(ckFilmConvertByteKernel, "convert_to_byte");
		FIND_KERNEL(ckFilmConvertHalfFloatKernel, "convert_to_half_float");
		FIND_KERNEL(ckShaderKernel, "shader");
		FIND_KERNEL(ckBakeKernel, "bake");

#undef FIND_KERNEL
		return true;
	}

	~OpenCLDeviceBase()
	{
		task_pool.stop();

		if(null_mem)
			clReleaseMemObject(CL_MEM_PTR(null_mem));

		ConstMemMap::iterator mt;
		for(mt = const_mem_map.begin(); mt != const_mem_map.end(); mt++) {
			mem_free(*(mt->second));
			delete mt->second;
		}

		if(ckFilmConvertByteKernel)
			clReleaseKernel(ckFilmConvertByteKernel);
		if(ckFilmConvertHalfFloatKernel)
			clReleaseKernel(ckFilmConvertHalfFloatKernel);
		if(ckShaderKernel)
			clReleaseKernel(ckShaderKernel);
		if(ckBakeKernel)
			clReleaseKernel(ckBakeKernel);
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

		/* Zero-size allocation might be invoked by render, but not really
		 * supported by OpenCL. Using NULL as device pointer also doesn't really
		 * work for some reason, so for the time being we'll use special case
		 * will null_mem buffer.
		 */
		if(size != 0) {
			mem.device_pointer = (device_ptr)clCreateBuffer(cxContext,
			                                                mem_flag,
			                                                size,
			                                                mem_ptr,
			                                                &ciErr);
			opencl_assert_err(ciErr, "clCreateBuffer");
		}
		else {
			mem.device_pointer = null_mem;
		}

		stats.mem_alloc(size);
		mem.device_size = size;
	}

	void mem_copy_to(device_memory& mem)
	{
		/* this is blocking */
		size_t size = mem.memory_size();
		if(size != 0) {
			opencl_assert(clEnqueueWriteBuffer(cqCommandQueue,
			                                   CL_MEM_PTR(mem.device_pointer),
			                                   CL_TRUE,
			                                   0,
			                                   size,
			                                   (void*)mem.data_pointer,
			                                   0,
			                                   NULL, NULL));
		}
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		size_t offset = elem*y*w;
		size_t size = elem*w*h;
		assert(size != 0);
		opencl_assert(clEnqueueReadBuffer(cqCommandQueue,
		                                  CL_MEM_PTR(mem.device_pointer),
		                                  CL_TRUE,
		                                  offset,
		                                  size,
		                                  (uchar*)mem.data_pointer + offset,
		                                  0,
		                                  NULL, NULL));
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
			if(mem.device_pointer != null_mem) {
				opencl_assert(clReleaseMemObject(CL_MEM_PTR(mem.device_pointer)));
			}
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

	void tex_alloc(const char *name,
	               device_memory& mem,
	               InterpolationType /*interpolation*/,
	               ExtensionType /*extension*/)
	{
		VLOG(1) << "Texture allocate: " << name << ", "
		        << string_human_readable_number(mem.memory_size()) << " bytes. ("
		        << string_human_readable_size(mem.memory_size()) << ")";
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

		/* Try to divide evenly over 2 dimensions. */
		size_t sqrt_workgroup_size = max((size_t)sqrt((double)workgroup_size), 1);
		size_t local_size[2] = {sqrt_workgroup_size, sqrt_workgroup_size};

		/* Some implementations have max size 1 on 2nd dimension. */
		if(local_size[1] > max_work_items[1]) {
			local_size[0] = workgroup_size/max_work_items[1];
			local_size[1] = max_work_items[1];
		}

		size_t global_size[2] = {global_size_round_up(local_size[0], w),
		                         global_size_round_up(local_size[1], h)};

		/* Vertical size of 1 is coming from bake/shade kernels where we should
		 * not round anything up because otherwise we'll either be doing too
		 * much work per pixel (if we don't check global ID on Y axis) or will
		 * be checking for global ID to always have Y of 0.
		 */
		if(h == 1) {
			global_size[h] = 1;
		}

		/* run kernel */
		opencl_assert(clEnqueueNDRangeKernel(cqCommandQueue, kernel, 2, NULL, global_size, NULL, 0, NULL, NULL));
		opencl_assert(clFlush(cqCommandQueue));
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


		cl_kernel ckFilmConvertKernel = (rgba_byte)? ckFilmConvertByteKernel: ckFilmConvertHalfFloatKernel;

		cl_uint start_arg_index =
			kernel_set_args(ckFilmConvertKernel,
			                0,
			                d_data,
			                d_rgba,
			                d_buffer);

#define KERNEL_TEX(type, ttype, name) \
	set_kernel_arg_mem(ckFilmConvertKernel, &start_arg_index, #name);
#include "kernel_textures.h"
#undef KERNEL_TEX

		start_arg_index += kernel_set_args(ckFilmConvertKernel,
		                                   start_arg_index,
		                                   d_sample_scale,
		                                   d_x,
		                                   d_y,
		                                   d_w,
		                                   d_h,
		                                   d_offset,
		                                   d_stride);

		enqueue_kernel(ckFilmConvertKernel, d_w, d_h);
	}

	void shader(DeviceTask& task)
	{
		/* cast arguments to cl types */
		cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
		cl_mem d_input = CL_MEM_PTR(task.shader_input);
		cl_mem d_output = CL_MEM_PTR(task.shader_output);
		cl_mem d_output_luma = CL_MEM_PTR(task.shader_output_luma);
		cl_int d_shader_eval_type = task.shader_eval_type;
		cl_int d_shader_filter = task.shader_filter;
		cl_int d_shader_x = task.shader_x;
		cl_int d_shader_w = task.shader_w;
		cl_int d_offset = task.offset;

		cl_kernel kernel;

		if(task.shader_eval_type >= SHADER_EVAL_BAKE)
			kernel = ckBakeKernel;
		else
			kernel = ckShaderKernel;

		cl_uint start_arg_index =
			kernel_set_args(kernel,
			                0,
			                d_data,
			                d_input,
			                d_output);

		if(task.shader_eval_type < SHADER_EVAL_BAKE) {
			start_arg_index += kernel_set_args(kernel,
			                                   start_arg_index,
			                                   d_output_luma);
		}

#define KERNEL_TEX(type, ttype, name) \
		set_kernel_arg_mem(kernel, &start_arg_index, #name);
#include "kernel_textures.h"
#undef KERNEL_TEX

		start_arg_index += kernel_set_args(kernel,
		                                   start_arg_index,
		                                   d_shader_eval_type);
		if(task.shader_eval_type >= SHADER_EVAL_BAKE) {
			start_arg_index += kernel_set_args(kernel,
			                                   start_arg_index,
			                                   d_shader_filter);
		}
		start_arg_index += kernel_set_args(kernel,
		                                   start_arg_index,
		                                   d_shader_x,
		                                   d_shader_w,
		                                   d_offset);

		for(int sample = 0; sample < task.num_samples; sample++) {

			if(task.get_cancel())
				break;

			kernel_set_args(kernel, start_arg_index, sample);

			enqueue_kernel(kernel, task.shader_w, 1);

			clFinish(cqCommandQueue);

			task.update_progress(NULL);
		}
	}

	class OpenCLDeviceTask : public DeviceTask {
	public:
		OpenCLDeviceTask(OpenCLDeviceBase *device, DeviceTask& task)
		: DeviceTask(task)
		{
			run = function_bind(&OpenCLDeviceBase::thread_run,
			                    device,
			                    this);
		}
	};

	int get_split_task_count(DeviceTask& /*task*/)
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

	virtual void thread_run(DeviceTask * /*task*/) = 0;

protected:
	string kernel_build_options(const string *debug_src = NULL)
	{
		string build_options = "-cl-fast-relaxed-math ";

		if(platform_name == "NVIDIA CUDA") {
			build_options += "-D__KERNEL_OPENCL_NVIDIA__ "
			                 "-cl-nv-maxrregcount=32 "
			                 "-cl-nv-verbose ";

			uint compute_capability_major, compute_capability_minor;
			clGetDeviceInfo(cdDevice, CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV,
			                sizeof(cl_uint), &compute_capability_major, NULL);
			clGetDeviceInfo(cdDevice, CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV,
			                sizeof(cl_uint), &compute_capability_minor, NULL);

			build_options += string_printf("-D__COMPUTE_CAPABILITY__=%u ",
			                               compute_capability_major * 100 +
			                               compute_capability_minor * 10);
		}

		else if(platform_name == "Apple")
			build_options += "-D__KERNEL_OPENCL_APPLE__ ";

		else if(platform_name == "AMD Accelerated Parallel Processing")
			build_options += "-D__KERNEL_OPENCL_AMD__ ";

		else if(platform_name == "Intel(R) OpenCL") {
			build_options += "-D__KERNEL_OPENCL_INTEL_CPU__ ";

			/* Options for gdb source level kernel debugging.
			 * this segfaults on linux currently.
			 */
			if(opencl_kernel_use_debug() && debug_src)
				build_options += "-g -s \"" + *debug_src + "\" ";
		}

		if(opencl_kernel_use_debug())
			build_options += "-D__KERNEL_OPENCL_DEBUG__ ";

#ifdef WITH_CYCLES_DEBUG
		build_options += "-D__KERNEL_DEBUG__ ";
#endif

		return build_options;
	}

	class ArgumentWrapper {
	public:
		ArgumentWrapper() : size(0), pointer(NULL) {}
		template <typename T>
		ArgumentWrapper(T& argument) : size(sizeof(argument)),
		                               pointer(&argument) { }
		ArgumentWrapper(int argument) : size(sizeof(int)),
		                                int_value(argument),
		                                pointer(&int_value) { }
		ArgumentWrapper(float argument) : size(sizeof(float)),
		                                  float_value(argument),
		                                  pointer(&float_value) { }
		size_t size;
		int int_value;
		float float_value;
		void *pointer;
	};

	/* TODO(sergey): In the future we can use variadic templates, once
	 * C++0x is allowed. Should allow to clean this up a bit.
	 */
	int kernel_set_args(cl_kernel kernel,
	                    int start_argument_index,
	                    const ArgumentWrapper& arg1 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg2 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg3 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg4 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg5 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg6 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg7 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg8 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg9 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg10 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg11 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg12 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg13 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg14 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg15 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg16 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg17 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg18 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg19 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg20 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg21 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg22 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg23 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg24 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg25 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg26 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg27 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg28 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg29 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg30 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg31 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg32 = ArgumentWrapper(),
	                    const ArgumentWrapper& arg33 = ArgumentWrapper())
	{
		int current_arg_index = 0;
#define FAKE_VARARG_HANDLE_ARG(arg) \
		do { \
			if(arg.pointer != NULL) { \
				opencl_assert(clSetKernelArg( \
					kernel, \
					start_argument_index + current_arg_index, \
					arg.size, arg.pointer)); \
				++current_arg_index; \
			} \
			else { \
				return current_arg_index; \
			} \
		} while(false)
		FAKE_VARARG_HANDLE_ARG(arg1);
		FAKE_VARARG_HANDLE_ARG(arg2);
		FAKE_VARARG_HANDLE_ARG(arg3);
		FAKE_VARARG_HANDLE_ARG(arg4);
		FAKE_VARARG_HANDLE_ARG(arg5);
		FAKE_VARARG_HANDLE_ARG(arg6);
		FAKE_VARARG_HANDLE_ARG(arg7);
		FAKE_VARARG_HANDLE_ARG(arg8);
		FAKE_VARARG_HANDLE_ARG(arg9);
		FAKE_VARARG_HANDLE_ARG(arg10);
		FAKE_VARARG_HANDLE_ARG(arg11);
		FAKE_VARARG_HANDLE_ARG(arg12);
		FAKE_VARARG_HANDLE_ARG(arg13);
		FAKE_VARARG_HANDLE_ARG(arg14);
		FAKE_VARARG_HANDLE_ARG(arg15);
		FAKE_VARARG_HANDLE_ARG(arg16);
		FAKE_VARARG_HANDLE_ARG(arg17);
		FAKE_VARARG_HANDLE_ARG(arg18);
		FAKE_VARARG_HANDLE_ARG(arg19);
		FAKE_VARARG_HANDLE_ARG(arg20);
		FAKE_VARARG_HANDLE_ARG(arg21);
		FAKE_VARARG_HANDLE_ARG(arg22);
		FAKE_VARARG_HANDLE_ARG(arg23);
		FAKE_VARARG_HANDLE_ARG(arg24);
		FAKE_VARARG_HANDLE_ARG(arg25);
		FAKE_VARARG_HANDLE_ARG(arg26);
		FAKE_VARARG_HANDLE_ARG(arg27);
		FAKE_VARARG_HANDLE_ARG(arg28);
		FAKE_VARARG_HANDLE_ARG(arg29);
		FAKE_VARARG_HANDLE_ARG(arg30);
		FAKE_VARARG_HANDLE_ARG(arg31);
		FAKE_VARARG_HANDLE_ARG(arg32);
		FAKE_VARARG_HANDLE_ARG(arg33);
#undef FAKE_VARARG_HANDLE_ARG
		return current_arg_index;
	}

	inline void release_kernel_safe(cl_kernel kernel)
	{
		if(kernel) {
			clReleaseKernel(kernel);
		}
	}

	inline void release_mem_object_safe(cl_mem mem)
	{
		if(mem != NULL) {
			clReleaseMemObject(mem);
		}
	}

	inline void release_program_safe(cl_program program)
	{
		if(program) {
			clReleaseProgram(program);
		}
	}

	/* ** Those guys are for workign around some compiler-specific bugs ** */

	virtual cl_program load_cached_kernel(
	        const DeviceRequestedFeatures& /*requested_features*/,
	        OpenCLCache::ProgramName program_name,
	        thread_scoped_lock& cache_locker)
	{
		return OpenCLCache::get_program(cpPlatform,
		                                cdDevice,
		                                program_name,
		                                cache_locker);
	}

	virtual void store_cached_kernel(cl_platform_id platform,
	                                 cl_device_id device,
	                                 cl_program program,
	                                 OpenCLCache::ProgramName program_name,
	                                 thread_scoped_lock& cache_locker)
	{
		OpenCLCache::store_program(platform,
		                           device,
		                           program,
		                           program_name,
		                           cache_locker);
	}

	virtual string build_options_for_base_program(
	        const DeviceRequestedFeatures& /*requested_features*/)
	{
		/* TODO(sergey): By default we compile all features, meaning
		 * mega kernel is not getting feature-based optimizations.
		 *
		 * Ideally we need always compile kernel with as less features
		 * enabled as possible to keep performance at it's max.
		 */
		return "";
	}
};

class OpenCLDeviceMegaKernel : public OpenCLDeviceBase
{
public:
	cl_kernel ckPathTraceKernel;
	cl_program path_trace_program;

	OpenCLDeviceMegaKernel(DeviceInfo& info, Stats &stats, bool background_)
	: OpenCLDeviceBase(info, stats, background_)
	{
		ckPathTraceKernel = NULL;
		path_trace_program = NULL;
	}

	bool load_kernels(const DeviceRequestedFeatures& requested_features)
	{
		/* Get Shader, bake and film convert kernels.
		 * It'll also do verification of OpenCL actually initialized.
		 */
		if(!OpenCLDeviceBase::load_kernels(requested_features)) {
			return false;
		}

		/* Try to use cached kernel. */
		thread_scoped_lock cache_locker;
		path_trace_program = OpenCLCache::get_program(cpPlatform,
		                                              cdDevice,
		                                              OpenCLCache::OCL_DEV_MEGAKERNEL_PROGRAM,
		                                              cache_locker);

		if(!path_trace_program) {
			/* Verify we have right opencl version. */
			if(!opencl_version_check())
				return false;

			/* Calculate md5 hash to detect changes. */
			string kernel_path = path_get("kernel");
			string kernel_md5 = path_files_md5_hash(kernel_path);
			string custom_kernel_build_options = "-D__COMPILE_ONLY_MEGAKERNEL__ ";
			string device_md5 = device_md5_hash(custom_kernel_build_options);

			/* Path to cached binary. */
			string clbin = string_printf("cycles_kernel_%s_%s.clbin",
			                             device_md5.c_str(),
			                             kernel_md5.c_str());
			clbin = path_user_get(path_join("cache", clbin));

			/* Path to preprocessed source for debugging. */
			string clsrc, *debug_src = NULL;
			if(opencl_kernel_use_debug()) {
				clsrc = string_printf("cycles_kernel_%s_%s.cl",
				                      device_md5.c_str(),
				                      kernel_md5.c_str());
				clsrc = path_user_get(path_join("cache", clsrc));
				debug_src = &clsrc;
			}

			/* If exists already, try use it. */
			if(path_exists(clbin) && load_binary(kernel_path,
			                                     clbin,
			                                     custom_kernel_build_options,
			                                     &path_trace_program,
			                                     debug_src))
			{
				/* Kernel loaded from binary, nothing to do. */
			}
			else {
				string init_kernel_source = "#include \"kernels/opencl/kernel.cl\" // " +
				                            kernel_md5 + "\n";
				/* If does not exist or loading binary failed, compile kernel. */
				if(!compile_kernel("mega_kernel",
				                   kernel_path,
				                   init_kernel_source,
				                   custom_kernel_build_options,
				                   &path_trace_program,
				                   debug_src))
				{
					return false;
				}
				/* Save binary for reuse. */
				if(!save_binary(&path_trace_program, clbin)) {
					return false;
				}
			}
			/* Cache the program. */
			OpenCLCache::store_program(cpPlatform,
			                           cdDevice,
			                           path_trace_program,
			                           OpenCLCache::OCL_DEV_MEGAKERNEL_PROGRAM,
			                           cache_locker);
		}

		/* Find kernels. */
		ckPathTraceKernel = clCreateKernel(path_trace_program,
		                                   "kernel_ocl_path_trace",
		                                   &ciErr);
		if(opencl_error(ciErr))
			return false;
		return true;
	}

	~OpenCLDeviceMegaKernel()
	{
		task_pool.stop();
		release_kernel_safe(ckPathTraceKernel);
		release_program_safe(path_trace_program);
	}

	void path_trace(RenderTile& rtile, int sample)
	{
		/* Cast arguments to cl types. */
		cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
		cl_mem d_buffer = CL_MEM_PTR(rtile.buffer);
		cl_mem d_rng_state = CL_MEM_PTR(rtile.rng_state);
		cl_int d_x = rtile.x;
		cl_int d_y = rtile.y;
		cl_int d_w = rtile.w;
		cl_int d_h = rtile.h;
		cl_int d_offset = rtile.offset;
		cl_int d_stride = rtile.stride;

		/* Sample arguments. */
		cl_int d_sample = sample;

		cl_uint start_arg_index =
			kernel_set_args(ckPathTraceKernel,
			                0,
			                d_data,
			                d_buffer,
			                d_rng_state);

#define KERNEL_TEX(type, ttype, name) \
		set_kernel_arg_mem(ckPathTraceKernel, &start_arg_index, #name);
#include "kernel_textures.h"
#undef KERNEL_TEX

		start_arg_index += kernel_set_args(ckPathTraceKernel,
		                                   start_arg_index,
		                                   d_sample,
		                                   d_x,
		                                   d_y,
		                                   d_w,
		                                   d_h,
		                                   d_offset,
		                                   d_stride);

		enqueue_kernel(ckPathTraceKernel, d_w, d_h);
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
			/* Keep rendering tiles until done. */
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

				/* Complete kernel execution before release tile */
				/* This helps in multi-device render;
				 * The device that reaches the critical-section function
				 * release_tile waits (stalling other devices from entering
				 * release_tile) for all kernels to complete. If device1 (a
				 * slow-render device) reaches release_tile first then it would
				 * stall device2 (a fast-render device) from proceeding to render
				 * next tile.
				 */
				clFinish(cqCommandQueue);

				task->release_tile(tile);
			}
		}
	}
};

/* TODO(sergey): This is to keep tile split on OpenCL level working
 * for now, since without this view-port render does not work as it
 * should.
 *
 * Ideally it'll be done on the higher level, but we need to get ready
 * for merge rather soon, so let's keep split logic private here in
 * the file.
 */
class SplitRenderTile : public RenderTile {
public:
	SplitRenderTile()
		: RenderTile(),
		  buffer_offset_x(0),
		  buffer_offset_y(0),
		  rng_state_offset_x(0),
		  rng_state_offset_y(0),
		  buffer_rng_state_stride(0) {}

	explicit SplitRenderTile(RenderTile& tile)
		: RenderTile(),
		  buffer_offset_x(0),
		  buffer_offset_y(0),
		  rng_state_offset_x(0),
		  rng_state_offset_y(0),
		  buffer_rng_state_stride(0)
	{
		x = tile.x;
		y = tile.y;
		w = tile.w;
		h = tile.h;
		start_sample = tile.start_sample;
		num_samples = tile.num_samples;
		sample = tile.sample;
		resolution = tile.resolution;
		offset = tile.offset;
		stride = tile.stride;
		buffer = tile.buffer;
		rng_state = tile.rng_state;
		buffers = tile.buffers;
	}

	/* Split kernel is device global memory constrained;
	 * hence split kernel cant render big tile size's in
	 * one go. If the user sets a big tile size (big tile size
	 * is a term relative to the available device global memory),
	 * we split the tile further and then call path_trace on
	 * each of those split tiles. The following variables declared,
	 * assist in achieving that purpose
	 */
	int buffer_offset_x;
	int buffer_offset_y;
	int rng_state_offset_x;
	int rng_state_offset_y;
	int buffer_rng_state_stride;
};

/* OpenCLDeviceSplitKernel's declaration/definition. */
class OpenCLDeviceSplitKernel : public OpenCLDeviceBase
{
public:
	/* Kernel declaration. */
	cl_kernel ckPathTraceKernel_data_init;
	cl_kernel ckPathTraceKernel_scene_intersect;
	cl_kernel ckPathTraceKernel_lamp_emission;
	cl_kernel ckPathTraceKernel_queue_enqueue;
	cl_kernel ckPathTraceKernel_background_buffer_update;
	cl_kernel ckPathTraceKernel_shader_eval;
	cl_kernel ckPathTraceKernel_holdout_emission_blurring_pathtermination_ao;
	cl_kernel ckPathTraceKernel_direct_lighting;
	cl_kernel ckPathTraceKernel_shadow_blocked;
	cl_kernel ckPathTraceKernel_next_iteration_setup;
	cl_kernel ckPathTraceKernel_sum_all_radiance;

	/* cl_program declaration. */
	cl_program data_init_program;
	cl_program scene_intersect_program;
	cl_program lamp_emission_program;
	cl_program queue_enqueue_program;
	cl_program background_buffer_update_program;
	cl_program shader_eval_program;
	cl_program holdout_emission_blurring_pathtermination_ao_program;
	cl_program direct_lighting_program;
	cl_program shadow_blocked_program;
	cl_program next_iteration_setup_program;
	cl_program sum_all_radiance_program;

	/* Global memory variables [porting]; These memory is used for
	 * co-operation between different kernels; Data written by one
	 * kernel will be available to another kernel via this global
	 * memory.
	 */
	cl_mem rng_coop;
	cl_mem throughput_coop;
	cl_mem L_transparent_coop;
	cl_mem PathRadiance_coop;
	cl_mem Ray_coop;
	cl_mem PathState_coop;
	cl_mem Intersection_coop;
	cl_mem kgbuffer;  /* KernelGlobals buffer. */

	/* Global buffers for ShaderData. */
	cl_mem sd;             /* ShaderData used in the main path-iteration loop. */
	cl_mem sd_DL_shadow;   /* ShaderData used in Direct Lighting and
	                        * shadow_blocked kernel.
	                        */

	/* Global memory required for shadow blocked and accum_radiance. */
	cl_mem BSDFEval_coop;
	cl_mem ISLamp_coop;
	cl_mem LightRay_coop;
	cl_mem AOAlpha_coop;
	cl_mem AOBSDF_coop;
	cl_mem AOLightRay_coop;
	cl_mem Intersection_coop_shadow;

#ifdef WITH_CYCLES_DEBUG
	/* DebugData memory */
	cl_mem debugdata_coop;
#endif

	/* Global state array that tracks ray state. */
	cl_mem ray_state;

	/* Per sample buffers. */
	cl_mem per_sample_output_buffers;

	/* Denotes which sample each ray is being processed for. */
	cl_mem work_array;

	/* Queue */
	cl_mem Queue_data;  /* Array of size queuesize * num_queues * sizeof(int). */
	cl_mem Queue_index; /* Array of size num_queues * sizeof(int);
	                     * Tracks the size of each queue.
	                     */

	/* Flag to make sceneintersect and lampemission kernel use queues. */
	cl_mem use_queues_flag;

	/* Amount of memory in output buffer associated with one pixel/thread. */
	size_t per_thread_output_buffer_size;

	/* Total allocatable available device memory. */
	size_t total_allocatable_memory;

	/* host version of ray_state; Used in checking host path-iteration
	 * termination.
	 */
	char *hostRayStateArray;

	/* Number of path-iterations to be done in one shot. */
	unsigned int PathIteration_times;

#ifdef __WORK_STEALING__
	/* Work pool with respect to each work group. */
	cl_mem work_pool_wgs;

	/* Denotes the maximum work groups possible w.r.t. current tile size. */
	unsigned int max_work_groups;
#endif

	/* clos_max value for which the kernels have been loaded currently. */
	int current_max_closure;

	/* Marked True in constructor and marked false at the end of path_trace(). */
	bool first_tile;

	OpenCLDeviceSplitKernel(DeviceInfo& info, Stats &stats, bool background_)
	: OpenCLDeviceBase(info, stats, background_)
	{
		background = background_;

		/* Initialize kernels. */
		ckPathTraceKernel_data_init = NULL;
		ckPathTraceKernel_scene_intersect = NULL;
		ckPathTraceKernel_lamp_emission = NULL;
		ckPathTraceKernel_background_buffer_update = NULL;
		ckPathTraceKernel_shader_eval = NULL;
		ckPathTraceKernel_holdout_emission_blurring_pathtermination_ao = NULL;
		ckPathTraceKernel_direct_lighting = NULL;
		ckPathTraceKernel_shadow_blocked = NULL;
		ckPathTraceKernel_next_iteration_setup = NULL;
		ckPathTraceKernel_sum_all_radiance = NULL;
		ckPathTraceKernel_queue_enqueue = NULL;

		/* Initialize program. */
		data_init_program = NULL;
		scene_intersect_program = NULL;
		lamp_emission_program = NULL;
		queue_enqueue_program = NULL;
		background_buffer_update_program = NULL;
		shader_eval_program = NULL;
		holdout_emission_blurring_pathtermination_ao_program = NULL;
		direct_lighting_program = NULL;
		shadow_blocked_program = NULL;
		next_iteration_setup_program = NULL;
		sum_all_radiance_program = NULL;

		/* Initialize cl_mem variables. */
		kgbuffer = NULL;
		sd = NULL;
		sd_DL_shadow = NULL;

		rng_coop = NULL;
		throughput_coop = NULL;
		L_transparent_coop = NULL;
		PathRadiance_coop = NULL;
		Ray_coop = NULL;
		PathState_coop = NULL;
		Intersection_coop = NULL;
		ray_state = NULL;

		AOAlpha_coop = NULL;
		AOBSDF_coop = NULL;
		AOLightRay_coop = NULL;
		BSDFEval_coop = NULL;
		ISLamp_coop = NULL;
		LightRay_coop = NULL;
		Intersection_coop_shadow = NULL;

#ifdef WITH_CYCLES_DEBUG
		debugdata_coop = NULL;
#endif

		work_array = NULL;

		/* Queue. */
		Queue_data = NULL;
		Queue_index = NULL;
		use_queues_flag = NULL;

		per_sample_output_buffers = NULL;

		per_thread_output_buffer_size = 0;
		hostRayStateArray = NULL;
		PathIteration_times = PATH_ITER_INC_FACTOR;
#ifdef __WORK_STEALING__
		work_pool_wgs = NULL;
		max_work_groups = 0;
#endif
		current_max_closure = -1;
		first_tile = true;

		/* Get device's maximum memory that can be allocated. */
		ciErr = clGetDeviceInfo(cdDevice,
		                        CL_DEVICE_MAX_MEM_ALLOC_SIZE,
		                        sizeof(size_t),
		                        &total_allocatable_memory,
		                        NULL);
		assert(ciErr == CL_SUCCESS);
		if(platform_name == "AMD Accelerated Parallel Processing") {
			/* This value is tweak-able; AMD platform does not seem to
			 * give maximum performance when all of CL_DEVICE_MAX_MEM_ALLOC_SIZE
			 * is considered for further computation.
			 */
			total_allocatable_memory /= 2;
		}
	}

	/* TODO(sergey): Seems really close to load_kernel(),
	 * could it be de-duplicated?
	 */
	bool load_split_kernel(const string& kernel_name,
	                       const string& kernel_path,
	                       const string& kernel_init_source,
	                       const string& clbin,
	                       const string& custom_kernel_build_options,
	                       cl_program *program,
	                       const string *debug_src = NULL)
	{
		if(!opencl_version_check()) {
			return false;
		}

		string cache_clbin = path_user_get(path_join("cache", clbin));

		/* If exists already, try use it. */
		if(path_exists(cache_clbin) && load_binary(kernel_path,
		                                           cache_clbin,
		                                           custom_kernel_build_options,
		                                           program,
		                                           debug_src))
		{
			/* Kernel loaded from binary. */
		}
		else {
			/* If does not exist or loading binary failed, compile kernel. */
			if(!compile_kernel(kernel_name,
			                   kernel_path,
			                   kernel_init_source,
			                   custom_kernel_build_options,
			                   program,
			                   debug_src))
			{
				return false;
			}
			/* Save binary for reuse. */
			if(!save_binary(program, cache_clbin)) {
				return false;
			}
		}
		return true;
	}

	/* Split kernel utility functions. */
	size_t get_tex_size(const char *tex_name)
	{
		cl_mem ptr;
		size_t ret_size = 0;
		MemMap::iterator i = mem_map.find(tex_name);
		if(i != mem_map.end()) {
			ptr = CL_MEM_PTR(i->second);
			ciErr = clGetMemObjectInfo(ptr,
			                           CL_MEM_SIZE,
			                           sizeof(ret_size),
			                           &ret_size,
			                           NULL);
			assert(ciErr == CL_SUCCESS);
		}
		return ret_size;
	}

	size_t get_shader_data_size(size_t max_closure)
	{
		/* ShaderData size with variable size ShaderClosure array */
		return sizeof(ShaderData) - (sizeof(ShaderClosure) * (MAX_CLOSURE - max_closure));
	}

	/* Returns size of KernelGlobals structure associated with OpenCL. */
	size_t get_KernelGlobals_size()
	{
		/* Copy dummy KernelGlobals related to OpenCL from kernel_globals.h to
		 * fetch its size.
		 */
		typedef struct KernelGlobals {
			ccl_constant KernelData *data;
#define KERNEL_TEX(type, ttype, name) \
	ccl_global type *name;
#include "kernel_textures.h"
#undef KERNEL_TEX
			void *sd_input;
			void *isect_shadow;
		} KernelGlobals;

		return sizeof(KernelGlobals);
	}

	bool load_kernels(const DeviceRequestedFeatures& requested_features)
	{
		/* Get Shader, bake and film_convert kernels.
		 * It'll also do verification of OpenCL actually initialized.
		 */
		if(!OpenCLDeviceBase::load_kernels(requested_features)) {
			return false;
		}

		string kernel_path = path_get("kernel");
		string kernel_md5 = path_files_md5_hash(kernel_path);
		string device_md5;
		string kernel_init_source;
		string clbin;
		string clsrc, *debug_src = NULL;

		string build_options = "-D__SPLIT_KERNEL__ ";
#ifdef __WORK_STEALING__
		build_options += "-D__WORK_STEALING__ ";
#endif
		build_options += requested_features.get_build_options();

		/* Set compute device build option. */
		cl_device_type device_type;
		ciErr = clGetDeviceInfo(cdDevice,
		                        CL_DEVICE_TYPE,
		                        sizeof(cl_device_type),
		                        &device_type,
		                        NULL);
		assert(ciErr == CL_SUCCESS);
		if(device_type == CL_DEVICE_TYPE_GPU) {
			build_options += " -D__COMPUTE_DEVICE_GPU__";
		}

#define GLUE(a, b) a ## b
#define LOAD_KERNEL(name) \
	do { \
		kernel_init_source = "#include \"kernels/opencl/kernel_" #name ".cl\" // " + \
		                     kernel_md5 + "\n"; \
		device_md5 = device_md5_hash(build_options); \
		clbin = string_printf("cycles_kernel_%s_%s_" #name ".clbin", \
		                      device_md5.c_str(), kernel_md5.c_str()); \
		if(opencl_kernel_use_debug()) { \
			clsrc = string_printf("cycles_kernel_%s_%s_" #name ".cl", \
			                      device_md5.c_str(), kernel_md5.c_str()); \
			clsrc = path_user_get(path_join("cache", clsrc)); \
			debug_src = &clsrc; \
		} \
		if(!load_split_kernel(#name, \
		                      kernel_path, \
		                      kernel_init_source, \
		                      clbin, \
		                      build_options, \
		                      &GLUE(name, _program), \
		                      debug_src)) \
		{ \
			fprintf(stderr, "Faled to compile %s\n", #name); \
			return false; \
		} \
	} while(false)

		LOAD_KERNEL(data_init);
		LOAD_KERNEL(scene_intersect);
		LOAD_KERNEL(lamp_emission);
		LOAD_KERNEL(queue_enqueue);
		LOAD_KERNEL(background_buffer_update);
		LOAD_KERNEL(shader_eval);
		LOAD_KERNEL(holdout_emission_blurring_pathtermination_ao);
		LOAD_KERNEL(direct_lighting);
		LOAD_KERNEL(shadow_blocked);
		LOAD_KERNEL(next_iteration_setup);
		LOAD_KERNEL(sum_all_radiance);

#undef LOAD_KERNEL

#define FIND_KERNEL(name) \
	do { \
		GLUE(ckPathTraceKernel_, name) = \
			clCreateKernel(GLUE(name, _program), \
			               "kernel_ocl_path_trace_"  #name, &ciErr); \
		if(opencl_error(ciErr)) { \
			fprintf(stderr,"Missing kernel kernel_ocl_path_trace_%s\n", #name); \
			return false; \
		} \
	} while(false)

		FIND_KERNEL(data_init);
		FIND_KERNEL(scene_intersect);
		FIND_KERNEL(lamp_emission);
		FIND_KERNEL(queue_enqueue);
		FIND_KERNEL(background_buffer_update);
		FIND_KERNEL(shader_eval);
		FIND_KERNEL(holdout_emission_blurring_pathtermination_ao);
		FIND_KERNEL(direct_lighting);
		FIND_KERNEL(shadow_blocked);
		FIND_KERNEL(next_iteration_setup);
		FIND_KERNEL(sum_all_radiance);
#undef FIND_KERNEL
#undef GLUE

		current_max_closure = requested_features.max_closure;

		return true;
	}

	~OpenCLDeviceSplitKernel()
	{
		task_pool.stop();

		/* Release kernels */
		release_kernel_safe(ckPathTraceKernel_data_init);
		release_kernel_safe(ckPathTraceKernel_scene_intersect);
		release_kernel_safe(ckPathTraceKernel_lamp_emission);
		release_kernel_safe(ckPathTraceKernel_queue_enqueue);
		release_kernel_safe(ckPathTraceKernel_background_buffer_update);
		release_kernel_safe(ckPathTraceKernel_shader_eval);
		release_kernel_safe(ckPathTraceKernel_holdout_emission_blurring_pathtermination_ao);
		release_kernel_safe(ckPathTraceKernel_direct_lighting);
		release_kernel_safe(ckPathTraceKernel_shadow_blocked);
		release_kernel_safe(ckPathTraceKernel_next_iteration_setup);
		release_kernel_safe(ckPathTraceKernel_sum_all_radiance);

		/* Release global memory */
		release_mem_object_safe(rng_coop);
		release_mem_object_safe(throughput_coop);
		release_mem_object_safe(L_transparent_coop);
		release_mem_object_safe(PathRadiance_coop);
		release_mem_object_safe(Ray_coop);
		release_mem_object_safe(PathState_coop);
		release_mem_object_safe(Intersection_coop);
		release_mem_object_safe(kgbuffer);
		release_mem_object_safe(sd);
		release_mem_object_safe(sd_DL_shadow);
		release_mem_object_safe(ray_state);
		release_mem_object_safe(AOAlpha_coop);
		release_mem_object_safe(AOBSDF_coop);
		release_mem_object_safe(AOLightRay_coop);
		release_mem_object_safe(BSDFEval_coop);
		release_mem_object_safe(ISLamp_coop);
		release_mem_object_safe(LightRay_coop);
		release_mem_object_safe(Intersection_coop_shadow);
#ifdef WITH_CYCLES_DEBUG
		release_mem_object_safe(debugdata_coop);
#endif
		release_mem_object_safe(use_queues_flag);
		release_mem_object_safe(Queue_data);
		release_mem_object_safe(Queue_index);
		release_mem_object_safe(work_array);
#ifdef __WORK_STEALING__
		release_mem_object_safe(work_pool_wgs);
#endif
		release_mem_object_safe(per_sample_output_buffers);

		/* Release programs */
		release_program_safe(data_init_program);
		release_program_safe(scene_intersect_program);
		release_program_safe(lamp_emission_program);
		release_program_safe(queue_enqueue_program);
		release_program_safe(background_buffer_update_program);
		release_program_safe(shader_eval_program);
		release_program_safe(holdout_emission_blurring_pathtermination_ao_program);
		release_program_safe(direct_lighting_program);
		release_program_safe(shadow_blocked_program);
		release_program_safe(next_iteration_setup_program);
		release_program_safe(sum_all_radiance_program);

		if(hostRayStateArray != NULL) {
			free(hostRayStateArray);
		}
	}

	void path_trace(DeviceTask *task,
	                SplitRenderTile& rtile,
	                int2 max_render_feasible_tile_size)
	{
		/* cast arguments to cl types */
		cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
		cl_mem d_buffer = CL_MEM_PTR(rtile.buffer);
		cl_mem d_rng_state = CL_MEM_PTR(rtile.rng_state);
		cl_int d_x = rtile.x;
		cl_int d_y = rtile.y;
		cl_int d_w = rtile.w;
		cl_int d_h = rtile.h;
		cl_int d_offset = rtile.offset;
		cl_int d_stride = rtile.stride;

		/* Make sure that set render feasible tile size is a multiple of local
		 * work size dimensions.
		 */
		assert(max_render_feasible_tile_size.x % SPLIT_KERNEL_LOCAL_SIZE_X == 0);
		assert(max_render_feasible_tile_size.y % SPLIT_KERNEL_LOCAL_SIZE_Y == 0);

		size_t global_size[2];
		size_t local_size[2] = {SPLIT_KERNEL_LOCAL_SIZE_X,
		                        SPLIT_KERNEL_LOCAL_SIZE_Y};

		/* Set the range of samples to be processed for every ray in
		 * path-regeneration logic.
		 */
		cl_int start_sample = rtile.start_sample;
		cl_int end_sample = rtile.start_sample + rtile.num_samples;
		cl_int num_samples = rtile.num_samples;

#ifdef __WORK_STEALING__
		global_size[0] = (((d_w - 1) / local_size[0]) + 1) * local_size[0];
		global_size[1] = (((d_h - 1) / local_size[1]) + 1) * local_size[1];
		unsigned int num_parallel_samples = 1;
#else
		global_size[1] = (((d_h - 1) / local_size[1]) + 1) * local_size[1];
		unsigned int num_threads = max_render_feasible_tile_size.x *
		                           max_render_feasible_tile_size.y;
		unsigned int num_tile_columns_possible = num_threads / global_size[1];
		/* Estimate number of parallel samples that can be
		 * processed in parallel.
		 */
		unsigned int num_parallel_samples = min(num_tile_columns_possible / d_w,
		                                        rtile.num_samples);
		/* Wavefront size in AMD is 64.
		 * TODO(sergey): What about other platforms?
		 */
		if(num_parallel_samples >= 64) {
			/* TODO(sergey): Could use generic round-up here. */
			num_parallel_samples = (num_parallel_samples / 64) * 64;
		}
		assert(num_parallel_samples != 0);

		global_size[0] = d_w * num_parallel_samples;
#endif  /* __WORK_STEALING__ */

		assert(global_size[0] * global_size[1] <=
		       max_render_feasible_tile_size.x * max_render_feasible_tile_size.y);

		/* Allocate all required global memory once. */
		if(first_tile) {
			size_t num_global_elements = max_render_feasible_tile_size.x *
			                             max_render_feasible_tile_size.y;
			/* TODO(sergey): This will actually over-allocate if
			 * particular kernel does not support multiclosure.
			 */
			size_t shaderdata_size = get_shader_data_size(current_max_closure);

#ifdef __WORK_STEALING__
			/* Calculate max groups */
			size_t max_global_size[2];
			size_t tile_x = max_render_feasible_tile_size.x;
			size_t tile_y = max_render_feasible_tile_size.y;
			max_global_size[0] = (((tile_x - 1) / local_size[0]) + 1) * local_size[0];
			max_global_size[1] = (((tile_y - 1) / local_size[1]) + 1) * local_size[1];
			max_work_groups = (max_global_size[0] * max_global_size[1]) /
			                  (local_size[0] * local_size[1]);
			/* Allocate work_pool_wgs memory. */
			work_pool_wgs = mem_alloc(max_work_groups * sizeof(unsigned int));
#endif  /* __WORK_STEALING__ */

			/* Allocate queue_index memory only once. */
			Queue_index = mem_alloc(NUM_QUEUES * sizeof(int));
			use_queues_flag = mem_alloc(sizeof(char));
			kgbuffer = mem_alloc(get_KernelGlobals_size());

			/* Create global buffers for ShaderData. */
			sd = mem_alloc(num_global_elements * shaderdata_size);
			sd_DL_shadow = mem_alloc(num_global_elements * 2 * shaderdata_size);

			/* Creation of global memory buffers which are shared among
			 * the kernels.
			 */
			rng_coop = mem_alloc(num_global_elements * sizeof(RNG));
			throughput_coop = mem_alloc(num_global_elements * sizeof(float3));
			L_transparent_coop = mem_alloc(num_global_elements * sizeof(float));
			PathRadiance_coop = mem_alloc(num_global_elements * sizeof(PathRadiance));
			Ray_coop = mem_alloc(num_global_elements * sizeof(Ray));
			PathState_coop = mem_alloc(num_global_elements * sizeof(PathState));
			Intersection_coop = mem_alloc(num_global_elements * sizeof(Intersection));
			AOAlpha_coop = mem_alloc(num_global_elements * sizeof(float3));
			AOBSDF_coop = mem_alloc(num_global_elements * sizeof(float3));
			AOLightRay_coop = mem_alloc(num_global_elements * sizeof(Ray));
			BSDFEval_coop = mem_alloc(num_global_elements * sizeof(BsdfEval));
			ISLamp_coop = mem_alloc(num_global_elements * sizeof(int));
			LightRay_coop = mem_alloc(num_global_elements * sizeof(Ray));
			Intersection_coop_shadow = mem_alloc(2 * num_global_elements * sizeof(Intersection));

#ifdef WITH_CYCLES_DEBUG
			debugdata_coop = mem_alloc(num_global_elements * sizeof(DebugData));
#endif

			ray_state = mem_alloc(num_global_elements * sizeof(char));

			hostRayStateArray = (char *)calloc(num_global_elements, sizeof(char));
			assert(hostRayStateArray != NULL && "Can't create hostRayStateArray memory");

			Queue_data = mem_alloc(num_global_elements * (NUM_QUEUES * sizeof(int)+sizeof(int)));
			work_array = mem_alloc(num_global_elements * sizeof(unsigned int));
			per_sample_output_buffers = mem_alloc(num_global_elements *
			                                      per_thread_output_buffer_size);
		}

		cl_int dQueue_size = global_size[0] * global_size[1];

		cl_uint start_arg_index =
			kernel_set_args(ckPathTraceKernel_data_init,
			                0,
			                kgbuffer,
			                sd_DL_shadow,
			                d_data,
			                per_sample_output_buffers,
			                d_rng_state,
			                rng_coop,
			                throughput_coop,
			                L_transparent_coop,
			                PathRadiance_coop,
			                Ray_coop,
			                PathState_coop,
			                Intersection_coop_shadow,
			                ray_state);

/* TODO(sergey): Avoid map lookup here. */
#define KERNEL_TEX(type, ttype, name) \
	set_kernel_arg_mem(ckPathTraceKernel_data_init, &start_arg_index, #name);
#include "kernel_textures.h"
#undef KERNEL_TEX

		start_arg_index +=
			kernel_set_args(ckPathTraceKernel_data_init,
			                start_arg_index,
			                start_sample,
			                d_x,
			                d_y,
			                d_w,
			                d_h,
			                d_offset,
			                d_stride,
			                rtile.rng_state_offset_x,
			                rtile.rng_state_offset_y,
			                rtile.buffer_rng_state_stride,
			                Queue_data,
			                Queue_index,
			                dQueue_size,
			                use_queues_flag,
			                work_array,
#ifdef __WORK_STEALING__
			                work_pool_wgs,
			                num_samples,
#endif
#ifdef WITH_CYCLES_DEBUG
			                debugdata_coop,
#endif
			                num_parallel_samples);

		kernel_set_args(ckPathTraceKernel_scene_intersect,
		                0,
		                kgbuffer,
		                d_data,
		                rng_coop,
		                Ray_coop,
		                PathState_coop,
		                Intersection_coop,
		                ray_state,
		                d_w,
		                d_h,
		                Queue_data,
		                Queue_index,
		                dQueue_size,
		                use_queues_flag,
#ifdef WITH_CYCLES_DEBUG
		                debugdata_coop,
#endif
		                num_parallel_samples);

		kernel_set_args(ckPathTraceKernel_lamp_emission,
		                0,
		                kgbuffer,
		                d_data,
		                throughput_coop,
		                PathRadiance_coop,
		                Ray_coop,
		                PathState_coop,
		                Intersection_coop,
		                ray_state,
		                d_w,
		                d_h,
		                Queue_data,
		                Queue_index,
		                dQueue_size,
		                use_queues_flag,
		                num_parallel_samples);

		kernel_set_args(ckPathTraceKernel_queue_enqueue,
		                0,
		                Queue_data,
		                Queue_index,
		                ray_state,
		                dQueue_size);

		kernel_set_args(ckPathTraceKernel_background_buffer_update,
		                 0,
		                 kgbuffer,
		                 d_data,
		                 per_sample_output_buffers,
		                 d_rng_state,
		                 rng_coop,
		                 throughput_coop,
		                 PathRadiance_coop,
		                 Ray_coop,
		                 PathState_coop,
		                 L_transparent_coop,
		                 ray_state,
		                 d_w,
		                 d_h,
		                 d_x,
		                 d_y,
		                 d_stride,
		                 rtile.rng_state_offset_x,
		                 rtile.rng_state_offset_y,
		                 rtile.buffer_rng_state_stride,
		                 work_array,
		                 Queue_data,
		                 Queue_index,
		                 dQueue_size,
		                 end_sample,
		                 start_sample,
#ifdef __WORK_STEALING__
		                 work_pool_wgs,
		                 num_samples,
#endif
#ifdef WITH_CYCLES_DEBUG
		                 debugdata_coop,
#endif
		                 num_parallel_samples);

		kernel_set_args(ckPathTraceKernel_shader_eval,
		                0,
		                kgbuffer,
		                d_data,
		                sd,
		                rng_coop,
		                Ray_coop,
		                PathState_coop,
		                Intersection_coop,
		                ray_state,
		                Queue_data,
		                Queue_index,
		                dQueue_size);

		kernel_set_args(ckPathTraceKernel_holdout_emission_blurring_pathtermination_ao,
		                0,
		                kgbuffer,
		                d_data,
		                sd,
		                per_sample_output_buffers,
		                rng_coop,
		                throughput_coop,
		                L_transparent_coop,
		                PathRadiance_coop,
		                PathState_coop,
		                Intersection_coop,
		                AOAlpha_coop,
		                AOBSDF_coop,
		                AOLightRay_coop,
		                d_w,
		                d_h,
		                d_x,
		                d_y,
		                d_stride,
		                ray_state,
		                work_array,
		                Queue_data,
		                Queue_index,
		                dQueue_size,
#ifdef __WORK_STEALING__
		                start_sample,
#endif
		                num_parallel_samples);

		kernel_set_args(ckPathTraceKernel_direct_lighting,
		                0,
		                kgbuffer,
		                d_data,
		                sd,
		                rng_coop,
		                PathState_coop,
		                ISLamp_coop,
		                LightRay_coop,
		                BSDFEval_coop,
		                ray_state,
		                Queue_data,
		                Queue_index,
		                dQueue_size);

		kernel_set_args(ckPathTraceKernel_shadow_blocked,
		                0,
		                kgbuffer,
		                d_data,
		                PathState_coop,
		                LightRay_coop,
		                AOLightRay_coop,
		                ray_state,
		                Queue_data,
		                Queue_index,
		                dQueue_size);

		kernel_set_args(ckPathTraceKernel_next_iteration_setup,
		                0,
		                kgbuffer,
		                d_data,
		                sd,
		                rng_coop,
		                throughput_coop,
		                PathRadiance_coop,
		                Ray_coop,
		                PathState_coop,
		                LightRay_coop,
		                ISLamp_coop,
		                BSDFEval_coop,
		                AOLightRay_coop,
		                AOBSDF_coop,
		                AOAlpha_coop,
		                ray_state,
		                Queue_data,
		                Queue_index,
		                dQueue_size,
		                use_queues_flag);

		kernel_set_args(ckPathTraceKernel_sum_all_radiance,
		                0,
		                d_data,
		                d_buffer,
		                per_sample_output_buffers,
		                num_parallel_samples,
		                d_w,
		                d_h,
		                d_stride,
		                rtile.buffer_offset_x,
		                rtile.buffer_offset_y,
		                rtile.buffer_rng_state_stride,
		                start_sample);

		/* Macro for Enqueuing split kernels. */
#define GLUE(a, b) a ## b
#define ENQUEUE_SPLIT_KERNEL(kernelName, globalSize, localSize) \
		{ \
			ciErr = clEnqueueNDRangeKernel(cqCommandQueue, \
			                               GLUE(ckPathTraceKernel_, \
			                                    kernelName), \
			                               2, \
			                               NULL, \
			                               globalSize, \
			                               localSize, \
			                               0, \
			                               NULL, \
			                               NULL); \
			opencl_assert_err(ciErr, "clEnqueueNDRangeKernel"); \
			if(ciErr != CL_SUCCESS) { \
				string message = string_printf("OpenCL error: %s in clEnqueueNDRangeKernel()", \
				                               clewErrorString(ciErr)); \
				opencl_error(message); \
				return; \
			} \
		} (void) 0

		/* Enqueue ckPathTraceKernel_data_init kernel. */
		ENQUEUE_SPLIT_KERNEL(data_init, global_size, local_size);
		bool activeRaysAvailable = true;

		/* Record number of time host intervention has been made */
		unsigned int numHostIntervention = 0;
		unsigned int numNextPathIterTimes = PathIteration_times;
		bool canceled = false;
		while(activeRaysAvailable) {
			/* Twice the global work size of other kernels for
			 * ckPathTraceKernel_shadow_blocked_direct_lighting. */
			size_t global_size_shadow_blocked[2];
			global_size_shadow_blocked[0] = global_size[0] * 2;
			global_size_shadow_blocked[1] = global_size[1];

			/* Do path-iteration in host [Enqueue Path-iteration kernels. */
			for(int PathIter = 0; PathIter < PathIteration_times; PathIter++) {
				ENQUEUE_SPLIT_KERNEL(scene_intersect, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(lamp_emission, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(queue_enqueue, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(background_buffer_update, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(shader_eval, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(holdout_emission_blurring_pathtermination_ao, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(direct_lighting, global_size, local_size);
				ENQUEUE_SPLIT_KERNEL(shadow_blocked, global_size_shadow_blocked, local_size);
				ENQUEUE_SPLIT_KERNEL(next_iteration_setup, global_size, local_size);
				if(task->get_cancel()) {
					canceled = true;
					break;
				}
			}

			/* Read ray-state into Host memory to decide if we should exit
			 * path-iteration in host.
			 */
			ciErr = clEnqueueReadBuffer(cqCommandQueue,
			                            ray_state,
			                            CL_TRUE,
			                            0,
			                            global_size[0] * global_size[1] * sizeof(char),
			                            hostRayStateArray,
			                            0,
			                            NULL,
			                            NULL);
			assert(ciErr == CL_SUCCESS);

			activeRaysAvailable = false;

			for(int rayStateIter = 0;
			    rayStateIter < global_size[0] * global_size[1];
			    ++rayStateIter)
			{
				if(int8_t(hostRayStateArray[rayStateIter]) != RAY_INACTIVE) {
					/* Not all rays are RAY_INACTIVE. */
					activeRaysAvailable = true;
					break;
				}
			}

			if(activeRaysAvailable) {
				numHostIntervention++;
				PathIteration_times = PATH_ITER_INC_FACTOR;
				/* Host intervention done before all rays become RAY_INACTIVE;
				 * Set do more initial iterations for the next tile.
				 */
				numNextPathIterTimes += PATH_ITER_INC_FACTOR;
			}
			if(task->get_cancel()) {
				canceled = true;
				break;
			}
		}

		/* Execute SumALLRadiance kernel to accumulate radiance calculated in
		 * per_sample_output_buffers into RenderTile's output buffer.
		 */
		if(!canceled) {
			size_t sum_all_radiance_local_size[2] = {16, 16};
			size_t sum_all_radiance_global_size[2];
			sum_all_radiance_global_size[0] =
				(((d_w - 1) / sum_all_radiance_local_size[0]) + 1) *
				sum_all_radiance_local_size[0];
			sum_all_radiance_global_size[1] =
				(((d_h - 1) / sum_all_radiance_local_size[1]) + 1) *
				sum_all_radiance_local_size[1];
			ENQUEUE_SPLIT_KERNEL(sum_all_radiance,
			                     sum_all_radiance_global_size,
			                     sum_all_radiance_local_size);
		}

#undef ENQUEUE_SPLIT_KERNEL
#undef GLUE

		if(numHostIntervention == 0) {
			/* This means that we are executing kernel more than required
			 * Must avoid this for the next sample/tile.
			 */
			PathIteration_times = ((numNextPathIterTimes - PATH_ITER_INC_FACTOR) <= 0) ?
			PATH_ITER_INC_FACTOR : numNextPathIterTimes - PATH_ITER_INC_FACTOR;
		}
		else {
			/* Number of path-iterations done for this tile is set as
			 * Initial path-iteration times for the next tile
			 */
			PathIteration_times = numNextPathIterTimes;
		}

		first_tile = false;
	}

	/* Calculates the amount of memory that has to be always
	 * allocated in order for the split kernel to function.
	 * This memory is tile/scene-property invariant (meaning,
	 * the value returned by this function does not depend
	 * on the user set tile size or scene properties.
	 */
	size_t get_invariable_mem_allocated()
	{
		size_t total_invariable_mem_allocated = 0;
		size_t KernelGlobals_size = 0;

		KernelGlobals_size = get_KernelGlobals_size();

		total_invariable_mem_allocated += KernelGlobals_size; /* KernelGlobals size */
		total_invariable_mem_allocated += NUM_QUEUES * sizeof(unsigned int); /* Queue index size */
		total_invariable_mem_allocated += sizeof(char); /* use_queues_flag size */

		return total_invariable_mem_allocated;
	}

	/* Calculate the memory that has-to-be/has-been allocated for
	 * the split kernel to function.
	 */
	size_t get_tile_specific_mem_allocated(const int2 tile_size)
	{
		size_t tile_specific_mem_allocated = 0;

		/* Get required tile info */
		unsigned int user_set_tile_w = tile_size.x;
		unsigned int user_set_tile_h = tile_size.y;

#ifdef __WORK_STEALING__
		/* Calculate memory to be allocated for work_pools in
		 * case of work_stealing.
		 */
		size_t max_global_size[2];
		size_t max_num_work_pools = 0;
		max_global_size[0] =
			(((user_set_tile_w - 1) / SPLIT_KERNEL_LOCAL_SIZE_X) + 1) *
			SPLIT_KERNEL_LOCAL_SIZE_X;
		max_global_size[1] =
			(((user_set_tile_h - 1) / SPLIT_KERNEL_LOCAL_SIZE_Y) + 1) *
			SPLIT_KERNEL_LOCAL_SIZE_Y;
		max_num_work_pools =
			(max_global_size[0] * max_global_size[1]) /
			(SPLIT_KERNEL_LOCAL_SIZE_X * SPLIT_KERNEL_LOCAL_SIZE_Y);
		tile_specific_mem_allocated += max_num_work_pools * sizeof(unsigned int);
#endif

		tile_specific_mem_allocated +=
			user_set_tile_w * user_set_tile_h * per_thread_output_buffer_size;
		tile_specific_mem_allocated +=
			user_set_tile_w * user_set_tile_h * sizeof(RNG);

		return tile_specific_mem_allocated;
	}

	/* Calculates the texture memories and KernelData (d_data) memory
	 * that has been allocated.
	 */
	size_t get_scene_specific_mem_allocated(cl_mem d_data)
	{
		size_t scene_specific_mem_allocated = 0;
		/* Calculate texture memories. */
#define KERNEL_TEX(type, ttype, name) \
	scene_specific_mem_allocated += get_tex_size(#name);
#include "kernel_textures.h"
#undef KERNEL_TEX
		size_t d_data_size;
		ciErr = clGetMemObjectInfo(d_data,
		                           CL_MEM_SIZE,
		                           sizeof(d_data_size),
		                           &d_data_size,
		                           NULL);
		assert(ciErr == CL_SUCCESS && "Can't get d_data mem object info");
		scene_specific_mem_allocated += d_data_size;
		return scene_specific_mem_allocated;
	}

	/* Calculate the memory required for one thread in split kernel. */
	size_t get_per_thread_memory()
	{
		size_t shaderdata_size = 0;
		/* TODO(sergey): This will actually over-allocate if
		 * particular kernel does not support multiclosure.
		 */
		shaderdata_size = get_shader_data_size(current_max_closure);
		size_t retval = sizeof(RNG)
			+ sizeof(float3)          /* Throughput size */
			+ sizeof(float)           /* L transparent size */
			+ sizeof(char)            /* Ray state size */
			+ sizeof(unsigned int)    /* Work element size */
			+ sizeof(int)             /* ISLamp_size */
			+ sizeof(PathRadiance) + sizeof(Ray) + sizeof(PathState)
			+ sizeof(Intersection)    /* Overall isect */
			+ sizeof(Intersection)    /* Instersection_coop_AO */
			+ sizeof(Intersection)    /* Intersection coop DL */
			+ shaderdata_size         /* Overall ShaderData */
			+ (shaderdata_size * 2)   /* ShaderData : DL and shadow */
			+ sizeof(Ray) + sizeof(BsdfEval)
			+ sizeof(float3)          /* AOAlpha size */
			+ sizeof(float3)          /* AOBSDF size */
			+ sizeof(Ray)
			+ (sizeof(int) * NUM_QUEUES)
			+ per_thread_output_buffer_size;
		return retval;
	}

	/* Considers the total memory available in the device and
	 * and returns the maximum global work size possible.
	 */
	size_t get_feasible_global_work_size(int2 tile_size, cl_mem d_data)
	{
		/* Calculate invariably allocated memory. */
		size_t invariable_mem_allocated = get_invariable_mem_allocated();
		/* Calculate tile specific allocated memory. */
		size_t tile_specific_mem_allocated =
			get_tile_specific_mem_allocated(tile_size);
		/* Calculate scene specific allocated memory. */
		size_t scene_specific_mem_allocated =
			get_scene_specific_mem_allocated(d_data);
		/* Calculate total memory available for the threads in global work size. */
		size_t available_memory = total_allocatable_memory
			- invariable_mem_allocated
			- tile_specific_mem_allocated
			- scene_specific_mem_allocated
			- DATA_ALLOCATION_MEM_FACTOR;
		size_t per_thread_memory_required = get_per_thread_memory();
		return (available_memory / per_thread_memory_required);
	}

	/* Checks if the device has enough memory to render the whole tile;
	 * If not, we should split single tile into multiple tiles of small size
	 * and process them all.
	 */
	bool need_to_split_tile(unsigned int d_w,
	                        unsigned int d_h,
	                        int2 max_render_feasible_tile_size)
	{
		size_t global_size_estimate[2];
		/* TODO(sergey): Such round-ups are in quite few places, need to replace
		 * them with an utility macro.
		 */
		global_size_estimate[0] =
			(((d_w - 1) / SPLIT_KERNEL_LOCAL_SIZE_X) + 1) *
			SPLIT_KERNEL_LOCAL_SIZE_X;
		global_size_estimate[1] =
			(((d_h - 1) / SPLIT_KERNEL_LOCAL_SIZE_Y) + 1) *
			SPLIT_KERNEL_LOCAL_SIZE_Y;
		if((global_size_estimate[0] * global_size_estimate[1]) >
		   (max_render_feasible_tile_size.x * max_render_feasible_tile_size.y))
		{
			return true;
		}
		else {
			return false;
		}
	}

	/* Considers the scene properties, global memory available in the device
	 * and returns a rectanglular tile dimension (approx the maximum)
	 * that should render on split kernel.
	 */
	int2 get_max_render_feasible_tile_size(size_t feasible_global_work_size)
	{
		int2 max_render_feasible_tile_size;
		int square_root_val = (int)sqrt(feasible_global_work_size);
		max_render_feasible_tile_size.x = square_root_val;
		max_render_feasible_tile_size.y = square_root_val;
		/* Ciel round-off max_render_feasible_tile_size. */
		int2 ceil_render_feasible_tile_size;
		ceil_render_feasible_tile_size.x =
			(((max_render_feasible_tile_size.x - 1) / SPLIT_KERNEL_LOCAL_SIZE_X) + 1) *
			SPLIT_KERNEL_LOCAL_SIZE_X;
		ceil_render_feasible_tile_size.y =
			(((max_render_feasible_tile_size.y - 1) / SPLIT_KERNEL_LOCAL_SIZE_Y) + 1) *
			SPLIT_KERNEL_LOCAL_SIZE_Y;
		if(ceil_render_feasible_tile_size.x * ceil_render_feasible_tile_size.y <=
		   feasible_global_work_size)
		{
			return ceil_render_feasible_tile_size;
		}
		/* Floor round-off max_render_feasible_tile_size. */
		int2 floor_render_feasible_tile_size;
		floor_render_feasible_tile_size.x =
			(max_render_feasible_tile_size.x / SPLIT_KERNEL_LOCAL_SIZE_X) *
			SPLIT_KERNEL_LOCAL_SIZE_X;
		floor_render_feasible_tile_size.y =
			(max_render_feasible_tile_size.y / SPLIT_KERNEL_LOCAL_SIZE_Y) *
			SPLIT_KERNEL_LOCAL_SIZE_Y;
		return floor_render_feasible_tile_size;
	}

	/* Try splitting the current tile into multiple smaller
	 * almost-square-tiles.
	 */
	int2 get_split_tile_size(RenderTile rtile,
	                         int2 max_render_feasible_tile_size)
	{
		int2 split_tile_size;
		int num_global_threads = max_render_feasible_tile_size.x *
		                         max_render_feasible_tile_size.y;
		int d_w = rtile.w;
		int d_h = rtile.h;
		/* Ceil round off d_w and d_h */
		d_w = (((d_w - 1) / SPLIT_KERNEL_LOCAL_SIZE_X) + 1) *
			SPLIT_KERNEL_LOCAL_SIZE_X;
		d_h = (((d_h - 1) / SPLIT_KERNEL_LOCAL_SIZE_Y) + 1) *
			SPLIT_KERNEL_LOCAL_SIZE_Y;
		while(d_w * d_h > num_global_threads) {
			/* Halve the longer dimension. */
			if(d_w >= d_h) {
				d_w = d_w / 2;
				d_w = (((d_w - 1) / SPLIT_KERNEL_LOCAL_SIZE_X) + 1) *
					SPLIT_KERNEL_LOCAL_SIZE_X;
			}
			else {
				d_h = d_h / 2;
				d_h = (((d_h - 1) / SPLIT_KERNEL_LOCAL_SIZE_Y) + 1) *
					SPLIT_KERNEL_LOCAL_SIZE_Y;
			}
		}
		split_tile_size.x = d_w;
		split_tile_size.y = d_h;
		return split_tile_size;
	}

	/* Splits existing tile into multiple tiles of tile size split_tile_size. */
	vector<SplitRenderTile> split_tiles(RenderTile rtile, int2 split_tile_size)
	{
		vector<SplitRenderTile> to_path_trace_rtile;
		int d_w = rtile.w;
		int d_h = rtile.h;
		int num_tiles_x = (((d_w - 1) / split_tile_size.x) + 1);
		int num_tiles_y = (((d_h - 1) / split_tile_size.y) + 1);
		/* Buffer and rng_state offset calc. */
		size_t offset_index = rtile.offset + (rtile.x + rtile.y * rtile.stride);
		size_t offset_x = offset_index % rtile.stride;
		size_t offset_y = offset_index / rtile.stride;
		/* Resize to_path_trace_rtile. */
		to_path_trace_rtile.resize(num_tiles_x * num_tiles_y);
		for(int tile_iter_y = 0; tile_iter_y < num_tiles_y; tile_iter_y++) {
			for(int tile_iter_x = 0; tile_iter_x < num_tiles_x; tile_iter_x++) {
				int rtile_index = tile_iter_y * num_tiles_x + tile_iter_x;
				to_path_trace_rtile[rtile_index].rng_state_offset_x = offset_x + tile_iter_x * split_tile_size.x;
				to_path_trace_rtile[rtile_index].rng_state_offset_y = offset_y + tile_iter_y * split_tile_size.y;
				to_path_trace_rtile[rtile_index].buffer_offset_x = offset_x + tile_iter_x * split_tile_size.x;
				to_path_trace_rtile[rtile_index].buffer_offset_y = offset_y + tile_iter_y * split_tile_size.y;
				to_path_trace_rtile[rtile_index].start_sample = rtile.start_sample;
				to_path_trace_rtile[rtile_index].num_samples = rtile.num_samples;
				to_path_trace_rtile[rtile_index].sample = rtile.sample;
				to_path_trace_rtile[rtile_index].resolution = rtile.resolution;
				to_path_trace_rtile[rtile_index].offset = rtile.offset;
				to_path_trace_rtile[rtile_index].buffers = rtile.buffers;
				to_path_trace_rtile[rtile_index].buffer = rtile.buffer;
				to_path_trace_rtile[rtile_index].rng_state = rtile.rng_state;
				to_path_trace_rtile[rtile_index].x = rtile.x + (tile_iter_x * split_tile_size.x);
				to_path_trace_rtile[rtile_index].y = rtile.y + (tile_iter_y * split_tile_size.y);
				to_path_trace_rtile[rtile_index].buffer_rng_state_stride = rtile.stride;
				/* Fill width and height of the new render tile. */
				to_path_trace_rtile[rtile_index].w = (tile_iter_x == (num_tiles_x - 1)) ?
					(d_w - (tile_iter_x * split_tile_size.x)) /* Border tile */
					: split_tile_size.x;
				to_path_trace_rtile[rtile_index].h = (tile_iter_y == (num_tiles_y - 1)) ?
					(d_h - (tile_iter_y * split_tile_size.y)) /* Border tile */
					: split_tile_size.y;
				to_path_trace_rtile[rtile_index].stride = to_path_trace_rtile[rtile_index].w;
			}
		}
		return to_path_trace_rtile;
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
			bool initialize_data_and_check_render_feasibility = false;
			bool need_to_split_tiles_further = false;
			int2 max_render_feasible_tile_size;
			size_t feasible_global_work_size;
			const int2 tile_size = task->requested_tile_size;
			/* Keep rendering tiles until done. */
			while(task->acquire_tile(this, tile)) {
				if(!initialize_data_and_check_render_feasibility) {
					/* Initialize data. */
					/* Calculate per_thread_output_buffer_size. */
					size_t output_buffer_size = 0;
					ciErr = clGetMemObjectInfo((cl_mem)tile.buffer,
					                           CL_MEM_SIZE,
					                           sizeof(output_buffer_size),
					                           &output_buffer_size,
					                           NULL);
					assert(ciErr == CL_SUCCESS && "Can't get tile.buffer mem object info");
					/* This value is different when running on AMD and NV. */
					if(background) {
						/* In offline render the number of buffer elements
						 * associated with tile.buffer is the current tile size.
						 */
						per_thread_output_buffer_size =
							output_buffer_size / (tile.w * tile.h);
					}
					else {
						/* interactive rendering, unlike offline render, the number of buffer elements
						 * associated with tile.buffer is the entire viewport size.
						 */
						per_thread_output_buffer_size =
							output_buffer_size / (tile.buffers->params.width *
							                      tile.buffers->params.height);
					}
					/* Check render feasibility. */
					feasible_global_work_size = get_feasible_global_work_size(
						tile_size,
						CL_MEM_PTR(const_mem_map["__data"]->device_pointer));
					max_render_feasible_tile_size =
						get_max_render_feasible_tile_size(
							feasible_global_work_size);
					need_to_split_tiles_further =
						need_to_split_tile(tile_size.x,
						                   tile_size.y,
						                   max_render_feasible_tile_size);
					initialize_data_and_check_render_feasibility = true;
				}
				if(need_to_split_tiles_further) {
					int2 split_tile_size =
						get_split_tile_size(tile,
						                    max_render_feasible_tile_size);
					vector<SplitRenderTile> to_path_trace_render_tiles =
						split_tiles(tile, split_tile_size);
					/* Print message to console */
					if(background && (to_path_trace_render_tiles.size() > 1)) {
						fprintf(stderr, "Message : Tiles need to be split "
						        "further inside path trace (due to insufficient "
						        "device-global-memory for split kernel to "
						        "function) \n"
						        "The current tile of dimensions %dx%d is split "
						        "into tiles of dimension %dx%d for render \n",
						        tile.w, tile.h,
						        split_tile_size.x,
						        split_tile_size.y);
					}
					/* Process all split tiles. */
					for(int tile_iter = 0;
					    tile_iter < to_path_trace_render_tiles.size();
					    ++tile_iter)
					{
						path_trace(task,
						           to_path_trace_render_tiles[tile_iter],
						           max_render_feasible_tile_size);
					}
				}
				else {
					/* No splitting required; process the entire tile at once. */
					/* Render feasible tile size is user-set-tile-size itself. */
					max_render_feasible_tile_size.x =
						(((tile_size.x - 1) / SPLIT_KERNEL_LOCAL_SIZE_X) + 1) *
						SPLIT_KERNEL_LOCAL_SIZE_X;
					max_render_feasible_tile_size.y =
						(((tile_size.y - 1) / SPLIT_KERNEL_LOCAL_SIZE_Y) + 1) *
						SPLIT_KERNEL_LOCAL_SIZE_Y;
					/* buffer_rng_state_stride is stride itself. */
					SplitRenderTile split_tile(tile);
					split_tile.buffer_rng_state_stride = tile.stride;
					path_trace(task, split_tile, max_render_feasible_tile_size);
				}
				tile.sample = tile.start_sample + tile.num_samples;

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

				task->release_tile(tile);
			}
		}
	}

protected:
	cl_mem mem_alloc(size_t bufsize, cl_mem_flags mem_flag = CL_MEM_READ_WRITE)
	{
		cl_mem ptr;
		assert(bufsize != 0);
		ptr = clCreateBuffer(cxContext, mem_flag, bufsize, NULL, &ciErr);
		opencl_assert_err(ciErr, "clCreateBuffer");
		return ptr;
	}

	/* ** Those guys are for workign around some compiler-specific bugs ** */

	cl_program load_cached_kernel(
	        const DeviceRequestedFeatures& /*requested_features*/,
	        OpenCLCache::ProgramName /*program_name*/,
	        thread_scoped_lock /*cache_locker*/)
	{
		VLOG(2) << "Skip loading kernel from cache, "
		        << "not supported by split kernel.";
		return NULL;
	}

	void store_cached_kernel(cl_platform_id /*platform*/,
	                         cl_device_id /*device*/,
	                         cl_program /*program*/,
	                         OpenCLCache::ProgramName /*program_name*/,
	                         thread_scoped_lock& /*slot_locker*/)
	{
		VLOG(2) << "Skip storing kernel in cache, "
		        << "not supported by split kernel.";
	}

	string build_options_for_base_program(
	        const DeviceRequestedFeatures& requested_features)
	{
		return requested_features.get_build_options();
	}
};

Device *device_opencl_create(DeviceInfo& info, Stats &stats, bool background)
{
	vector<OpenCLPlatformDevice> usable_devices;
	opencl_get_usable_devices(&usable_devices);
	assert(info.num < usable_devices.size());
	const OpenCLPlatformDevice& platform_device = usable_devices[info.num];
	const string& platform_name = platform_device.platform_name;
	const cl_device_type device_type = platform_device.device_type;
	if(opencl_kernel_use_split(platform_name, device_type)) {
		VLOG(1) << "Using split kernel.";
		return new OpenCLDeviceSplitKernel(info, stats, background);
	} else {
		VLOG(1) << "Using mega kernel.";
		return new OpenCLDeviceMegaKernel(info, stats, background);
	}
}

bool device_opencl_init(void)
{
	static bool initialized = false;
	static bool result = false;

	if(initialized)
		return result;

	initialized = true;

	if(opencl_device_type() != 0) {
		int clew_result = clewInit();
		if(clew_result == CLEW_SUCCESS) {
			VLOG(1) << "CLEW initialization succeeded.";
			result = true;
		}
		else {
			VLOG(1) << "CLEW initialization failed: "
			        << ((clew_result == CLEW_ERROR_ATEXIT_FAILED)
			            ? "Error setting up atexit() handler"
			            : "Error opening the library");
		}
	}
	else {
		VLOG(1) << "Skip initializing CLEW, platform is force disabled.";
		result = false;
	}

	return result;
}

void device_opencl_info(vector<DeviceInfo>& devices)
{
	vector<OpenCLPlatformDevice> usable_devices;
	opencl_get_usable_devices(&usable_devices);
	/* Devices are numbered consecutively across platforms. */
	int num_devices = 0;
	foreach(OpenCLPlatformDevice& platform_device, usable_devices) {
		const string& platform_name = platform_device.platform_name;
		const cl_device_type device_type = platform_device.device_type;
		const string& device_name = platform_device.device_name;
		DeviceInfo info;
		info.type = DEVICE_OPENCL;
		info.description = string_remove_trademark(string(device_name));
		info.num = num_devices;
		info.id = string_printf("OPENCL_%d", info.num);
		/* We don't know if it's used for display, but assume it is. */
		info.display_device = true;
		info.advanced_shading = opencl_kernel_use_advanced_shading(platform_name);
		info.pack_images = true;
		info.use_split_kernel = opencl_kernel_use_split(platform_name,
		                                                device_type);
		devices.push_back(info);
		num_devices++;
	}
}

string device_opencl_capabilities(void)
{
	if(opencl_device_type() == 0) {
		return "All OpenCL devices are forced to be OFF";
	}
	string result = "";
	string error_msg = "";  /* Only used by opencl_assert(), but in the future
	                         * it could also be nicely reported to the console.
	                         */
	cl_uint num_platforms = 0;
	opencl_assert(clGetPlatformIDs(0, NULL, &num_platforms));
	if(num_platforms == 0) {
		return "No OpenCL platforms found\n";
	}
	result += string_printf("Number of platforms: %u\n", num_platforms);

	vector<cl_platform_id> platform_ids;
	platform_ids.resize(num_platforms);
	opencl_assert(clGetPlatformIDs(num_platforms, &platform_ids[0], NULL));

#define APPEND_STRING_INFO(func, id, name, what) \
	do { \
		char data[1024] = "\0"; \
		opencl_assert(func(id, what, sizeof(data), &data, NULL)); \
		result += string_printf("%s: %s\n", name, data); \
	} while(false)
#define APPEND_PLATFORM_STRING_INFO(id, name, what) \
	APPEND_STRING_INFO(clGetPlatformInfo, id, "\tPlatform " name, what)
#define APPEND_DEVICE_STRING_INFO(id, name, what) \
	APPEND_STRING_INFO(clGetDeviceInfo, id, "\t\t\tDevice " name, what)

	vector<cl_device_id> device_ids;
	for(cl_uint platform = 0; platform < num_platforms; ++platform) {
		cl_platform_id platform_id = platform_ids[platform];

		result += string_printf("Platform #%u\n", platform);

		APPEND_PLATFORM_STRING_INFO(platform_id, "Name", CL_PLATFORM_NAME);
		APPEND_PLATFORM_STRING_INFO(platform_id, "Vendor", CL_PLATFORM_VENDOR);
		APPEND_PLATFORM_STRING_INFO(platform_id, "Version", CL_PLATFORM_VERSION);
		APPEND_PLATFORM_STRING_INFO(platform_id, "Profile", CL_PLATFORM_PROFILE);
		APPEND_PLATFORM_STRING_INFO(platform_id, "Extensions", CL_PLATFORM_EXTENSIONS);

		cl_uint num_devices = 0;
		opencl_assert(clGetDeviceIDs(platform_ids[platform],
		                             CL_DEVICE_TYPE_ALL,
		                             0,
		                             NULL,
		                             &num_devices));
		result += string_printf("\tNumber of devices: %u\n", num_devices);

		device_ids.resize(num_devices);
		opencl_assert(clGetDeviceIDs(platform_ids[platform],
		                             CL_DEVICE_TYPE_ALL,
		                             num_devices,
		                             &device_ids[0],
		                             NULL));
		for(cl_uint device = 0; device < num_devices; ++device) {
			cl_device_id device_id = device_ids[device];

			result += string_printf("\t\tDevice: #%u\n", device);

			APPEND_DEVICE_STRING_INFO(device_id, "Name", CL_DEVICE_NAME);
			APPEND_DEVICE_STRING_INFO(device_id, "Vendor", CL_DEVICE_VENDOR);
			APPEND_DEVICE_STRING_INFO(device_id, "OpenCL C Version", CL_DEVICE_OPENCL_C_VERSION);
			APPEND_DEVICE_STRING_INFO(device_id, "Profile", CL_DEVICE_PROFILE);
			APPEND_DEVICE_STRING_INFO(device_id, "Version", CL_DEVICE_VERSION);
			APPEND_DEVICE_STRING_INFO(device_id, "Extensions", CL_DEVICE_EXTENSIONS);
		}
	}

#undef APPEND_STRING_INFO
#undef APPEND_PLATFORM_STRING_INFO
#undef APPEND_DEVICE_STRING_INFO

	return result;
}

CCL_NAMESPACE_END

#endif /* WITH_OPENCL */

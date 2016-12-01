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

#include "device.h"

#include "util_map.h"
#include "util_param.h"
#include "util_string.h"

#include "clew.h"

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
	                     const string& device_name,
	                     const string& hardware_id)
	  : platform_id(platform_id),
	    platform_name(platform_name),
	    device_id(device_id),
	    device_type(device_type),
	    device_name(device_name),
	    hardware_id(hardware_id) {}
	cl_platform_id platform_id;
	string platform_name;
	cl_device_id device_id;
	cl_device_type device_type;
	string device_name;
	string hardware_id;
};

/* Contains all static OpenCL helper functions. */
class OpenCLInfo
{
public:
	static cl_device_type device_type();
	static bool use_debug();
	static bool kernel_use_advanced_shading(const string& platform_name);
	static bool kernel_use_split(const string& platform_name,
	                             const cl_device_type device_type);
	static bool device_supported(const string& platform_name,
	                             const cl_device_id device_id);
	static bool platform_version_check(cl_platform_id platform,
	                                   string *error = NULL);
	static bool device_version_check(cl_device_id device,
	                                 string *error = NULL);
	static string get_hardware_id(string platform_name,
	                              cl_device_id device_id);
	static void get_usable_devices(vector<OpenCLPlatformDevice> *usable_devices,
	                               bool force_all = false);
};

/* Thread safe cache for contexts and programs.
 */
class OpenCLCache
{
	struct Slot
	{
		struct ProgramEntry
		{
			ProgramEntry();
			ProgramEntry(const ProgramEntry& rhs);
			~ProgramEntry();
			cl_program program;
			thread_mutex *mutex;
		};

		Slot();
		Slot(const Slot& rhs);
		~Slot();

		thread_mutex *context_mutex;
		cl_context context;
		typedef map<ustring, ProgramEntry> EntryMap;
		EntryMap programs;

	};

	/* key is combination of platform ID and device ID */
	typedef pair<cl_platform_id, cl_device_id> PlatformDevicePair;

	/* map of Slot objects */
	typedef map<PlatformDevicePair, Slot> CacheMap;
	CacheMap cache;

	/* MD5 hash of the kernel source. */
	string kernel_md5;

	thread_mutex cache_lock;
	thread_mutex kernel_md5_lock;

	/* lazy instantiate */
	static OpenCLCache& global_instance();

public:

	enum ProgramName {
		OCL_DEV_BASE_PROGRAM,
		OCL_DEV_MEGAKERNEL_PROGRAM,
	};

	/* Lookup context in the cache. If this returns NULL, slot_locker
	 * will be holding a lock for the cache. slot_locker should refer to a
	 * default constructed thread_scoped_lock. */
	static cl_context get_context(cl_platform_id platform,
	                              cl_device_id device,
	                              thread_scoped_lock& slot_locker);
	/* Same as above. */
	static cl_program get_program(cl_platform_id platform,
	                              cl_device_id device,
	                              ustring key,
	                              thread_scoped_lock& slot_locker);

	/* Store context in the cache. You MUST have tried to get the item before storing to it. */
	static void store_context(cl_platform_id platform,
	                          cl_device_id device,
	                          cl_context context,
	                          thread_scoped_lock& slot_locker);
	/* Same as above. */
	static void store_program(cl_platform_id platform,
	                          cl_device_id device,
	                          cl_program program,
	                          ustring key,
	                          thread_scoped_lock& slot_locker);

	static string get_kernel_md5();
};

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

class OpenCLDeviceBase : public Device
{
public:
	DedicatedTaskPool task_pool;
	cl_context cxContext;
	cl_command_queue cqCommandQueue;
	cl_platform_id cpPlatform;
	cl_device_id cdDevice;
	cl_int ciErr;

	class OpenCLProgram {
	public:
		OpenCLProgram() : loaded(false), device(NULL) {}
		OpenCLProgram(OpenCLDeviceBase *device,
		              string program_name,
		              string kernel_name,
		              string kernel_build_options,
		              bool use_stdout = true);
		~OpenCLProgram();

		void add_kernel(ustring name);
		void load();

		bool is_loaded()    { return loaded; }
		string get_log()    { return log; }
		void report_error();

		cl_kernel operator()();
		cl_kernel operator()(ustring name);

		void release();

	private:
		bool build_kernel(const string *debug_src);
		bool compile_kernel(const string *debug_src);
		bool load_binary(const string& clbin, const string *debug_src = NULL);
		bool save_binary(const string& clbin);

		void add_log(string msg, bool is_debug);
		void add_error(string msg);

		bool loaded;
		cl_program program;
		OpenCLDeviceBase *device;

		/* Used for the OpenCLCache key. */
		string program_name;

		string kernel_file, kernel_build_options, device_md5;

		bool use_stdout;
		string log, error_msg;
		string compile_output;

		map<ustring, cl_kernel> kernels;
	};

	OpenCLProgram base_program;

	typedef map<string, device_vector<uchar>*> ConstMemMap;
	typedef map<string, device_ptr> MemMap;

	ConstMemMap const_mem_map;
	MemMap mem_map;
	device_ptr null_mem;

	bool device_initialized;
	string platform_name;

	bool opencl_error(cl_int err);
	void opencl_error(const string& message);
	void opencl_assert_err(cl_int err, const char* where);

	OpenCLDeviceBase(DeviceInfo& info, Stats &stats, bool background_);
	~OpenCLDeviceBase();

	static void CL_CALLBACK context_notify_callback(const char *err_info,
		const void * /*private_info*/, size_t /*cb*/, void *user_data);

	bool opencl_version_check();

	string device_md5_hash(string kernel_custom_build_options = "");
	bool load_kernels(const DeviceRequestedFeatures& requested_features);

	/* Has to be implemented by the real device classes.
	 * The base device will then load all these programs. */
	virtual void load_kernels(const DeviceRequestedFeatures& requested_features,
	                          vector<OpenCLProgram*> &programs) = 0;

	void mem_alloc(device_memory& mem, MemoryType type);
	void mem_copy_to(device_memory& mem);
	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem);
	void mem_zero(device_memory& mem);
	void mem_free(device_memory& mem);
	void const_copy_to(const char *name, void *host, size_t size);
	void tex_alloc(const char *name,
	               device_memory& mem,
	               InterpolationType /*interpolation*/,
	               ExtensionType /*extension*/);
	void tex_free(device_memory& mem);

	size_t global_size_round_up(int group_size, int global_size);
	void enqueue_kernel(cl_kernel kernel, size_t w, size_t h);
	void set_kernel_arg_mem(cl_kernel kernel, cl_uint *narg, const char *name);

	void film_convert(DeviceTask& task, device_ptr buffer, device_ptr rgba_byte, device_ptr rgba_half);
	void shader(DeviceTask& task);

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
	string kernel_build_options(const string *debug_src = NULL);

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
	                    const ArgumentWrapper& arg33 = ArgumentWrapper());

	void release_kernel_safe(cl_kernel kernel);
	void release_mem_object_safe(cl_mem mem);
	void release_program_safe(cl_program program);

	/* ** Those guys are for workign around some compiler-specific bugs ** */

	virtual cl_program load_cached_kernel(
	        ustring key,
	        thread_scoped_lock& cache_locker);

	virtual void store_cached_kernel(
	        cl_program program,
	        ustring key,
	        thread_scoped_lock& cache_locker);

	virtual string build_options_for_base_program(
	        const DeviceRequestedFeatures& /*requested_features*/);
};

Device *opencl_create_mega_device(DeviceInfo& info, Stats& stats, bool background);
Device *opencl_create_split_device(DeviceInfo& info, Stats& stats, bool background);

CCL_NAMESPACE_END

#endif

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

#include "opencl.h"

#include "util_logging.h"
#include "util_path.h"
#include "util_time.h"

using std::cerr;
using std::endl;

CCL_NAMESPACE_BEGIN

OpenCLCache::Slot::ProgramEntry::ProgramEntry()
 : program(NULL),
   mutex(NULL)
{
}

OpenCLCache::Slot::ProgramEntry::ProgramEntry(const ProgramEntry& rhs)
 : program(rhs.program),
   mutex(NULL)
{
}

OpenCLCache::Slot::ProgramEntry::~ProgramEntry()
{
	delete mutex;
}

OpenCLCache::Slot::Slot()
 : context_mutex(NULL),
   context(NULL)
{
}

OpenCLCache::Slot::Slot(const Slot& rhs)
 : context_mutex(NULL),
   context(NULL),
   programs(rhs.programs)
{
}

OpenCLCache::Slot::~Slot()
{
	delete context_mutex;
}

OpenCLCache& OpenCLCache::global_instance()
{
	static OpenCLCache instance;
	return instance;
}

cl_context OpenCLCache::get_context(cl_platform_id platform,
                                    cl_device_id device,
                                    thread_scoped_lock& slot_locker)
{
	assert(platform != NULL);

	OpenCLCache& self = global_instance();

	thread_scoped_lock cache_lock(self.cache_lock);

	pair<CacheMap::iterator,bool> ins = self.cache.insert(
		CacheMap::value_type(PlatformDevicePair(platform, device), Slot()));

	Slot &slot = ins.first->second;

	/* create slot lock only while holding cache lock */
	if(!slot.context_mutex)
		slot.context_mutex = new thread_mutex;

	/* need to unlock cache before locking slot, to allow store to complete */
	cache_lock.unlock();

	/* lock the slot */
	slot_locker = thread_scoped_lock(*slot.context_mutex);

	/* If the thing isn't cached */
	if(slot.context == NULL) {
		/* return with the caller's lock holder holding the slot lock */
		return NULL;
	}

	/* the item was already cached, release the slot lock */
	slot_locker.unlock();

	cl_int ciErr = clRetainContext(slot.context);
	assert(ciErr == CL_SUCCESS);
	(void)ciErr;

	return slot.context;
}

cl_program OpenCLCache::get_program(cl_platform_id platform,
                                    cl_device_id device,
                                    ustring key,
                                    thread_scoped_lock& slot_locker)
{
	assert(platform != NULL);

	OpenCLCache& self = global_instance();

	thread_scoped_lock cache_lock(self.cache_lock);

	pair<CacheMap::iterator,bool> ins = self.cache.insert(
		CacheMap::value_type(PlatformDevicePair(platform, device), Slot()));

	Slot &slot = ins.first->second;

	pair<Slot::EntryMap::iterator,bool> ins2 = slot.programs.insert(
		Slot::EntryMap::value_type(key, Slot::ProgramEntry()));

	Slot::ProgramEntry &entry = ins2.first->second;

	/* create slot lock only while holding cache lock */
	if(!entry.mutex)
		entry.mutex = new thread_mutex;

	/* need to unlock cache before locking slot, to allow store to complete */
	cache_lock.unlock();

	/* lock the slot */
	slot_locker = thread_scoped_lock(*entry.mutex);

	/* If the thing isn't cached */
	if(entry.program == NULL) {
		/* return with the caller's lock holder holding the slot lock */
		return NULL;
	}

	/* the item was already cached, release the slot lock */
	slot_locker.unlock();

	cl_int ciErr = clRetainProgram(entry.program);
	assert(ciErr == CL_SUCCESS);
	(void)ciErr;

	return entry.program;
}

void OpenCLCache::store_context(cl_platform_id platform,
                                cl_device_id device,
                                cl_context context,
                                thread_scoped_lock& slot_locker)
{
	assert(platform != NULL);
	assert(device != NULL);
	assert(context != NULL);

	OpenCLCache &self = global_instance();

	thread_scoped_lock cache_lock(self.cache_lock);
	CacheMap::iterator i = self.cache.find(PlatformDevicePair(platform, device));
	cache_lock.unlock();

	Slot &slot = i->second;

	/* sanity check */
	assert(i != self.cache.end());
	assert(slot.context == NULL);

	slot.context = context;

	/* unlock the slot */
	slot_locker.unlock();

	/* increment reference count in OpenCL.
	 * The caller is going to release the object when done with it. */
	cl_int ciErr = clRetainContext(context);
	assert(ciErr == CL_SUCCESS);
	(void)ciErr;
}

void OpenCLCache::store_program(cl_platform_id platform,
                                cl_device_id device,
                                cl_program program,
                                ustring key,
                                thread_scoped_lock& slot_locker)
{
	assert(platform != NULL);
	assert(device != NULL);
	assert(program != NULL);

	OpenCLCache &self = global_instance();

	thread_scoped_lock cache_lock(self.cache_lock);

	CacheMap::iterator i = self.cache.find(PlatformDevicePair(platform, device));
	assert(i != self.cache.end());
	Slot &slot = i->second;

	Slot::EntryMap::iterator i2 = slot.programs.find(key);
	assert(i2 != slot.programs.end());
	Slot::ProgramEntry &entry = i2->second;

	assert(entry.program == NULL);

	cache_lock.unlock();

	entry.program = program;

	/* unlock the slot */
	slot_locker.unlock();

	/* Increment reference count in OpenCL.
	 * The caller is going to release the object when done with it.
	 */
	cl_int ciErr = clRetainProgram(program);
	assert(ciErr == CL_SUCCESS);
	(void)ciErr;
}

string OpenCLCache::get_kernel_md5()
{
	OpenCLCache &self = global_instance();
	thread_scoped_lock lock(self.kernel_md5_lock);

	if(self.kernel_md5.empty()) {
		self.kernel_md5 = path_files_md5_hash(path_get("kernel"));
	}
	return self.kernel_md5;
}

OpenCLDeviceBase::OpenCLProgram::OpenCLProgram(OpenCLDeviceBase *device,
                                               string program_name,
                                               string kernel_file,
                                               string kernel_build_options,
                                               bool use_stdout)
 : device(device),
   program_name(program_name),
   kernel_file(kernel_file),
   kernel_build_options(kernel_build_options),
   use_stdout(use_stdout)
{
	loaded = false;
	program = NULL;
}

OpenCLDeviceBase::OpenCLProgram::~OpenCLProgram()
{
	release();
}

void OpenCLDeviceBase::OpenCLProgram::release()
{
	for(map<ustring, cl_kernel>::iterator kernel = kernels.begin(); kernel != kernels.end(); ++kernel) {
		if(kernel->second) {
			clReleaseKernel(kernel->second);
			kernel->second = NULL;
		}
	}
	if(program) {
		clReleaseProgram(program);
		program = NULL;
	}
}

void OpenCLDeviceBase::OpenCLProgram::add_log(string msg, bool debug)
{
	if(!use_stdout) {
		log += msg + "\n";
	}
	else if(!debug) {
		printf("%s\n", msg.c_str());
	}
	else {
		VLOG(2) << msg;
	}
}

void OpenCLDeviceBase::OpenCLProgram::add_error(string msg)
{
	if(use_stdout) {
		fprintf(stderr, "%s\n", msg.c_str());
	}
	if(error_msg == "") {
		error_msg += "\n";
	}
	error_msg += msg;
}

void OpenCLDeviceBase::OpenCLProgram::add_kernel(ustring name)
{
	if(!kernels.count(name)) {
		kernels[name] = NULL;
	}
}

bool OpenCLDeviceBase::OpenCLProgram::build_kernel(const string *debug_src)
{
	string build_options;
	build_options = device->kernel_build_options(debug_src) + kernel_build_options;

	cl_int ciErr = clBuildProgram(program, 0, NULL, build_options.c_str(), NULL, NULL);

	/* show warnings even if build is successful */
	size_t ret_val_size = 0;

	clGetProgramBuildInfo(program, device->cdDevice, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);

	if(ciErr != CL_SUCCESS) {
		add_error(string("OpenCL build failed with error ") + clewErrorString(ciErr) + ", errors in console.");
	}

	if(ret_val_size > 1) {
		vector<char> build_log(ret_val_size + 1);
		clGetProgramBuildInfo(program, device->cdDevice, CL_PROGRAM_BUILD_LOG, ret_val_size, &build_log[0], NULL);

		build_log[ret_val_size] = '\0';
		/* Skip meaningless empty output from the NVidia compiler. */
		if(!(ret_val_size == 2 && build_log[0] == '\n')) {
			add_log(string("OpenCL program ") + program_name + " build output: " + string(&build_log[0]), ciErr == CL_SUCCESS);
		}
	}

	return (ciErr == CL_SUCCESS);
}

bool OpenCLDeviceBase::OpenCLProgram::compile_kernel(const string *debug_src)
{
	string source = "#include \"kernels/opencl/" + kernel_file + "\" // " + OpenCLCache::get_kernel_md5() + "\n";
	/* We compile kernels consisting of many files. unfortunately OpenCL
	 * kernel caches do not seem to recognize changes in included files.
	 * so we force recompile on changes by adding the md5 hash of all files.
	 */
	source = path_source_replace_includes(source, path_get("kernel"));

	if(debug_src) {
		path_write_text(*debug_src, source);
	}

	size_t source_len = source.size();
	const char *source_str = source.c_str();
	cl_int ciErr;

	program = clCreateProgramWithSource(device->cxContext,
	                                   1,
	                                   &source_str,
	                                   &source_len,
	                                   &ciErr);

	if(ciErr != CL_SUCCESS) {
		add_error(string("OpenCL program creation failed: ") + clewErrorString(ciErr));
		return false;
	}

	double starttime = time_dt();
	add_log(string("Compiling OpenCL program ") + program_name.c_str(), false);
	add_log(string("Build flags: ") + kernel_build_options, true);

	if(!build_kernel(debug_src))
		return false;

	add_log(string("Kernel compilation of ") + program_name + " finished in " + string_printf("%.2lfs.\n", time_dt() - starttime), false);

	return true;
}

bool OpenCLDeviceBase::OpenCLProgram::load_binary(const string& clbin,
                                                  const string *debug_src)
{
	/* read binary into memory */
	vector<uint8_t> binary;

	if(!path_read_binary(clbin, binary)) {
		add_error(string_printf("OpenCL failed to read cached binary %s.", clbin.c_str()));
		return false;
	}

	/* create program */
	cl_int status, ciErr;
	size_t size = binary.size();
	const uint8_t *bytes = &binary[0];

	program = clCreateProgramWithBinary(device->cxContext, 1, &device->cdDevice,
		&size, &bytes, &status, &ciErr);

	if(status != CL_SUCCESS || ciErr != CL_SUCCESS) {
		add_error(string("OpenCL failed create program from cached binary ") + clbin + ": "
		                 + clewErrorString(status) + " " + clewErrorString(ciErr));
		return false;
	}

	if(!build_kernel(debug_src))
		return false;

	return true;
}

bool OpenCLDeviceBase::OpenCLProgram::save_binary(const string& clbin)
{
	size_t size = 0;
	clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &size, NULL);

	if(!size)
		return false;

	vector<uint8_t> binary(size);
	uint8_t *bytes = &binary[0];

	clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(uint8_t*), &bytes, NULL);

	return path_write_binary(clbin, binary);
}

void OpenCLDeviceBase::OpenCLProgram::load()
{
	assert(device);

	loaded = false;

	string device_md5 = device->device_md5_hash(kernel_build_options);

	/* Try to use cached kernel. */
	thread_scoped_lock cache_locker;
	ustring cache_key(program_name + device_md5);
	program = device->load_cached_kernel(cache_key,
	                                     cache_locker);

	if(!program) {
		add_log(string("OpenCL program ") + program_name + " not found in cache.", true);

		string basename = "cycles_kernel_" + program_name + "_" + device_md5 + "_" + OpenCLCache::get_kernel_md5();
		basename = path_cache_get(path_join("kernels", basename));
		string clbin = basename + ".clbin";

		/* path to preprocessed source for debugging */
		string clsrc, *debug_src = NULL;

		if(OpenCLInfo::use_debug()) {
			clsrc = basename + ".cl";
			debug_src = &clsrc;
		}

		/* If binary kernel exists already, try use it. */
		if(path_exists(clbin) && load_binary(clbin)) {
			/* Kernel loaded from binary, nothing to do. */
			add_log(string("Loaded program from ") + clbin + ".", true);
		}
		else {
			add_log(string("Kernel file ") + clbin + " either doesn't exist or failed to be loaded by driver.", true);

			/* If does not exist or loading binary failed, compile kernel. */
			if(!compile_kernel(debug_src)) {
				return;
			}

			/* Save binary for reuse. */
			if(!save_binary(clbin)) {
				add_log(string("Saving compiled OpenCL kernel to ") + clbin + " failed!", true);
			}
		}

		/* Cache the program. */
		device->store_cached_kernel(program,
		                            cache_key,
		                            cache_locker);
	}
	else {
		add_log(string("Found cached OpenCL program ") + program_name + ".", true);
	}

	for(map<ustring, cl_kernel>::iterator kernel = kernels.begin(); kernel != kernels.end(); ++kernel) {
		assert(kernel->second == NULL);
		cl_int ciErr;
		string name = "kernel_ocl_" + kernel->first.string();
		kernel->second = clCreateKernel(program, name.c_str(), &ciErr);
		if(device->opencl_error(ciErr)) {
			add_error(string("Error getting kernel ") + name + " from program " + program_name + ": " + clewErrorString(ciErr));
			return;
		}
	}

	loaded = true;
}

void OpenCLDeviceBase::OpenCLProgram::report_error()
{
	/* If loaded is true, there was no error. */
	if(loaded) return;
	/* if use_stdout is true, the error was already reported. */
	if(use_stdout) return;

	cerr << error_msg << endl;
	if(!compile_output.empty()) {
		cerr << "OpenCL kernel build output for " << program_name << ":" << endl;
		cerr << compile_output << endl;
	}
}

cl_kernel OpenCLDeviceBase::OpenCLProgram::operator()()
{
	assert(kernels.size() == 1);
	return kernels.begin()->second;
}

cl_kernel OpenCLDeviceBase::OpenCLProgram::operator()(ustring name)
{
	assert(kernels.count(name));
	return kernels[name];
}

cl_device_type OpenCLInfo::device_type()
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

bool OpenCLInfo::use_debug()
{
	return DebugFlags().opencl.debug;
}

bool OpenCLInfo::kernel_use_advanced_shading(const string& platform)
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

bool OpenCLInfo::kernel_use_split(const string& platform_name,
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

bool OpenCLInfo::device_supported(const string& platform_name,
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

bool OpenCLInfo::platform_version_check(cl_platform_id platform,
                                        string *error)
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

bool OpenCLInfo::device_version_check(cl_device_id device,
                                      string *error)
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

void OpenCLInfo::get_usable_devices(vector<OpenCLPlatformDevice> *usable_devices,
                                    bool force_all)
{
	const bool force_all_platforms = force_all ||
		(DebugFlags().opencl.kernel_type != DebugFlags::OpenCL::KERNEL_DEFAULT);
	const cl_device_type device_type = OpenCLInfo::device_type();
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
		if(!platform_version_check(platform_id)) {
			FIRST_VLOG(2) << "Ignoring platform " << platform_name
			              << " due to too old compiler version.";
			continue;
		}
		num_devices = 0;
		cl_int ciErr;
		if((ciErr = clGetDeviceIDs(platform_id,
		                  device_type,
		                  0,
		                  NULL,
		                  &num_devices)) != CL_SUCCESS || num_devices == 0)
		{
			FIRST_VLOG(2) << "Ignoring platform " << platform_name
			              << ", failed to fetch number of devices: " << string(clewErrorString(ciErr));
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
			if(!device_version_check(device_id)) {
				FIRST_VLOG(2) << "Ignoring device " << device_name
				              << " due to old compiler version.";
				continue;
			}
			if(force_all_platforms ||
			   device_supported(platform_name, device_id))
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

CCL_NAMESPACE_END

#endif

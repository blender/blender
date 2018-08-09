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

#include "device/device_intern.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_set.h"
#include "util/util_string.h"

CCL_NAMESPACE_BEGIN

Device *device_opencl_create(DeviceInfo& info, Stats &stats, bool background)
{
	vector<OpenCLPlatformDevice> usable_devices;
	OpenCLInfo::get_usable_devices(&usable_devices);
	assert(info.num < usable_devices.size());
	const OpenCLPlatformDevice& platform_device = usable_devices[info.num];
	const string& platform_name = platform_device.platform_name;
	const cl_device_type device_type = platform_device.device_type;
	if(OpenCLInfo::kernel_use_split(platform_name, device_type)) {
		VLOG(1) << "Using split kernel.";
		return opencl_create_split_device(info, stats, background);
	} else {
		VLOG(1) << "Using mega kernel.";
		return opencl_create_mega_device(info, stats, background);
	}
}

bool device_opencl_init(void)
{
	static bool initialized = false;
	static bool result = false;

	if(initialized)
		return result;

	initialized = true;

	if(OpenCLInfo::device_type() != 0) {
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


static cl_int device_opencl_get_num_platforms_safe(cl_uint *num_platforms)
{
#ifdef _WIN32
	__try {
		return clGetPlatformIDs(0, NULL, num_platforms);
	}
	__except(EXCEPTION_EXECUTE_HANDLER) {
		/* Ignore crashes inside the OpenCL driver and hope we can
		 * survive even with corrupted OpenCL installs. */
		fprintf(stderr, "Cycles OpenCL: driver crashed, continuing without OpenCL.\n");
	}

	*num_platforms = 0;
	return CL_DEVICE_NOT_FOUND;
#else
	return clGetPlatformIDs(0, NULL, num_platforms);
#endif
}

void device_opencl_info(vector<DeviceInfo>& devices)
{
	cl_uint num_platforms = 0;
	device_opencl_get_num_platforms_safe(&num_platforms);
	if(num_platforms == 0) {
		return;
	}

	vector<OpenCLPlatformDevice> usable_devices;
	OpenCLInfo::get_usable_devices(&usable_devices);
	/* Devices are numbered consecutively across platforms. */
	int num_devices = 0;
	set<string> unique_ids;
	foreach(OpenCLPlatformDevice& platform_device, usable_devices) {
		/* Compute unique ID for persistent user preferences. */
		const string& platform_name = platform_device.platform_name;
		const cl_device_type device_type = platform_device.device_type;
		const string& device_name = platform_device.device_name;
		string hardware_id = platform_device.hardware_id;
		if(hardware_id == "") {
			hardware_id = string_printf("ID_%d", num_devices);
		}
		string id = string("OPENCL_") + platform_name + "_" + device_name + "_" + hardware_id;

		/* Hardware ID might not be unique, add device number in that case. */
		if(unique_ids.find(id) != unique_ids.end()) {
			id += string_printf("_ID_%d", num_devices);
		}
		unique_ids.insert(id);

		/* Create DeviceInfo. */
		DeviceInfo info;
		info.type = DEVICE_OPENCL;
		info.description = string_remove_trademark(string(device_name));
		info.num = num_devices;
		/* We don't know if it's used for display, but assume it is. */
		info.display_device = true;
		info.advanced_shading = OpenCLInfo::kernel_use_advanced_shading(platform_name);
		info.use_split_kernel = OpenCLInfo::kernel_use_split(platform_name,
		                                                     device_type);
		info.has_volume_decoupled = false;
		info.bvh_layout_mask = BVH_LAYOUT_BVH2;
		info.id = id;

		/* Check OpenCL extensions */
		info.has_half_images = platform_device.device_extensions.find("cl_khr_fp16") != string::npos;

		devices.push_back(info);
		num_devices++;
	}
}

string device_opencl_capabilities(void)
{
	if(OpenCLInfo::device_type() == 0) {
		return "All OpenCL devices are forced to be OFF";
	}
	string result = "";
	string error_msg = "";  /* Only used by opencl_assert(), but in the future
	                         * it could also be nicely reported to the console.
	                         */
	cl_uint num_platforms = 0;
	opencl_assert(device_opencl_get_num_platforms_safe(&num_platforms));
	if(num_platforms == 0) {
		return "No OpenCL platforms found\n";
	}
	result += string_printf("Number of platforms: %u\n", num_platforms);

	vector<cl_platform_id> platform_ids;
	platform_ids.resize(num_platforms);
	opencl_assert(clGetPlatformIDs(num_platforms, &platform_ids[0], NULL));

	typedef char cl_string[1024];

#define APPEND_INFO(func, id, name, what, type) \
	do { \
		type data; \
		memset(&data, 0, sizeof(data)); \
		opencl_assert(func(id, what, sizeof(data), &data, NULL)); \
		result += string_printf("%s: %s\n", name, to_string(data).c_str()); \
	} while(false)
#define APPEND_STRING_EXTENSION_INFO(func, id, name, what) \
	do { \
		char data[1024] = "\0"; \
		size_t length = 0; \
		if(func(id, what, sizeof(data), &data, &length) == CL_SUCCESS) { \
			if(length != 0 && data[0] != '\0') { \
				result += string_printf("%s: %s\n", name, data); \
			} \
		} \
	} while(false)
#define APPEND_PLATFORM_INFO(id, name, what, type) \
	APPEND_INFO(clGetPlatformInfo, id, "\tPlatform " name, what, type)
#define APPEND_DEVICE_INFO(id, name, what, type) \
	APPEND_INFO(clGetDeviceInfo, id, "\t\t\tDevice " name, what, type)
#define APPEND_DEVICE_STRING_EXTENSION_INFO(id, name, what) \
	APPEND_STRING_EXTENSION_INFO(clGetDeviceInfo, id, "\t\t\tDevice " name, what)

	vector<cl_device_id> device_ids;
	for(cl_uint platform = 0; platform < num_platforms; ++platform) {
		cl_platform_id platform_id = platform_ids[platform];

		result += string_printf("Platform #%u\n", platform);

		APPEND_PLATFORM_INFO(platform_id, "Name", CL_PLATFORM_NAME, cl_string);
		APPEND_PLATFORM_INFO(platform_id, "Vendor", CL_PLATFORM_VENDOR, cl_string);
		APPEND_PLATFORM_INFO(platform_id, "Version", CL_PLATFORM_VERSION, cl_string);
		APPEND_PLATFORM_INFO(platform_id, "Profile", CL_PLATFORM_PROFILE, cl_string);
		APPEND_PLATFORM_INFO(platform_id, "Extensions", CL_PLATFORM_EXTENSIONS, cl_string);

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

			APPEND_DEVICE_INFO(device_id, "Name", CL_DEVICE_NAME, cl_string);
			APPEND_DEVICE_STRING_EXTENSION_INFO(device_id, "Board Name", CL_DEVICE_BOARD_NAME_AMD);
			APPEND_DEVICE_INFO(device_id, "Vendor", CL_DEVICE_VENDOR, cl_string);
			APPEND_DEVICE_INFO(device_id, "OpenCL C Version", CL_DEVICE_OPENCL_C_VERSION, cl_string);
			APPEND_DEVICE_INFO(device_id, "Profile", CL_DEVICE_PROFILE, cl_string);
			APPEND_DEVICE_INFO(device_id, "Version", CL_DEVICE_VERSION, cl_string);
			APPEND_DEVICE_INFO(device_id, "Extensions", CL_DEVICE_EXTENSIONS, cl_string);
			APPEND_DEVICE_INFO(device_id, "Max clock frequency (MHz)", CL_DEVICE_MAX_CLOCK_FREQUENCY, cl_uint);
			APPEND_DEVICE_INFO(device_id, "Max compute units", CL_DEVICE_MAX_COMPUTE_UNITS, cl_uint);
			APPEND_DEVICE_INFO(device_id, "Max work group size", CL_DEVICE_MAX_WORK_GROUP_SIZE, size_t);
		}
	}

#undef APPEND_STRING_INFO
#undef APPEND_PLATFORM_STRING_INFO
#undef APPEND_DEVICE_STRING_INFO

	return result;
}

CCL_NAMESPACE_END

#endif /* WITH_OPENCL */

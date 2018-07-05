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

#include "kernel/kernel_types.h"

#include "util/util_algorithm.h"
#include "util/util_debug.h"
#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_md5.h"
#include "util/util_path.h"
#include "util/util_time.h"

CCL_NAMESPACE_BEGIN

struct texture_slot_t {
	texture_slot_t(const string& name, int slot)
		: name(name),
		  slot(slot) {
	}
	string name;
	int slot;
};

bool OpenCLDeviceBase::opencl_error(cl_int err)
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

void OpenCLDeviceBase::opencl_error(const string& message)
{
	if(error_msg == "")
		error_msg = message;
	fprintf(stderr, "%s\n", message.c_str());
}

void OpenCLDeviceBase::opencl_assert_err(cl_int err, const char* where)
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

OpenCLDeviceBase::OpenCLDeviceBase(DeviceInfo& info, Stats &stats, bool background_)
: Device(info, stats, background_),
  memory_manager(this),
  texture_info(this, "__texture_info", MEM_TEXTURE)
{
	cpPlatform = NULL;
	cdDevice = NULL;
	cxContext = NULL;
	cqCommandQueue = NULL;
	null_mem = 0;
	device_initialized = false;
	textures_need_update = true;

	vector<OpenCLPlatformDevice> usable_devices;
	OpenCLInfo::get_usable_devices(&usable_devices);
	if(usable_devices.size() == 0) {
		opencl_error("OpenCL: no devices found.");
		return;
	}
	assert(info.num < usable_devices.size());
	OpenCLPlatformDevice& platform_device = usable_devices[info.num];
	cpPlatform = platform_device.platform_id;
	cdDevice = platform_device.device_id;
	platform_name = platform_device.platform_name;
	device_name = platform_device.device_name;
	VLOG(2) << "Creating new Cycles device for OpenCL platform "
	        << platform_name << ", device "
	        << device_name << ".";

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
	if(opencl_error(ciErr)) {
		opencl_error("OpenCL: Error creating command queue");
		return;
	}

	null_mem = (device_ptr)clCreateBuffer(cxContext, CL_MEM_READ_ONLY, 1, NULL, &ciErr);
	if(opencl_error(ciErr)) {
		opencl_error("OpenCL: Error creating memory buffer for NULL");
		return;
	}

	/* Allocate this right away so that texture_info is placed at offset 0 in the device memory buffers */
	texture_info.resize(1);
	memory_manager.alloc("texture_info", texture_info);

	fprintf(stderr, "Device init success\n");
	device_initialized = true;
}

OpenCLDeviceBase::~OpenCLDeviceBase()
{
	task_pool.stop();

	memory_manager.free();

	if(null_mem)
		clReleaseMemObject(CL_MEM_PTR(null_mem));

	ConstMemMap::iterator mt;
	for(mt = const_mem_map.begin(); mt != const_mem_map.end(); mt++) {
		delete mt->second;
	}

	base_program.release();
	if(cqCommandQueue)
		clReleaseCommandQueue(cqCommandQueue);
	if(cxContext)
		clReleaseContext(cxContext);
}

void CL_CALLBACK OpenCLDeviceBase::context_notify_callback(const char *err_info,
	const void * /*private_info*/, size_t /*cb*/, void *user_data)
{
	string device_name = OpenCLInfo::get_device_name((cl_device_id)user_data);
	fprintf(stderr, "OpenCL error (%s): %s\n", device_name.c_str(), err_info);
}

bool OpenCLDeviceBase::opencl_version_check()
{
	string error;
	if(!OpenCLInfo::platform_version_check(cpPlatform, &error)) {
		opencl_error(error);
		return false;
	}
	if(!OpenCLInfo::device_version_check(cdDevice, &error)) {
		opencl_error(error);
		return false;
	}
	return true;
}

string OpenCLDeviceBase::device_md5_hash(string kernel_custom_build_options)
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

bool OpenCLDeviceBase::load_kernels(const DeviceRequestedFeatures& requested_features)
{
	VLOG(2) << "Loading kernels for platform " << platform_name
	        << ", device " << device_name << ".";
	/* Verify if device was initialized. */
	if(!device_initialized) {
		fprintf(stderr, "OpenCL: failed to initialize device.\n");
		return false;
	}

	/* Verify we have right opencl version. */
	if(!opencl_version_check())
		return false;

	base_program = OpenCLProgram(this, "base", "kernel.cl", build_options_for_base_program(requested_features));
	base_program.add_kernel(ustring("convert_to_byte"));
	base_program.add_kernel(ustring("convert_to_half_float"));
	base_program.add_kernel(ustring("displace"));
	base_program.add_kernel(ustring("background"));
	base_program.add_kernel(ustring("bake"));
	base_program.add_kernel(ustring("zero_buffer"));

	denoising_program = OpenCLProgram(this, "denoising", "filter.cl", "");
	denoising_program.add_kernel(ustring("filter_divide_shadow"));
	denoising_program.add_kernel(ustring("filter_get_feature"));
	denoising_program.add_kernel(ustring("filter_detect_outliers"));
	denoising_program.add_kernel(ustring("filter_combine_halves"));
	denoising_program.add_kernel(ustring("filter_construct_transform"));
	denoising_program.add_kernel(ustring("filter_nlm_calc_difference"));
	denoising_program.add_kernel(ustring("filter_nlm_blur"));
	denoising_program.add_kernel(ustring("filter_nlm_calc_weight"));
	denoising_program.add_kernel(ustring("filter_nlm_update_output"));
	denoising_program.add_kernel(ustring("filter_nlm_normalize"));
	denoising_program.add_kernel(ustring("filter_nlm_construct_gramian"));
	denoising_program.add_kernel(ustring("filter_finalize"));

	vector<OpenCLProgram*> programs;
	programs.push_back(&base_program);
	programs.push_back(&denoising_program);
	/* Call actual class to fill the vector with its programs. */
	if(!load_kernels(requested_features, programs)) {
		return false;
	}

	/* Parallel compilation is supported by Cycles, but currently all OpenCL frameworks
	 * serialize the calls internally, so it's not much use right now.
	 * Note: When enabling parallel compilation, use_stdout in the OpenCLProgram constructor
	 * should be set to false as well. */
#if 0
	TaskPool task_pool;
	foreach(OpenCLProgram *program, programs) {
		task_pool.push(function_bind(&OpenCLProgram::load, program));
	}
	task_pool.wait_work();

	foreach(OpenCLProgram *program, programs) {
		VLOG(2) << program->get_log();
		if(!program->is_loaded()) {
			program->report_error();
			return false;
		}
	}
#else
	foreach(OpenCLProgram *program, programs) {
		program->load();
		if(!program->is_loaded()) {
			return false;
		}
	}
#endif

	return true;
}

void OpenCLDeviceBase::mem_alloc(device_memory& mem)
{
	if(mem.name) {
		VLOG(1) << "Buffer allocate: " << mem.name << ", "
			    << string_human_readable_number(mem.memory_size()) << " bytes. ("
			    << string_human_readable_size(mem.memory_size()) << ")";
	}

	size_t size = mem.memory_size();

	/* check there is enough memory available for the allocation */
	cl_ulong max_alloc_size = 0;
	clGetDeviceInfo(cdDevice, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &max_alloc_size, NULL);

	if(DebugFlags().opencl.mem_limit) {
		max_alloc_size = min(max_alloc_size,
		                     cl_ulong(DebugFlags().opencl.mem_limit - stats.mem_used));
	}

	if(size > max_alloc_size) {
		string error = "Scene too complex to fit in available memory.";
		if(mem.name != NULL) {
			error += string_printf(" (allocating buffer %s failed.)", mem.name);
		}
		set_error(error);

		return;
	}

	cl_mem_flags mem_flag;
	void *mem_ptr = NULL;

	if(mem.type == MEM_READ_ONLY || mem.type == MEM_TEXTURE)
		mem_flag = CL_MEM_READ_ONLY;
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

void OpenCLDeviceBase::mem_copy_to(device_memory& mem)
{
	if(mem.type == MEM_TEXTURE) {
		tex_free(mem);
		tex_alloc(mem);
	}
	else {
		if(!mem.device_pointer) {
			mem_alloc(mem);
		}

		/* this is blocking */
		size_t size = mem.memory_size();
		if(size != 0) {
			opencl_assert(clEnqueueWriteBuffer(cqCommandQueue,
			                                   CL_MEM_PTR(mem.device_pointer),
			                                   CL_TRUE,
			                                   0,
			                                   size,
			                                   mem.host_pointer,
			                                   0,
			                                   NULL, NULL));
		}
	}
}

void OpenCLDeviceBase::mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
{
	size_t offset = elem*y*w;
	size_t size = elem*w*h;
	assert(size != 0);
	opencl_assert(clEnqueueReadBuffer(cqCommandQueue,
	                                  CL_MEM_PTR(mem.device_pointer),
	                                  CL_TRUE,
	                                  offset,
	                                  size,
	                                  (uchar*)mem.host_pointer + offset,
	                                  0,
	                                  NULL, NULL));
}

void OpenCLDeviceBase::mem_zero_kernel(device_ptr mem, size_t size)
{
	cl_kernel ckZeroBuffer = base_program(ustring("zero_buffer"));

	size_t global_size[] = {1024, 1024};
	size_t num_threads = global_size[0] * global_size[1];

	cl_mem d_buffer = CL_MEM_PTR(mem);
	cl_ulong d_offset = 0;
	cl_ulong d_size = 0;

	while(d_offset < size) {
		d_size = std::min<cl_ulong>(num_threads*sizeof(float4), size - d_offset);

		kernel_set_args(ckZeroBuffer, 0, d_buffer, d_size, d_offset);

		ciErr = clEnqueueNDRangeKernel(cqCommandQueue,
		                               ckZeroBuffer,
		                               2,
		                               NULL,
		                               global_size,
		                               NULL,
		                               0,
		                               NULL,
		                               NULL);
		opencl_assert_err(ciErr, "clEnqueueNDRangeKernel");

		d_offset += d_size;
	}
}

void OpenCLDeviceBase::mem_zero(device_memory& mem)
{
	if(!mem.device_pointer) {
		mem_alloc(mem);
	}

	if(mem.device_pointer) {
		if(base_program.is_loaded()) {
			mem_zero_kernel(mem.device_pointer, mem.memory_size());
		}

		if(mem.host_pointer) {
			memset(mem.host_pointer, 0, mem.memory_size());
		}

		if(!base_program.is_loaded()) {
			void* zero = mem.host_pointer;

			if(!mem.host_pointer) {
				zero = util_aligned_malloc(mem.memory_size(), 16);
				memset(zero, 0, mem.memory_size());
			}

			opencl_assert(clEnqueueWriteBuffer(cqCommandQueue,
			                                   CL_MEM_PTR(mem.device_pointer),
			                                   CL_TRUE,
			                                   0,
			                                   mem.memory_size(),
			                                   zero,
			                                   0,
			                                   NULL, NULL));

			if(!mem.host_pointer) {
				util_aligned_free(zero);
			}
		}
	}
}

void OpenCLDeviceBase::mem_free(device_memory& mem)
{
	if(mem.type == MEM_TEXTURE) {
		tex_free(mem);
	}
	else {
		if(mem.device_pointer) {
			if(mem.device_pointer != null_mem) {
				opencl_assert(clReleaseMemObject(CL_MEM_PTR(mem.device_pointer)));
			}
			mem.device_pointer = 0;

			stats.mem_free(mem.device_size);
			mem.device_size = 0;
		}
	}
}

int OpenCLDeviceBase::mem_sub_ptr_alignment()
{
	return OpenCLInfo::mem_sub_ptr_alignment(cdDevice);
}

device_ptr OpenCLDeviceBase::mem_alloc_sub_ptr(device_memory& mem, int offset, int size)
{
	cl_mem_flags mem_flag;
	if(mem.type == MEM_READ_ONLY || mem.type == MEM_TEXTURE)
		mem_flag = CL_MEM_READ_ONLY;
	else
		mem_flag = CL_MEM_READ_WRITE;

	cl_buffer_region info;
	info.origin = mem.memory_elements_size(offset);
	info.size = mem.memory_elements_size(size);

	device_ptr sub_buf = (device_ptr) clCreateSubBuffer(CL_MEM_PTR(mem.device_pointer),
	                                                    mem_flag,
	                                                    CL_BUFFER_CREATE_TYPE_REGION,
	                                                    &info,
	                                                    &ciErr);
	opencl_assert_err(ciErr, "clCreateSubBuffer");
	return sub_buf;
}

void OpenCLDeviceBase::mem_free_sub_ptr(device_ptr device_pointer)
{
	if(device_pointer && device_pointer != null_mem) {
		opencl_assert(clReleaseMemObject(CL_MEM_PTR(device_pointer)));
	}
}

void OpenCLDeviceBase::const_copy_to(const char *name, void *host, size_t size)
{
	ConstMemMap::iterator i = const_mem_map.find(name);
	device_vector<uchar> *data;

	if(i == const_mem_map.end()) {
		data = new device_vector<uchar>(this, name, MEM_READ_ONLY);
		data->alloc(size);
		const_mem_map.insert(ConstMemMap::value_type(name, data));
	}
	else {
		data = i->second;
	}

	memcpy(data->data(), host, size);
	data->copy_to_device();
}

void OpenCLDeviceBase::tex_alloc(device_memory& mem)
{
	VLOG(1) << "Texture allocate: " << mem.name << ", "
	        << string_human_readable_number(mem.memory_size()) << " bytes. ("
	        << string_human_readable_size(mem.memory_size()) << ")";

	memory_manager.alloc(mem.name, mem);
	/* Set the pointer to non-null to keep code that inspects its value from thinking its unallocated. */
	mem.device_pointer = 1;
	textures[mem.name] = &mem;
	textures_need_update = true;
}

void OpenCLDeviceBase::tex_free(device_memory& mem)
{
	if(mem.device_pointer) {
		mem.device_pointer = 0;

		if(memory_manager.free(mem)) {
			textures_need_update = true;
		}

		foreach(TexturesMap::value_type& value, textures) {
			if(value.second == &mem) {
				textures.erase(value.first);
				break;
			}
		}
	}
}

size_t OpenCLDeviceBase::global_size_round_up(int group_size, int global_size)
{
	int r = global_size % group_size;
	return global_size + ((r == 0)? 0: group_size - r);
}

void OpenCLDeviceBase::enqueue_kernel(cl_kernel kernel, size_t w, size_t h, bool x_workgroups, size_t max_workgroup_size)
{
	size_t workgroup_size, max_work_items[3];

	clGetKernelWorkGroupInfo(kernel, cdDevice,
		CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &workgroup_size, NULL);
	clGetDeviceInfo(cdDevice,
		CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t)*3, max_work_items, NULL);

	if(max_workgroup_size > 0 && workgroup_size > max_workgroup_size) {
		workgroup_size = max_workgroup_size;
	}

	/* Try to divide evenly over 2 dimensions. */
	size_t local_size[2];
	if(x_workgroups) {
		local_size[0] = workgroup_size;
		local_size[1] = 1;
	}
	else {
		size_t sqrt_workgroup_size = max((size_t)sqrt((double)workgroup_size), 1);
		local_size[0] = local_size[1] = sqrt_workgroup_size;
	}

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

void OpenCLDeviceBase::set_kernel_arg_mem(cl_kernel kernel, cl_uint *narg, const char *name)
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

void OpenCLDeviceBase::set_kernel_arg_buffers(cl_kernel kernel, cl_uint *narg)
{
	flush_texture_buffers();

	memory_manager.set_kernel_arg_buffers(kernel, narg);
}

void OpenCLDeviceBase::flush_texture_buffers()
{
	if(!textures_need_update) {
		return;
	}
	textures_need_update = false;

	/* Setup slots for textures. */
	int num_slots = 0;

	vector<texture_slot_t> texture_slots;

#define KERNEL_TEX(type, name) \
	if(textures.find(#name) != textures.end()) { \
		texture_slots.push_back(texture_slot_t(#name, num_slots)); \
	} \
	num_slots++;
#include "kernel/kernel_textures.h"

	int num_data_slots = num_slots;

	foreach(TexturesMap::value_type& tex, textures) {
		string name = tex.first;

		if(string_startswith(name, "__tex_image")) {
			int pos = name.rfind("_");
			int id = atoi(name.data() + pos + 1);
			texture_slots.push_back(texture_slot_t(name,
				                                   num_data_slots + id));
			num_slots = max(num_slots, num_data_slots + id + 1);
		}
	}

	/* Realloc texture descriptors buffer. */
	memory_manager.free(texture_info);
	texture_info.resize(num_slots);
	memory_manager.alloc("texture_info", texture_info);

	/* Fill in descriptors */
	foreach(texture_slot_t& slot, texture_slots) {
		TextureInfo& info = texture_info[slot.slot];

		MemoryManager::BufferDescriptor desc = memory_manager.get_descriptor(slot.name);
		info.data = desc.offset;
		info.cl_buffer = desc.device_buffer;

		if(string_startswith(slot.name, "__tex_image")) {
			device_memory *mem = textures[slot.name];

			info.width = mem->data_width;
			info.height = mem->data_height;
			info.depth = mem->data_depth;

			info.interpolation = mem->interpolation;
			info.extension = mem->extension;
		}
	}

	/* Force write of descriptors. */
	memory_manager.free(texture_info);
	memory_manager.alloc("texture_info", texture_info);
}

void OpenCLDeviceBase::film_convert(DeviceTask& task, device_ptr buffer, device_ptr rgba_byte, device_ptr rgba_half)
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


	cl_kernel ckFilmConvertKernel = (rgba_byte)? base_program(ustring("convert_to_byte")): base_program(ustring("convert_to_half_float"));

	cl_uint start_arg_index =
		kernel_set_args(ckFilmConvertKernel,
		                0,
		                d_data,
		                d_rgba,
		                d_buffer);

	set_kernel_arg_buffers(ckFilmConvertKernel, &start_arg_index);

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

bool OpenCLDeviceBase::denoising_non_local_means(device_ptr image_ptr,
                                                 device_ptr guide_ptr,
                                                 device_ptr variance_ptr,
                                                 device_ptr out_ptr,
                                                 DenoisingTask *task)
{

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

	cl_mem weightAccum = CL_MEM_PTR(task->nlm_state.temporary_3_ptr);

	cl_mem difference = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, mem_size, NULL, &ciErr);
	opencl_assert_err(ciErr, "clCreateBuffer denoising_non_local_means");
	cl_mem blurDifference = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, mem_size, NULL, &ciErr);
	opencl_assert_err(ciErr, "clCreateBuffer denoising_non_local_means");

	cl_mem image_mem = CL_MEM_PTR(image_ptr);
	cl_mem guide_mem = CL_MEM_PTR(guide_ptr);
	cl_mem variance_mem = CL_MEM_PTR(variance_ptr);
	cl_mem out_mem = CL_MEM_PTR(out_ptr);

	mem_zero_kernel(task->nlm_state.temporary_3_ptr, sizeof(float)*w*h);
	mem_zero_kernel(out_ptr, sizeof(float)*w*h);

	cl_kernel ckNLMCalcDifference = denoising_program(ustring("filter_nlm_calc_difference"));
	cl_kernel ckNLMBlur           = denoising_program(ustring("filter_nlm_blur"));
	cl_kernel ckNLMCalcWeight     = denoising_program(ustring("filter_nlm_calc_weight"));
	cl_kernel ckNLMUpdateOutput   = denoising_program(ustring("filter_nlm_update_output"));
	cl_kernel ckNLMNormalize      = denoising_program(ustring("filter_nlm_normalize"));

	kernel_set_args(ckNLMCalcDifference, 0,
	                guide_mem,
	                variance_mem,
	                difference,
	                w, h, stride,
	                shift_stride,
	                r, 0, a, k_2);
	kernel_set_args(ckNLMBlur, 0,
	                difference,
	                blurDifference,
	                w, h, stride,
	                shift_stride,
	                r, f);
	kernel_set_args(ckNLMCalcWeight, 0,
	                blurDifference,
	                difference,
	                w, h, stride,
	                shift_stride,
	                r, f);
	kernel_set_args(ckNLMUpdateOutput, 0,
	                blurDifference,
	                image_mem,
	                out_mem,
	                weightAccum,
	                w, h, stride,
	                shift_stride,
	                r, f);

	enqueue_kernel(ckNLMCalcDifference, w*h, num_shifts, true);
	enqueue_kernel(ckNLMBlur,           w*h, num_shifts, true);
	enqueue_kernel(ckNLMCalcWeight,     w*h, num_shifts, true);
	enqueue_kernel(ckNLMBlur,           w*h, num_shifts, true);
	enqueue_kernel(ckNLMUpdateOutput,   w*h, num_shifts, true);

	opencl_assert(clReleaseMemObject(difference));
	opencl_assert(clReleaseMemObject(blurDifference));

	kernel_set_args(ckNLMNormalize, 0,
	                out_mem, weightAccum, w, h, stride);
	enqueue_kernel(ckNLMNormalize, w, h);

	return true;
}

bool OpenCLDeviceBase::denoising_construct_transform(DenoisingTask *task)
{
	cl_mem buffer_mem = CL_MEM_PTR(task->buffer.mem.device_pointer);
	cl_mem transform_mem = CL_MEM_PTR(task->storage.transform.device_pointer);
	cl_mem rank_mem = CL_MEM_PTR(task->storage.rank.device_pointer);

	cl_kernel ckFilterConstructTransform = denoising_program(ustring("filter_construct_transform"));

	kernel_set_args(ckFilterConstructTransform, 0,
	                buffer_mem,
	                transform_mem,
	                rank_mem,
	                task->filter_area,
	                task->rect,
	                task->buffer.pass_stride,
	                task->radius,
	                task->pca_threshold);

	enqueue_kernel(ckFilterConstructTransform,
	               task->storage.w,
	               task->storage.h,
	               256);

	return true;
}

bool OpenCLDeviceBase::denoising_reconstruct(device_ptr color_ptr,
                                             device_ptr color_variance_ptr,
                                             device_ptr output_ptr,
                                             DenoisingTask *task)
{
	mem_zero(task->storage.XtWX);
	mem_zero(task->storage.XtWY);

	cl_mem color_mem = CL_MEM_PTR(color_ptr);
	cl_mem color_variance_mem = CL_MEM_PTR(color_variance_ptr);
	cl_mem output_mem = CL_MEM_PTR(output_ptr);

	cl_mem buffer_mem = CL_MEM_PTR(task->buffer.mem.device_pointer);
	cl_mem transform_mem = CL_MEM_PTR(task->storage.transform.device_pointer);
	cl_mem rank_mem = CL_MEM_PTR(task->storage.rank.device_pointer);
	cl_mem XtWX_mem = CL_MEM_PTR(task->storage.XtWX.device_pointer);
	cl_mem XtWY_mem = CL_MEM_PTR(task->storage.XtWY.device_pointer);

	cl_kernel ckNLMCalcDifference   = denoising_program(ustring("filter_nlm_calc_difference"));
	cl_kernel ckNLMBlur             = denoising_program(ustring("filter_nlm_blur"));
	cl_kernel ckNLMCalcWeight       = denoising_program(ustring("filter_nlm_calc_weight"));
	cl_kernel ckNLMConstructGramian = denoising_program(ustring("filter_nlm_construct_gramian"));
	cl_kernel ckFinalize            = denoising_program(ustring("filter_finalize"));

	int w = task->reconstruction_state.source_w;
	int h = task->reconstruction_state.source_h;
	int stride = task->buffer.stride;

	int shift_stride = stride*h;
	int num_shifts = (2*task->radius + 1)*(2*task->radius + 1);
	int mem_size = sizeof(float)*shift_stride*num_shifts;

	cl_mem difference = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, mem_size, NULL, &ciErr);
	opencl_assert_err(ciErr, "clCreateBuffer denoising_reconstruct");
	cl_mem blurDifference = clCreateBuffer(cxContext, CL_MEM_READ_WRITE, mem_size, NULL, &ciErr);
	opencl_assert_err(ciErr, "clCreateBuffer denoising_reconstruct");

	kernel_set_args(ckNLMCalcDifference, 0,
	                color_mem,
	                color_variance_mem,
	                difference,
	                w, h, stride,
	                shift_stride,
	                task->radius,
	                task->buffer.pass_stride,
	                1.0f, task->nlm_k_2);
	kernel_set_args(ckNLMBlur, 0,
	                difference,
	                blurDifference,
	                w, h, stride,
	                shift_stride,
	                task->radius, 4);
	kernel_set_args(ckNLMCalcWeight, 0,
	                blurDifference,
	                difference,
	                w, h, stride,
	                shift_stride,
	                task->radius, 4);
	kernel_set_args(ckNLMConstructGramian, 0,
	                blurDifference,
	                buffer_mem,
	                transform_mem,
	                rank_mem,
	                XtWX_mem,
	                XtWY_mem,
	                task->reconstruction_state.filter_window,
	                w, h, stride,
	                shift_stride,
	                task->radius, 4,
	                task->buffer.pass_stride);

	enqueue_kernel(ckNLMCalcDifference,   w*h, num_shifts, true);
	enqueue_kernel(ckNLMBlur,             w*h, num_shifts, true);
	enqueue_kernel(ckNLMCalcWeight,       w*h, num_shifts, true);
	enqueue_kernel(ckNLMBlur,             w*h, num_shifts, true);
	enqueue_kernel(ckNLMConstructGramian, w*h, num_shifts, true, 256);

	opencl_assert(clReleaseMemObject(difference));
	opencl_assert(clReleaseMemObject(blurDifference));

	kernel_set_args(ckFinalize, 0,
	                output_mem,
	                rank_mem,
	                XtWX_mem,
	                XtWY_mem,
	                task->filter_area,
	                task->reconstruction_state.buffer_params,
	                task->render_buffer.samples);
	enqueue_kernel(ckFinalize, w, h);

	return true;
}

bool OpenCLDeviceBase::denoising_combine_halves(device_ptr a_ptr,
                                                device_ptr b_ptr,
                                                device_ptr mean_ptr,
                                                device_ptr variance_ptr,
                                                int r, int4 rect,
                                                DenoisingTask *task)
{
	cl_mem a_mem = CL_MEM_PTR(a_ptr);
	cl_mem b_mem = CL_MEM_PTR(b_ptr);
	cl_mem mean_mem = CL_MEM_PTR(mean_ptr);
	cl_mem variance_mem = CL_MEM_PTR(variance_ptr);

	cl_kernel ckFilterCombineHalves = denoising_program(ustring("filter_combine_halves"));

	kernel_set_args(ckFilterCombineHalves, 0,
	                mean_mem,
	                variance_mem,
	                a_mem,
	                b_mem,
	                rect,
	                r);
	enqueue_kernel(ckFilterCombineHalves,
	               task->rect.z-task->rect.x,
	               task->rect.w-task->rect.y);

	return true;
}

bool OpenCLDeviceBase::denoising_divide_shadow(device_ptr a_ptr,
                                               device_ptr b_ptr,
                                               device_ptr sample_variance_ptr,
                                               device_ptr sv_variance_ptr,
                                               device_ptr buffer_variance_ptr,
                                               DenoisingTask *task)
{
	cl_mem a_mem = CL_MEM_PTR(a_ptr);
	cl_mem b_mem = CL_MEM_PTR(b_ptr);
	cl_mem sample_variance_mem = CL_MEM_PTR(sample_variance_ptr);
	cl_mem sv_variance_mem = CL_MEM_PTR(sv_variance_ptr);
	cl_mem buffer_variance_mem = CL_MEM_PTR(buffer_variance_ptr);

	cl_mem tile_info_mem = CL_MEM_PTR(task->tile_info_mem.device_pointer);

	cl_kernel ckFilterDivideShadow = denoising_program(ustring("filter_divide_shadow"));

	int arg_ofs = kernel_set_args(ckFilterDivideShadow, 0,
	                              task->render_buffer.samples,
	                              tile_info_mem);
	cl_mem buffers[9];
	for(int i = 0; i < 9; i++) {
		buffers[i] = CL_MEM_PTR(task->tile_info->buffers[i]);
		arg_ofs += kernel_set_args(ckFilterDivideShadow, arg_ofs,
		                           buffers[i]);
	}
	kernel_set_args(ckFilterDivideShadow, arg_ofs,
	                a_mem,
	                b_mem,
	                sample_variance_mem,
	                sv_variance_mem,
	                buffer_variance_mem,
	                task->rect,
	                task->render_buffer.pass_stride,
	                task->render_buffer.offset);
	enqueue_kernel(ckFilterDivideShadow,
	               task->rect.z-task->rect.x,
	               task->rect.w-task->rect.y);

	return true;
}

bool OpenCLDeviceBase::denoising_get_feature(int mean_offset,
                                             int variance_offset,
                                             device_ptr mean_ptr,
                                             device_ptr variance_ptr,
                                             DenoisingTask *task)
{
	cl_mem mean_mem = CL_MEM_PTR(mean_ptr);
	cl_mem variance_mem = CL_MEM_PTR(variance_ptr);

	cl_mem tile_info_mem = CL_MEM_PTR(task->tile_info_mem.device_pointer);

	cl_kernel ckFilterGetFeature = denoising_program(ustring("filter_get_feature"));

	int arg_ofs = kernel_set_args(ckFilterGetFeature, 0,
	                              task->render_buffer.samples,
	                              tile_info_mem);
	cl_mem buffers[9];
	for(int i = 0; i < 9; i++) {
		buffers[i] = CL_MEM_PTR(task->tile_info->buffers[i]);
		arg_ofs += kernel_set_args(ckFilterGetFeature, arg_ofs,
		                           buffers[i]);
	}
	kernel_set_args(ckFilterGetFeature, arg_ofs,
	                mean_offset,
	                variance_offset,
	                mean_mem,
	                variance_mem,
	                task->rect,
	                task->render_buffer.pass_stride,
	                task->render_buffer.offset);
	enqueue_kernel(ckFilterGetFeature,
	               task->rect.z-task->rect.x,
	               task->rect.w-task->rect.y);

	return true;
}

bool OpenCLDeviceBase::denoising_detect_outliers(device_ptr image_ptr,
                                                 device_ptr variance_ptr,
                                                 device_ptr depth_ptr,
                                                 device_ptr output_ptr,
                                                 DenoisingTask *task)
{
	cl_mem image_mem = CL_MEM_PTR(image_ptr);
	cl_mem variance_mem = CL_MEM_PTR(variance_ptr);
	cl_mem depth_mem = CL_MEM_PTR(depth_ptr);
	cl_mem output_mem = CL_MEM_PTR(output_ptr);

	cl_kernel ckFilterDetectOutliers = denoising_program(ustring("filter_detect_outliers"));

	kernel_set_args(ckFilterDetectOutliers, 0,
	                image_mem,
	                variance_mem,
	                depth_mem,
	                output_mem,
	                task->rect,
	                task->buffer.pass_stride);
	enqueue_kernel(ckFilterDetectOutliers,
	               task->rect.z-task->rect.x,
	               task->rect.w-task->rect.y);

	return true;
}

void OpenCLDeviceBase::denoise(RenderTile &rtile, DenoisingTask& denoising)
{
	denoising.functions.construct_transform = function_bind(&OpenCLDeviceBase::denoising_construct_transform, this, &denoising);
	denoising.functions.reconstruct = function_bind(&OpenCLDeviceBase::denoising_reconstruct, this, _1, _2, _3, &denoising);
	denoising.functions.divide_shadow = function_bind(&OpenCLDeviceBase::denoising_divide_shadow, this, _1, _2, _3, _4, _5, &denoising);
	denoising.functions.non_local_means = function_bind(&OpenCLDeviceBase::denoising_non_local_means, this, _1, _2, _3, _4, &denoising);
	denoising.functions.combine_halves = function_bind(&OpenCLDeviceBase::denoising_combine_halves, this, _1, _2, _3, _4, _5, _6, &denoising);
	denoising.functions.get_feature = function_bind(&OpenCLDeviceBase::denoising_get_feature, this, _1, _2, _3, _4, &denoising);
	denoising.functions.detect_outliers = function_bind(&OpenCLDeviceBase::denoising_detect_outliers, this, _1, _2, _3, _4, &denoising);

	denoising.filter_area = make_int4(rtile.x, rtile.y, rtile.w, rtile.h);
	denoising.render_buffer.samples = rtile.sample;

	denoising.run_denoising(&rtile);
}

void OpenCLDeviceBase::shader(DeviceTask& task)
{
	/* cast arguments to cl types */
	cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
	cl_mem d_input = CL_MEM_PTR(task.shader_input);
	cl_mem d_output = CL_MEM_PTR(task.shader_output);
	cl_int d_shader_eval_type = task.shader_eval_type;
	cl_int d_shader_filter = task.shader_filter;
	cl_int d_shader_x = task.shader_x;
	cl_int d_shader_w = task.shader_w;
	cl_int d_offset = task.offset;

	cl_kernel kernel;

	if(task.shader_eval_type >= SHADER_EVAL_BAKE) {
		kernel = base_program(ustring("bake"));
	}
	else if(task.shader_eval_type == SHADER_EVAL_DISPLACE) {
		kernel = base_program(ustring("displace"));
	}
	else {
		kernel = base_program(ustring("background"));
	}

	cl_uint start_arg_index =
		kernel_set_args(kernel,
		                0,
		                d_data,
		                d_input,
		                d_output);

	set_kernel_arg_buffers(kernel, &start_arg_index);

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

string OpenCLDeviceBase::kernel_build_options(const string *debug_src)
{
	string build_options = "-cl-no-signed-zeros -cl-mad-enable ";

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
		if(OpenCLInfo::use_debug() && debug_src)
			build_options += "-g -s \"" + *debug_src + "\" ";
	}

	if(OpenCLInfo::use_debug())
		build_options += "-D__KERNEL_OPENCL_DEBUG__ ";

#ifdef WITH_CYCLES_DEBUG
	build_options += "-D__KERNEL_DEBUG__ ";
#endif

	return build_options;
}

/* TODO(sergey): In the future we can use variadic templates, once
 * C++0x is allowed. Should allow to clean this up a bit.
 */
int OpenCLDeviceBase::kernel_set_args(cl_kernel kernel,
                    int start_argument_index,
                    const ArgumentWrapper& arg1,
                    const ArgumentWrapper& arg2,
                    const ArgumentWrapper& arg3,
                    const ArgumentWrapper& arg4,
                    const ArgumentWrapper& arg5,
                    const ArgumentWrapper& arg6,
                    const ArgumentWrapper& arg7,
                    const ArgumentWrapper& arg8,
                    const ArgumentWrapper& arg9,
                    const ArgumentWrapper& arg10,
                    const ArgumentWrapper& arg11,
                    const ArgumentWrapper& arg12,
                    const ArgumentWrapper& arg13,
                    const ArgumentWrapper& arg14,
                    const ArgumentWrapper& arg15,
                    const ArgumentWrapper& arg16,
                    const ArgumentWrapper& arg17,
                    const ArgumentWrapper& arg18,
                    const ArgumentWrapper& arg19,
                    const ArgumentWrapper& arg20,
                    const ArgumentWrapper& arg21,
                    const ArgumentWrapper& arg22,
                    const ArgumentWrapper& arg23,
                    const ArgumentWrapper& arg24,
                    const ArgumentWrapper& arg25,
                    const ArgumentWrapper& arg26,
                    const ArgumentWrapper& arg27,
                    const ArgumentWrapper& arg28,
                    const ArgumentWrapper& arg29,
                    const ArgumentWrapper& arg30,
                    const ArgumentWrapper& arg31,
                    const ArgumentWrapper& arg32,
                    const ArgumentWrapper& arg33)
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

void OpenCLDeviceBase::release_kernel_safe(cl_kernel kernel)
{
	if(kernel) {
		clReleaseKernel(kernel);
	}
}

void OpenCLDeviceBase::release_mem_object_safe(cl_mem mem)
{
	if(mem != NULL) {
		clReleaseMemObject(mem);
	}
}

void OpenCLDeviceBase::release_program_safe(cl_program program)
{
	if(program) {
		clReleaseProgram(program);
	}
}

/* ** Those guys are for workign around some compiler-specific bugs ** */

cl_program OpenCLDeviceBase::load_cached_kernel(
        ustring key,
        thread_scoped_lock& cache_locker)
{
	return OpenCLCache::get_program(cpPlatform,
	                                cdDevice,
	                                key,
	                                cache_locker);
}

void OpenCLDeviceBase::store_cached_kernel(
        cl_program program,
        ustring key,
        thread_scoped_lock& cache_locker)
{
	OpenCLCache::store_program(cpPlatform,
	                           cdDevice,
	                           program,
	                           key,
	                           cache_locker);
}

string OpenCLDeviceBase::build_options_for_base_program(
        const DeviceRequestedFeatures& requested_features)
{
	/* TODO(sergey): By default we compile all features, meaning
	 * mega kernel is not getting feature-based optimizations.
	 *
	 * Ideally we need always compile kernel with as less features
	 * enabled as possible to keep performance at it's max.
	 */

	/* For now disable baking when not in use as this has major
	 * impact on kernel build times.
	 */
	if(!requested_features.use_baking) {
		return "-D__NO_BAKING__";
	}

	return "";
}

CCL_NAMESPACE_END

#endif

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

#include "device/device.h"
#include "device/device_intern.h"

#include "util/util_foreach.h"
#include "util/util_half.h"
#include "util/util_logging.h"
#include "util/util_math.h"
#include "util/util_opengl.h"
#include "util/util_time.h"
#include "util/util_system.h"
#include "util/util_types.h"
#include "util/util_vector.h"
#include "util/util_string.h"

CCL_NAMESPACE_BEGIN

bool Device::need_types_update = true;
bool Device::need_devices_update = true;
thread_mutex Device::device_mutex;
vector<DeviceType> Device::types;
vector<DeviceInfo> Device::devices;

/* Device Requested Features */

std::ostream& operator <<(std::ostream &os,
                          const DeviceRequestedFeatures& requested_features)
{
	os << "Experimental features: "
	   << (requested_features.experimental ? "On" : "Off") << std::endl;
	os << "Max nodes group: " << requested_features.max_nodes_group << std::endl;
	/* TODO(sergey): Decode bitflag into list of names. */
	os << "Nodes features: " << requested_features.nodes_features << std::endl;
	os << "Use Hair: "
	   << string_from_bool(requested_features.use_hair) << std::endl;
	os << "Use Object Motion: "
	   << string_from_bool(requested_features.use_object_motion) << std::endl;
	os << "Use Camera Motion: "
	   << string_from_bool(requested_features.use_camera_motion) << std::endl;
	os << "Use Baking: "
	   << string_from_bool(requested_features.use_baking) << std::endl;
	os << "Use Subsurface: "
	   << string_from_bool(requested_features.use_subsurface) << std::endl;
	os << "Use Volume: "
	   << string_from_bool(requested_features.use_volume) << std::endl;
	os << "Use Branched Integrator: "
	   << string_from_bool(requested_features.use_integrator_branched) << std::endl;
	os << "Use Patch Evaluation: "
	   << string_from_bool(requested_features.use_patch_evaluation) << std::endl;
	os << "Use Transparent Shadows: "
	   << string_from_bool(requested_features.use_transparent) << std::endl;
	os << "Use Principled BSDF: "
	   << string_from_bool(requested_features.use_principled) << std::endl;
	os << "Use Denoising: "
	   << string_from_bool(requested_features.use_denoising) << std::endl;
	return os;
}

/* Device */

Device::~Device()
{
	if(!background && vertex_buffer != 0) {
		glDeleteBuffers(1, &vertex_buffer);
	}
}

void Device::draw_pixels(device_memory& rgba, int y, int w, int h, int dx, int dy, int width, int height, bool transparent,
	const DeviceDrawParams &draw_params)
{
	assert(rgba.type == MEM_PIXELS);

	mem_copy_from(rgba, y, w, h, rgba.memory_elements_size(1));

	if(transparent) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}

	glColor3f(1.0f, 1.0f, 1.0f);

	if(rgba.data_type == TYPE_HALF) {
		/* for multi devices, this assumes the inefficient method that we allocate
		 * all pixels on the device even though we only render to a subset */
		GLhalf *host_pointer = (GLhalf*)rgba.host_pointer;
		float vbuffer[16], *basep;
		float *vp = NULL;

		host_pointer += 4*y*w;

		/* draw half float texture, GLSL shader for display transform assumed to be bound */
		GLuint texid;
		glGenTextures(1, &texid);
		glBindTexture(GL_TEXTURE_2D, texid);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, w, h, 0, GL_RGBA, GL_HALF_FLOAT, host_pointer);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glEnable(GL_TEXTURE_2D);

		if(draw_params.bind_display_space_shader_cb) {
			draw_params.bind_display_space_shader_cb();
		}

		if(GLEW_VERSION_1_5) {
			if(!vertex_buffer)
				glGenBuffers(1, &vertex_buffer);

			glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
			/* invalidate old contents - avoids stalling if buffer is still waiting in queue to be rendered */
			glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STREAM_DRAW);

			vp = (float *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

			basep = NULL;
		}
		else {
			basep = vbuffer;
			vp = vbuffer;
		}

		if(vp) {
			/* texture coordinate - vertex pair */
			vp[0] = 0.0f;
			vp[1] = 0.0f;
			vp[2] = dx;
			vp[3] = dy;

			vp[4] = 1.0f;
			vp[5] = 0.0f;
			vp[6] = (float)width + dx;
			vp[7] = dy;

			vp[8] = 1.0f;
			vp[9] = 1.0f;
			vp[10] = (float)width + dx;
			vp[11] = (float)height + dy;

			vp[12] = 0.0f;
			vp[13] = 1.0f;
			vp[14] = dx;
			vp[15] = (float)height + dy;

			if(vertex_buffer)
				glUnmapBuffer(GL_ARRAY_BUFFER);
		}

		glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), basep);
		glVertexPointer(2, GL_FLOAT, 4 * sizeof(float), ((char *)basep) + 2 * sizeof(float));

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);

		if(vertex_buffer) {
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		if(draw_params.unbind_display_space_shader_cb) {
			draw_params.unbind_display_space_shader_cb();
		}

		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
		glDeleteTextures(1, &texid);
	}
	else {
		/* fallback for old graphics cards that don't support GLSL, half float,
		 * and non-power-of-two textures */
		glPixelZoom((float)width/(float)w, (float)height/(float)h);
		glRasterPos2f(dx, dy);

		uint8_t *pixels = (uint8_t*)rgba.host_pointer;

		pixels += 4*y*w;

		glDrawPixels(w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		glRasterPos2f(0.0f, 0.0f);
		glPixelZoom(1.0f, 1.0f);
	}

	if(transparent)
		glDisable(GL_BLEND);
}

Device *Device::create(DeviceInfo& info, Stats &stats, Profiler &profiler, bool background)
{
	Device *device;

	switch(info.type) {
		case DEVICE_CPU:
			device = device_cpu_create(info, stats, profiler, background);
			break;
#ifdef WITH_CUDA
		case DEVICE_CUDA:
			if(device_cuda_init())
				device = device_cuda_create(info, stats, profiler, background);
			else
				device = NULL;
			break;
#endif
#ifdef WITH_MULTI
		case DEVICE_MULTI:
			device = device_multi_create(info, stats, profiler, background);
			break;
#endif
#ifdef WITH_NETWORK
		case DEVICE_NETWORK:
			device = device_network_create(info, stats, profiler, "127.0.0.1");
			break;
#endif
#ifdef WITH_OPENCL
		case DEVICE_OPENCL:
			if(device_opencl_init())
				device = device_opencl_create(info, stats, profiler, background);
			else
				device = NULL;
			break;
#endif
		default:
			return NULL;
	}

	return device;
}

DeviceType Device::type_from_string(const char *name)
{
	if(strcmp(name, "CPU") == 0)
		return DEVICE_CPU;
	else if(strcmp(name, "CUDA") == 0)
		return DEVICE_CUDA;
	else if(strcmp(name, "OPENCL") == 0)
		return DEVICE_OPENCL;
	else if(strcmp(name, "NETWORK") == 0)
		return DEVICE_NETWORK;
	else if(strcmp(name, "MULTI") == 0)
		return DEVICE_MULTI;

	return DEVICE_NONE;
}

string Device::string_from_type(DeviceType type)
{
	if(type == DEVICE_CPU)
		return "CPU";
	else if(type == DEVICE_CUDA)
		return "CUDA";
	else if(type == DEVICE_OPENCL)
		return "OPENCL";
	else if(type == DEVICE_NETWORK)
		return "NETWORK";
	else if(type == DEVICE_MULTI)
		return "MULTI";

	return "";
}

vector<DeviceType>& Device::available_types()
{
	thread_scoped_lock lock(device_mutex);
	if(need_types_update) {
		types.clear();
		types.push_back(DEVICE_CPU);
#ifdef WITH_CUDA
		if(device_cuda_init()) {
			types.push_back(DEVICE_CUDA);
		}
#endif
#ifdef WITH_OPENCL
		if(device_opencl_init()) {
			types.push_back(DEVICE_OPENCL);
		}
#endif
#ifdef WITH_NETWORK
		types.push_back(DEVICE_NETWORK);
#endif
		need_types_update = false;
	}
	return types;
}

vector<DeviceInfo>& Device::available_devices()
{
	thread_scoped_lock lock(device_mutex);
	if(need_devices_update) {
		devices.clear();
#ifdef WITH_OPENCL
		if(device_opencl_init()) {
			device_opencl_info(devices);
		}
#endif
#ifdef WITH_CUDA
		if(device_cuda_init()) {
			device_cuda_info(devices);
		}
#endif
		device_cpu_info(devices);
#ifdef WITH_NETWORK
		device_network_info(devices);
#endif
		need_devices_update = false;
	}
	return devices;
}

string Device::device_capabilities()
{
	string capabilities = "CPU device capabilities: ";
	capabilities += device_cpu_capabilities() + "\n";

#ifdef WITH_OPENCL
	if(device_opencl_init()) {
		capabilities += "\nOpenCL device capabilities:\n";
		capabilities += device_opencl_capabilities();
	}
#endif

#ifdef WITH_CUDA
	if(device_cuda_init()) {
		capabilities += "\nCUDA device capabilities:\n";
		capabilities += device_cuda_capabilities();
	}
#endif

	return capabilities;
}

DeviceInfo Device::get_multi_device(const vector<DeviceInfo>& subdevices, int threads, bool background)
{
	assert(subdevices.size() > 1);

	DeviceInfo info;
	info.type = DEVICE_MULTI;
	info.id = "MULTI";
	info.description = "Multi Device";
	info.num = 0;

	info.has_half_images = true;
	info.has_volume_decoupled = true;
	info.has_osl = true;
	info.has_profiling = true;

	foreach(const DeviceInfo &device, subdevices) {
		/* Ensure CPU device does not slow down GPU. */
		if(device.type == DEVICE_CPU && subdevices.size() > 1) {
			if(background) {
				int orig_cpu_threads = (threads)? threads: system_cpu_thread_count();
				int cpu_threads = max(orig_cpu_threads - (subdevices.size() - 1), 0);

				VLOG(1) << "CPU render threads reduced from "
						<< orig_cpu_threads << " to " << cpu_threads
						<< ", to dedicate to GPU.";

				if(cpu_threads >= 1) {
					DeviceInfo cpu_device = device;
					cpu_device.cpu_threads = cpu_threads;
					info.multi_devices.push_back(cpu_device);
				}
				else {
					continue;
				}
			}
			else {
				VLOG(1) << "CPU render threads disabled for interactive render.";
				continue;
			}
		}
		else {
			info.multi_devices.push_back(device);
		}

		/* Accumulate device info. */
		info.has_half_images &= device.has_half_images;
		info.has_volume_decoupled &= device.has_volume_decoupled;
		info.has_osl &= device.has_osl;
		info.has_profiling &= device.has_profiling;
	}

	return info;
}

void Device::tag_update()
{
	need_types_update = true;
	need_devices_update = true;
}

void Device::free_memory()
{
	need_types_update = true;
	need_devices_update = true;
	types.free_memory();
	devices.free_memory();
}

CCL_NAMESPACE_END

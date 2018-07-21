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

#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <stdlib.h>

#include "bvh/bvh_params.h"

#include "device/device_memory.h"
#include "device/device_task.h"

#include "util/util_list.h"
#include "util/util_stats.h"
#include "util/util_string.h"
#include "util/util_thread.h"
#include "util/util_texture.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Progress;
class RenderTile;

/* Device Types */

enum DeviceType {
	DEVICE_NONE,
	DEVICE_CPU,
	DEVICE_OPENCL,
	DEVICE_CUDA,
	DEVICE_NETWORK,
	DEVICE_MULTI
};

class DeviceInfo {
public:
	DeviceType type;
	string description;
	string id; /* used for user preferences, should stay fixed with changing hardware config */
	int num;
	bool display_device;            /* GPU is used as a display device. */
	bool advanced_shading;          /* Supports full shading system. */
	bool has_half_images;           /* Support half-float textures. */
	bool has_volume_decoupled;      /* Decoupled volume shading. */
	BVHLayoutMask bvh_layout_mask;  /* Bitmask of supported BVH layouts. */
	bool has_osl;                   /* Support Open Shading Language. */
	bool use_split_kernel;          /* Use split or mega kernel. */
	int cpu_threads;
	vector<DeviceInfo> multi_devices;

	DeviceInfo()
	{
		type = DEVICE_CPU;
		id = "CPU";
		num = 0;
		cpu_threads = 0;
		display_device = false;
		advanced_shading = true;
		has_half_images = false;
		has_volume_decoupled = false;
		bvh_layout_mask = BVH_LAYOUT_NONE;
		has_osl = false;
		use_split_kernel = false;
	}

	bool operator==(const DeviceInfo &info) {
		/* Multiple Devices with the same ID would be very bad. */
		assert(id != info.id || (type == info.type && num == info.num && description == info.description));
		return id == info.id;
	}
};

class DeviceRequestedFeatures {
public:
	/* Use experimental feature set. */
	bool experimental;

	/* Selective nodes compilation. */

	/* Identifier of a node group up to which all the nodes needs to be
	 * compiled in. Nodes from higher group indices will be ignores.
	 */
	int max_nodes_group;

	/* Features bitfield indicating which features from the requested group
	 * will be compiled in. Nodes which corresponds to features which are not
	 * in this bitfield will be ignored even if they're in the requested group.
	 */
	int nodes_features;

	/* BVH/sampling kernel features. */
	bool use_hair;
	bool use_object_motion;
	bool use_camera_motion;

	/* Denotes whether baking functionality is needed. */
	bool use_baking;

	/* Use subsurface scattering materials. */
	bool use_subsurface;

	/* Use volume materials. */
	bool use_volume;

	/* Use branched integrator. */
	bool use_integrator_branched;

	/* Use OpenSubdiv patch evaluation */
	bool use_patch_evaluation;

	/* Use Transparent shadows */
	bool use_transparent;

	/* Use various shadow tricks, such as shadow catcher. */
	bool use_shadow_tricks;

	/* Per-uber shader usage flags. */
	bool use_principled;

	/* Denoising features. */
	bool use_denoising;

	/* Use raytracing in shaders. */
	bool use_shader_raytrace;

	DeviceRequestedFeatures()
	{
		/* TODO(sergey): Find more meaningful defaults. */
		experimental = false;
		max_nodes_group = 0;
		nodes_features = 0;
		use_hair = false;
		use_object_motion = false;
		use_camera_motion = false;
		use_baking = false;
		use_subsurface = false;
		use_volume = false;
		use_integrator_branched = false;
		use_patch_evaluation = false;
		use_transparent = false;
		use_shadow_tricks = false;
		use_principled = false;
		use_denoising = false;
		use_shader_raytrace = false;
	}

	bool modified(const DeviceRequestedFeatures& requested_features)
	{
		return !(experimental == requested_features.experimental &&
		         max_nodes_group == requested_features.max_nodes_group &&
		         nodes_features == requested_features.nodes_features &&
		         use_hair == requested_features.use_hair &&
		         use_object_motion == requested_features.use_object_motion &&
		         use_camera_motion == requested_features.use_camera_motion &&
		         use_baking == requested_features.use_baking &&
		         use_subsurface == requested_features.use_subsurface &&
		         use_volume == requested_features.use_volume &&
		         use_integrator_branched == requested_features.use_integrator_branched &&
		         use_patch_evaluation == requested_features.use_patch_evaluation &&
		         use_transparent == requested_features.use_transparent &&
		         use_shadow_tricks == requested_features.use_shadow_tricks &&
		         use_principled == requested_features.use_principled &&
		         use_denoising == requested_features.use_denoising &&
		         use_shader_raytrace == requested_features.use_shader_raytrace);
	}

	/* Convert the requested features structure to a build options,
	 * which could then be passed to compilers.
	 */
	string get_build_options(void) const
	{
		string build_options = "";
		if(experimental) {
			build_options += "-D__KERNEL_EXPERIMENTAL__ ";
		}
		build_options += "-D__NODES_MAX_GROUP__=" +
			string_printf("%d", max_nodes_group);
		build_options += " -D__NODES_FEATURES__=" +
			string_printf("%d", nodes_features);
		if(!use_hair) {
			build_options += " -D__NO_HAIR__";
		}
		if(!use_object_motion) {
			build_options += " -D__NO_OBJECT_MOTION__";
		}
		if(!use_camera_motion) {
			build_options += " -D__NO_CAMERA_MOTION__";
		}
		if(!use_baking) {
			build_options += " -D__NO_BAKING__";
		}
		if(!use_volume) {
			build_options += " -D__NO_VOLUME__";
		}
		if(!use_subsurface) {
			build_options += " -D__NO_SUBSURFACE__";
		}
		if(!use_integrator_branched) {
			build_options += " -D__NO_BRANCHED_PATH__";
		}
		if(!use_patch_evaluation) {
			build_options += " -D__NO_PATCH_EVAL__";
		}
		if(!use_transparent && !use_volume) {
			build_options += " -D__NO_TRANSPARENT__";
		}
		if(!use_shadow_tricks) {
			build_options += " -D__NO_SHADOW_TRICKS__";
		}
		if(!use_principled) {
			build_options += " -D__NO_PRINCIPLED__";
		}
		if(!use_denoising) {
			build_options += " -D__NO_DENOISING__";
		}
		if(!use_shader_raytrace) {
			build_options += " -D__NO_SHADER_RAYTRACE__";
		}
		return build_options;
	}
};

std::ostream& operator <<(std::ostream &os,
                          const DeviceRequestedFeatures& requested_features);

/* Device */

struct DeviceDrawParams {
	function<void(void)> bind_display_space_shader_cb;
	function<void(void)> unbind_display_space_shader_cb;
};

class Device {
	friend class device_sub_ptr;
protected:
	enum {
		FALLBACK_SHADER_STATUS_NONE = 0,
		FALLBACK_SHADER_STATUS_ERROR,
		FALLBACK_SHADER_STATUS_SUCCESS,
	};

	Device(DeviceInfo& info_, Stats &stats_, bool background) : background(background),
	    vertex_buffer(0),
	    fallback_status(FALLBACK_SHADER_STATUS_NONE), fallback_shader_program(0),
	    info(info_), stats(stats_) {}

	bool background;
	string error_msg;

	/* used for real time display */
	unsigned int vertex_buffer;
	int fallback_status, fallback_shader_program;
	int image_texture_location, fullscreen_location;

	bool bind_fallback_display_space_shader(const float width, const float height);

	virtual device_ptr mem_alloc_sub_ptr(device_memory& /*mem*/, int /*offset*/, int /*size*/)
	{
		/* Only required for devices that implement denoising. */
		assert(false);
		return (device_ptr) 0;
	}
	virtual void mem_free_sub_ptr(device_ptr /*ptr*/) {};

public:
	virtual ~Device();

	/* info */
	DeviceInfo info;
	virtual const string& error_message() { return error_msg; }
	bool have_error() { return !error_message().empty(); }
	virtual void set_error(const string& error)
	{
		if(!have_error()) {
			error_msg = error;
		}
		fprintf(stderr, "%s\n", error.c_str());
		fflush(stderr);
	}
	virtual bool show_samples() const { return false; }

	/* statistics */
	Stats &stats;

	/* memory alignment */
	virtual int mem_sub_ptr_alignment() { return MIN_ALIGNMENT_CPU_DATA_TYPES; }

	/* constant memory */
	virtual void const_copy_to(const char *name, void *host, size_t size) = 0;

	/* open shading language, only for CPU device */
	virtual void *osl_memory() { return NULL; }

	/* load/compile kernels, must be called before adding tasks */
	virtual bool load_kernels(
	        const DeviceRequestedFeatures& /*requested_features*/)
	{ return true; }

	/* tasks */
	virtual int get_split_task_count(DeviceTask& task) = 0;
	virtual void task_add(DeviceTask& task) = 0;
	virtual void task_wait() = 0;
	virtual void task_cancel() = 0;

	/* opengl drawing */
	virtual void draw_pixels(device_memory& mem, int y,
	    int w, int h, int width, int height,
	    int dx, int dy, int dw, int dh,
	    bool transparent, const DeviceDrawParams &draw_params);

#ifdef WITH_NETWORK
	/* networking */
	void server_run();
#endif

	/* multi device */
	virtual void map_tile(Device * /*sub_device*/, RenderTile& /*tile*/) {}
	virtual int device_number(Device * /*sub_device*/) { return 0; }
	virtual void map_neighbor_tiles(Device * /*sub_device*/, RenderTile * /*tiles*/) {}
	virtual void unmap_neighbor_tiles(Device * /*sub_device*/, RenderTile * /*tiles*/) {}

	/* static */
	static Device *create(DeviceInfo& info, Stats &stats, bool background = true);

	static DeviceType type_from_string(const char *name);
	static string string_from_type(DeviceType type);
	static vector<DeviceType>& available_types();
	static vector<DeviceInfo>& available_devices();
	static string device_capabilities();
	static DeviceInfo get_multi_device(const vector<DeviceInfo>& subdevices,
	                                   int threads,
	                                   bool background);

	/* Tag devices lists for update. */
	static void tag_update();

	static void free_memory();

protected:
	/* Memory allocation, only accessed through device_memory. */
	friend class MultiDevice;
	friend class DeviceServer;
	friend class device_memory;

	virtual void mem_alloc(device_memory& mem) = 0;
	virtual void mem_copy_to(device_memory& mem) = 0;
	virtual void mem_copy_from(device_memory& mem,
		int y, int w, int h, int elem) = 0;
	virtual void mem_zero(device_memory& mem) = 0;
	virtual void mem_free(device_memory& mem) = 0;

private:
	/* Indicted whether device types and devices lists were initialized. */
	static bool need_types_update, need_devices_update;
	static thread_mutex device_mutex;
	static vector<DeviceType> types;
	static vector<DeviceInfo> devices;
};

CCL_NAMESPACE_END

#endif /* __DEVICE_H__ */

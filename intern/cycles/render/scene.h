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

#ifndef __SCENE_H__
#define __SCENE_H__

#include "image.h"

#include "device_memory.h"

#include "kernel_types.h"

#include "util_param.h"
#include "util_string.h"
#include "util_thread.h"
#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class AttributeRequestSet;
class Background;
class Camera;
class Device;
class Film;
class Filter;
class Integrator;
class Light;
class LightManager;
class Mesh;
class MeshManager;
class Object;
class ObjectManager;
class Shader;
class ShaderManager;
class Progress;

/* Scene Device Data */

class DeviceScene {
public:
	/* BVH */
	device_vector<float4> bvh_nodes;
	device_vector<uint> object_node;
	device_vector<float4> tri_woop;
	device_vector<uint> prim_visibility;
	device_vector<uint> prim_index;
	device_vector<uint> prim_object;

	/* mesh */
	device_vector<float4> tri_normal;
	device_vector<float4> tri_vnormal;
	device_vector<float4> tri_vindex;
	device_vector<float4> tri_verts;

	/* objects */
	device_vector<float4> objects;

	/* attributes */
	device_vector<uint4> attributes_map;
	device_vector<float> attributes_float;
	device_vector<float4> attributes_float3;

	/* lights */
	device_vector<float4> light_distribution;
	device_vector<float4> light_data;
	device_vector<float2> light_background_marginal_cdf;
	device_vector<float2> light_background_conditional_cdf;

	/* particles */
	device_vector<float4> particles;

	/* shaders */
	device_vector<uint4> svm_nodes;
	device_vector<uint> shader_flag;
	device_vector<uint> object_flag;

	/* filter */
	device_vector<float> filter_table;

	/* integrator */
	device_vector<uint> sobol_directions;

	/* images */
	device_vector<uchar4> tex_image[TEX_NUM_IMAGES];
	device_vector<float4> tex_float_image[TEX_NUM_FLOAT_IMAGES];

	/* opencl images */
	device_vector<uchar4> tex_image_packed;
	device_vector<uint4> tex_image_packed_info;

	KernelData data;
};

/* Scene Parameters */

class SceneParams {
public:
	enum { OSL, SVM } shadingsystem;
	enum BVHType { BVH_DYNAMIC, BVH_STATIC } bvh_type;
	bool use_bvh_cache;
	bool use_bvh_spatial_split;
	bool use_qbvh;

	SceneParams()
	{
		shadingsystem = SVM;
		bvh_type = BVH_DYNAMIC;
		use_bvh_cache = false;
		use_bvh_spatial_split = false;
#ifdef __QBVH__
		use_qbvh = true;
#else
		use_qbvh = false;
#endif
	}

	bool modified(const SceneParams& params)
	{ return !(shadingsystem == params.shadingsystem
		&& bvh_type == params.bvh_type
		&& use_bvh_cache == params.use_bvh_cache
		&& use_bvh_spatial_split == params.use_bvh_spatial_split
		&& use_qbvh == params.use_qbvh); }
};

/* Scene */

class Scene {
public:
	/* data */
	Camera *camera;
	Filter *filter;
	Film *film;
	Background *background;
	Integrator *integrator;

	/* data lists */
	vector<Object*> objects;
	vector<Mesh*> meshes;
	vector<Shader*> shaders;
	vector<Light*> lights;

	/* data managers */
	ImageManager *image_manager;
	LightManager *light_manager;
	ShaderManager *shader_manager;
	MeshManager *mesh_manager;
	ObjectManager *object_manager;

	/* default shaders */
	int default_surface;
	int default_light;
	int default_background;
	int default_holdout;
	int default_empty;

	/* device */
	Device *device;
	DeviceScene dscene;

	/* parameters */
	SceneParams params;

	/* mutex must be locked manually by callers */
	thread_mutex mutex;

	Scene(const SceneParams& params);
	~Scene();

	void device_update(Device *device, Progress& progress);

	bool need_global_attribute(AttributeStandard std);
	void need_global_attributes(AttributeRequestSet& attributes);

	enum MotionType { MOTION_NONE = 0, MOTION_PASS, MOTION_BLUR };
	MotionType need_motion();

	bool need_update();
	bool need_reset();
};

CCL_NAMESPACE_END

#endif /*  __SCENE_H__ */


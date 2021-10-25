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

#ifndef __SCENE_H__
#define __SCENE_H__

#include "render/image.h"
#include "render/shader.h"

#include "device/device_memory.h"

#include "util/util_param.h"
#include "util/util_string.h"
#include "util/util_system.h"
#include "util/util_texture.h"
#include "util/util_thread.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class AttributeRequestSet;
class Background;
class Camera;
class Device;
class DeviceInfo;
class Film;
class Integrator;
class Light;
class LightManager;
class LookupTables;
class Mesh;
class MeshManager;
class Object;
class ObjectManager;
class ParticleSystemManager;
class ParticleSystem;
class CurveSystemManager;
class Shader;
class ShaderManager;
class Progress;
class BakeManager;
class BakeData;

/* Scene Device Data */

class DeviceScene {
public:
	/* BVH */
	device_vector<float4> bvh_nodes;
	device_vector<float4> bvh_leaf_nodes;
	device_vector<uint> object_node;
	device_vector<uint> prim_tri_index;
	device_vector<float4> prim_tri_verts;
	device_vector<uint> prim_type;
	device_vector<uint> prim_visibility;
	device_vector<uint> prim_index;
	device_vector<uint> prim_object;
	device_vector<float2> prim_time;

	/* mesh */
	device_vector<uint> tri_shader;
	device_vector<float4> tri_vnormal;
	device_vector<uint4> tri_vindex;
	device_vector<uint> tri_patch;
	device_vector<float2> tri_patch_uv;

	device_vector<float4> curves;
	device_vector<float4> curve_keys;

	device_vector<uint> patches;

	/* objects */
	device_vector<float4> objects;
	device_vector<float4> objects_vector;

	/* attributes */
	device_vector<uint4> attributes_map;
	device_vector<float> attributes_float;
	device_vector<float4> attributes_float3;
	device_vector<uchar4> attributes_uchar4;

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

	/* lookup tables */
	device_vector<float> lookup_table;

	/* integrator */
	device_vector<uint> sobol_directions;

	/* cpu images */
	vector<device_vector<float4>* > tex_float4_image;
	vector<device_vector<uchar4>* > tex_byte4_image;
	vector<device_vector<half4>* > tex_half4_image;
	vector<device_vector<float>* > tex_float_image;
	vector<device_vector<uchar>* > tex_byte_image;
	vector<device_vector<half>* > tex_half_image;

	/* opencl images */
	device_vector<float4> tex_image_float4_packed;
	device_vector<uchar4> tex_image_byte4_packed;
	device_vector<float> tex_image_float_packed;
	device_vector<uchar> tex_image_byte_packed;
	device_vector<uint4> tex_image_packed_info;

	KernelData data;
};

/* Scene Parameters */

class SceneParams {
public:
	ShadingSystem shadingsystem;
	enum BVHType {
		BVH_DYNAMIC = 0,
		BVH_STATIC = 1,

		BVH_NUM_TYPES,
	} bvh_type;
	bool use_bvh_spatial_split;
	bool use_bvh_unaligned_nodes;
	int num_bvh_time_steps;
	bool use_qbvh;
	bool persistent_data;
	int texture_limit;

	SceneParams()
	{
		shadingsystem = SHADINGSYSTEM_SVM;
		bvh_type = BVH_DYNAMIC;
		use_bvh_spatial_split = false;
		use_bvh_unaligned_nodes = true;
		num_bvh_time_steps = 0;
		use_qbvh = false;
		persistent_data = false;
		texture_limit = 0;
	}

	bool modified(const SceneParams& params)
	{ return !(shadingsystem == params.shadingsystem
		&& bvh_type == params.bvh_type
		&& use_bvh_spatial_split == params.use_bvh_spatial_split
		&& use_bvh_unaligned_nodes == params.use_bvh_unaligned_nodes
		&& num_bvh_time_steps == params.num_bvh_time_steps
		&& use_qbvh == params.use_qbvh
		&& persistent_data == params.persistent_data
		&& texture_limit == params.texture_limit); }
};

/* Scene */

class Scene {
public:
	/* data */
	Camera *camera;
	LookupTables *lookup_tables;
	Film *film;
	Background *background;
	Integrator *integrator;

	/* data lists */
	vector<Object*> objects;
	vector<Mesh*> meshes;
	vector<Shader*> shaders;
	vector<Light*> lights;
	vector<ParticleSystem*> particle_systems;

	/* data managers */
	ImageManager *image_manager;
	LightManager *light_manager;
	ShaderManager *shader_manager;
	MeshManager *mesh_manager;
	ObjectManager *object_manager;
	ParticleSystemManager *particle_system_manager;
	CurveSystemManager *curve_system_manager;
	BakeManager *bake_manager;

	/* default shaders */
	Shader *default_surface;
	Shader *default_light;
	Shader *default_background;
	Shader *default_empty;

	/* device */
	Device *device;
	DeviceScene dscene;

	/* parameters */
	SceneParams params;

	/* mutex must be locked manually by callers */
	thread_mutex mutex;

	Scene(const SceneParams& params, const DeviceInfo& device_info);
	~Scene();

	void device_update(Device *device, Progress& progress);

	bool need_global_attribute(AttributeStandard std);
	void need_global_attributes(AttributeRequestSet& attributes);

	enum MotionType { MOTION_NONE = 0, MOTION_PASS, MOTION_BLUR };
	MotionType need_motion(bool advanced_shading = true);
	float motion_shutter_time();

	bool need_update();
	bool need_reset();

	void reset();
	void device_free();

protected:
	/* Check if some heavy data worth logging was updated.
	 * Mainly used to suppress extra annoying logging.
	 */
	bool need_data_update();

	void free_memory(bool final);
};

CCL_NAMESPACE_END

#endif /*  __SCENE_H__ */


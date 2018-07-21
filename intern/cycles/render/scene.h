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

#include "bvh/bvh_params.h"

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
	device_vector<int4> bvh_nodes;
	device_vector<int4> bvh_leaf_nodes;
	device_vector<int> object_node;
	device_vector<uint> prim_tri_index;
	device_vector<float4> prim_tri_verts;
	device_vector<int> prim_type;
	device_vector<uint> prim_visibility;
	device_vector<int> prim_index;
	device_vector<int> prim_object;
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
	device_vector<KernelObject> objects;
	device_vector<Transform> object_motion_pass;
	device_vector<DecomposedTransform> object_motion;
	device_vector<uint> object_flag;

	/* cameras */
	device_vector<DecomposedTransform> camera_motion;

	/* attributes */
	device_vector<uint4> attributes_map;
	device_vector<float> attributes_float;
	device_vector<float4> attributes_float3;
	device_vector<uchar4> attributes_uchar4;

	/* lights */
	device_vector<KernelLightDistribution> light_distribution;
	device_vector<KernelLight> lights;
	device_vector<float2> light_background_marginal_cdf;
	device_vector<float2> light_background_conditional_cdf;

	/* particles */
	device_vector<KernelParticle> particles;

	/* shaders */
	device_vector<int4> svm_nodes;
	device_vector<KernelShader> shaders;

	/* lookup tables */
	device_vector<float> lookup_table;

	/* integrator */
	device_vector<uint> sobol_directions;

	/* ies lights */
	device_vector<float> ies_lights;

	KernelData data;

	DeviceScene(Device *device);
};

/* Scene Parameters */

class SceneParams {
public:
	/* Type of BVH, in terms whether it is supported dynamic updates of meshes
	 * or whether modifying geometry requires full BVH rebuild.
	 */
	enum BVHType {
		/* BVH supports dynamic updates of geometry.
		 *
		 * Faster for updating BVH tree when doing modifications in viewport,
		 * but slower for rendering.
		 */
		BVH_DYNAMIC = 0,
		/* BVH tree is calculated for specific scene, updates in geometry
		 * requires full tree rebuild.
		 *
		 * Slower to update BVH tree when modifying objects in viewport, also
		 * slower to build final BVH tree but gives best possible render speed.
		 */
		BVH_STATIC = 1,

		BVH_NUM_TYPES,
	};

	ShadingSystem shadingsystem;

	/* Requested BVH layout.
	 *
	 * If it's not supported by the device, the widest one from supported ones
	 * will be used, but BVH wider than this one will never be used.
	 */
	BVHLayout bvh_layout;

	BVHType bvh_type;
	bool use_bvh_spatial_split;
	bool use_bvh_unaligned_nodes;
	int num_bvh_time_steps;

	bool persistent_data;
	int texture_limit;

	SceneParams()
	{
		shadingsystem = SHADINGSYSTEM_SVM;
		bvh_layout = BVH_LAYOUT_BVH2;
		bvh_type = BVH_DYNAMIC;
		use_bvh_spatial_split = false;
		use_bvh_unaligned_nodes = true;
		num_bvh_time_steps = 0;
		persistent_data = false;
		texture_limit = 0;
	}

	bool modified(const SceneParams& params)
	{ return !(shadingsystem == params.shadingsystem
		&& bvh_layout == params.bvh_layout
		&& bvh_type == params.bvh_type
		&& use_bvh_spatial_split == params.use_bvh_spatial_split
		&& use_bvh_unaligned_nodes == params.use_bvh_unaligned_nodes
		&& num_bvh_time_steps == params.num_bvh_time_steps
		&& persistent_data == params.persistent_data
		&& texture_limit == params.texture_limit); }
};

/* Scene */

class Scene {
public:
	/* data */
	Camera *camera;
	Camera *dicing_camera;
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

	Scene(const SceneParams& params, Device *device);
	~Scene();

	void device_update(Device *device, Progress& progress);

	bool need_global_attribute(AttributeStandard std);
	void need_global_attributes(AttributeRequestSet& attributes);

	enum MotionType { MOTION_NONE = 0, MOTION_PASS, MOTION_BLUR };
	MotionType need_motion();
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

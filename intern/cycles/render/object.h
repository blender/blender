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

#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "graph/node.h"
#include "render/scene.h"

#include "util/util_boundbox.h"
#include "util/util_param.h"
#include "util/util_transform.h"
#include "util/util_thread.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Mesh;
class ParticleSystem;
class Progress;
class Scene;
struct Transform;

/* Object */

class Object : public Node {
public:
	NODE_DECLARE

	Mesh *mesh;
	Transform tfm;
	BoundBox bounds;
	uint random_id;
	int pass_id;
	vector<ParamValue> attributes;
	uint visibility;
	MotionTransform motion;
	bool use_motion;
	bool hide_on_missing_motion;
	bool use_holdout;
	bool is_shadow_catcher;

	float3 dupli_generated;
	float2 dupli_uv;

	ParticleSystem *particle_system;
	int particle_index;
	
	Object();
	~Object();

	void tag_update(Scene *scene);

	void compute_bounds(bool motion_blur);
	void apply_transform(bool apply_to_motion);

	vector<float> motion_times();

	/* Check whether object is traceable and it worth adding it to
	 * kernel scene.
	 */
	bool is_traceable();
};

/* Object Manager */

class ObjectManager {
public:
	bool need_update;
	bool need_flags_update;

	ObjectManager();
	~ObjectManager();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update_transforms(Device *device,
	                              DeviceScene *dscene,
	                              Scene *scene,
	                              uint *object_flag,
	                              Progress& progress);

	void device_update_flags(Device *device,
	                         DeviceScene *dscene,
	                         Scene *scene,
	                         Progress& progress,
	                         bool bounds_valid = true);
	void device_update_patch_map_offsets(Device *device, DeviceScene *dscene, Scene *scene);

	void device_free(Device *device, DeviceScene *dscene);

	void tag_update(Scene *scene);

	void apply_static_transforms(DeviceScene *dscene, Scene *scene, uint *object_flag, Progress& progress);

protected:
	/* Global state of object transform update. */
	struct UpdateObejctTransformState {
		/* Global state used by device_update_object_transform().
		 * Common for both threaded and non-threaded update.
		 */

		/* Type of the motion required by the scene settings. */
		Scene::MotionType need_motion;

		/* Mapping from particle system to a index in packed particle array.
		 * Only used for read.
		 */
		map<ParticleSystem*, int> particle_offset;

		/* Mesh area.
		 * Used to avoid calculation of mesh area multiple times. Used for both
		 * read and write. Acquire surface_area_lock to keep it all thread safe.
		 */
		map<Mesh*, float> surface_area_map;

		/* Packed object arrays. Those will be filled in. */
		uint *object_flag;
		float4 *objects;
		float4 *objects_vector;

		/* Flags which will be synchronized to Integrator. */
		bool have_motion;
		bool have_curves;

		/* ** Scheduling queue. ** */

		Scene *scene;

		/* Some locks to keep everything thread-safe. */
		thread_spin_lock queue_lock;
		thread_spin_lock surface_area_lock;

		/* First unused object index in the queue. */
		int queue_start_object;
	};
	void device_update_object_transform(UpdateObejctTransformState *state,
	                                    Object *ob,
	                                    const int object_index);
	void device_update_object_transform_task(UpdateObejctTransformState *state);
	bool device_update_object_transform_pop_work(
	        UpdateObejctTransformState *state,
	        int *start_index,
	        int *num_objects);
};

CCL_NAMESPACE_END

#endif /* __OBJECT_H__ */


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

#include "util_boundbox.h"
#include "util_param.h"
#include "util_transform.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Mesh;
class ParticleSystem;
class Progress;
class Scene;
struct Transform;

/* Object */

class Object {
public:
	Mesh *mesh;
	Transform tfm;
	BoundBox bounds;
	ustring name;
	uint random_id;
	int pass_id;
	vector<ParamValue> attributes;
	uint visibility;
	MotionTransform motion;
	bool use_motion;
	bool use_holdout;

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
};

/* Object Manager */

class ObjectManager {
public:
	bool need_update;

	ObjectManager();
	~ObjectManager();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update_transforms(Device *device, DeviceScene *dscene, Scene *scene, uint *object_flag, Progress& progress);
	void device_update_flags(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	void tag_update(Scene *scene);

	void apply_static_transforms(DeviceScene *dscene, Scene *scene, uint *object_flag, Progress& progress);
};

CCL_NAMESPACE_END

#endif /* __OBJECT_H__ */


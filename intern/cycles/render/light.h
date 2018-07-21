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

#ifndef __LIGHT_H__
#define __LIGHT_H__

#include "kernel/kernel_types.h"

#include "graph/node.h"

#include "util/util_ies.h"
#include "util/util_thread.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Object;
class Progress;
class Scene;
class Shader;

class Light : public Node {
public:
	NODE_DECLARE;

	Light();

	LightType type;
	float3 co;

	float3 dir;
	float size;

	float3 axisu;
	float sizeu;
	float3 axisv;
	float sizev;
	bool round;

	Transform tfm;

	int map_resolution;

	float spot_angle;
	float spot_smooth;

	bool cast_shadow;
	bool use_mis;
	bool use_diffuse;
	bool use_glossy;
	bool use_transmission;
	bool use_scatter;

	bool is_portal;
	bool is_enabled;

	Shader *shader;
	int samples;
	int max_bounces;
	uint random_id;

	void tag_update(Scene *scene);

	/* Check whether the light has contribution the the scene. */
	bool has_contribution(Scene *scene);
};

class LightManager {
public:
	bool use_light_visibility;
	bool need_update;

	LightManager();
	~LightManager();

	/* IES texture management */
	int add_ies(ustring ies);
	int add_ies_from_file(ustring filename);
	void remove_ies(int slot);

	void device_update(Device *device,
	                   DeviceScene *dscene,
	                   Scene *scene,
	                   Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	void tag_update(Scene *scene);

	/* Check whether there is a background light. */
	bool has_background_light(Scene *scene);

protected:
	/* Optimization: disable light which is either unsupported or
	 * which doesn't contribute to the scene or which is only used for MIS
	 * and scene doesn't need MIS.
	 */
	void disable_ineffective_light(Device *device, Scene *scene);

	void device_update_points(Device *device,
	                          DeviceScene *dscene,
	                          Scene *scene);
	void device_update_distribution(Device *device,
	                                DeviceScene *dscene,
	                                Scene *scene,
	                                Progress& progress);
	void device_update_background(Device *device,
	                              DeviceScene *dscene,
	                              Scene *scene,
	                              Progress& progress);
	void device_update_ies(DeviceScene *dscene);

	/* Check whether light manager can use the object as a light-emissive. */
	bool object_usable_as_light(Object *object);

	struct IESSlot {
		IESFile ies;
		uint hash;
		int users;
	};

	vector<IESSlot*> ies_slots;
	thread_mutex ies_mutex;
};

CCL_NAMESPACE_END

#endif /* __LIGHT_H__ */

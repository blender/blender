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
 * limitations under the License
 */

#ifndef __INTEGRATOR_H__
#define __INTEGRATOR_H__

#include "kernel_types.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;

class Integrator {
public:
	int min_bounce;
	int max_bounce;

	int max_diffuse_bounce;
	int max_glossy_bounce;
	int max_transmission_bounce;
	int max_volume_bounce;

	int transparent_min_bounce;
	int transparent_max_bounce;
	bool transparent_shadows;

	int volume_homogeneous_sampling;
	int volume_max_steps;
	float volume_step_size;

	bool caustics_reflective;
	bool caustics_refractive;
	float filter_glossy;

	int seed;
	int layer_flag;

	float sample_clamp_direct;
	float sample_clamp_indirect;
	bool motion_blur;

	int aa_samples;
	int diffuse_samples;
	int glossy_samples;
	int transmission_samples;
	int ao_samples;
	int mesh_light_samples;
	int subsurface_samples;
	int volume_samples;
	bool sample_all_lights_direct;
	bool sample_all_lights_indirect;

	enum Method {
		BRANCHED_PATH = 0,
		PATH = 1
	};
	
	Method method;

	SamplingPattern sampling_pattern;

	bool need_update;

	Integrator();
	~Integrator();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene);
	void device_free(Device *device, DeviceScene *dscene);

	bool modified(const Integrator& integrator);
	void tag_update(Scene *scene);
};

CCL_NAMESPACE_END

#endif /* __INTEGRATOR_H__ */


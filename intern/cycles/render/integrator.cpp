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

#include "device.h"
#include "integrator.h"
#include "light.h"
#include "scene.h"
#include "sobol.h"

#include "util_foreach.h"
#include "util_hash.h"

CCL_NAMESPACE_BEGIN

Integrator::Integrator()
{
	min_bounce = 2;
	max_bounce = 7;

	max_diffuse_bounce = max_bounce;
	max_glossy_bounce = max_bounce;
	max_transmission_bounce = max_bounce;
	max_volume_bounce = max_bounce;
	probalistic_termination = true;

	transparent_min_bounce = min_bounce;
	transparent_max_bounce = max_bounce;
	transparent_probalistic = true;
	transparent_shadows = false;

	volume_homogeneous_sampling = 0;
	volume_max_steps = 1024;
	volume_step_size = 0.1f;

	no_caustics = false;
	filter_glossy = 0.0f;
	seed = 0;
	layer_flag = ~0;
	sample_clamp_direct = 0.0f;
	sample_clamp_indirect = 0.0f;
	motion_blur = false;

	aa_samples = 0;
	diffuse_samples = 1;
	glossy_samples = 1;
	transmission_samples = 1;
	ao_samples = 1;
	mesh_light_samples = 1;
	subsurface_samples = 1;
	volume_samples = 1;
	method = PATH;

	sampling_pattern = SAMPLING_PATTERN_SOBOL;

	need_update = true;
}

Integrator::~Integrator()
{
}

void Integrator::device_update(Device *device, DeviceScene *dscene, Scene *scene)
{
	if(!need_update)
		return;

	device_free(device, dscene);

	KernelIntegrator *kintegrator = &dscene->data.integrator;

	/* integrator parameters */
	kintegrator->max_bounce = max_bounce + 1;
	if(probalistic_termination)
		kintegrator->min_bounce = min_bounce + 1;
	else
		kintegrator->min_bounce = kintegrator->max_bounce;

	kintegrator->max_diffuse_bounce = max_diffuse_bounce + 1;
	kintegrator->max_glossy_bounce = max_glossy_bounce + 1;
	kintegrator->max_transmission_bounce = max_transmission_bounce + 1;

	if(kintegrator->use_volumes)
		kintegrator->max_volume_bounce = max_volume_bounce + 1;
	else
		kintegrator->max_volume_bounce = 1;

	kintegrator->transparent_max_bounce = transparent_max_bounce + 1;
	if(transparent_probalistic)
		kintegrator->transparent_min_bounce = transparent_min_bounce + 1;
	else
		kintegrator->transparent_min_bounce = kintegrator->transparent_max_bounce;

	kintegrator->transparent_shadows = transparent_shadows;

	kintegrator->volume_homogeneous_sampling = volume_homogeneous_sampling;
	kintegrator->volume_max_steps = volume_max_steps;
	kintegrator->volume_step_size = volume_step_size;

	kintegrator->no_caustics = no_caustics;
	kintegrator->filter_glossy = (filter_glossy == 0.0f)? FLT_MAX: 1.0f/filter_glossy;

	kintegrator->seed = hash_int(seed);
	kintegrator->layer_flag = layer_flag << PATH_RAY_LAYER_SHIFT;

	kintegrator->use_ambient_occlusion =
		((dscene->data.film.pass_flag & PASS_AO) || dscene->data.background.ao_factor != 0.0f);
	
	kintegrator->sample_clamp_direct = (sample_clamp_direct == 0.0f)? FLT_MAX: sample_clamp_direct*3.0f;
	kintegrator->sample_clamp_indirect = (sample_clamp_indirect == 0.0f)? FLT_MAX: sample_clamp_indirect*3.0f;

	kintegrator->branched = (method == BRANCHED_PATH);
	kintegrator->diffuse_samples = diffuse_samples;
	kintegrator->glossy_samples = glossy_samples;
	kintegrator->transmission_samples = transmission_samples;
	kintegrator->ao_samples = ao_samples;
	kintegrator->mesh_light_samples = mesh_light_samples;
	kintegrator->subsurface_samples = subsurface_samples;
	kintegrator->volume_samples = volume_samples;
	kintegrator->sample_all_lights_direct = sample_all_lights_direct;
	kintegrator->sample_all_lights_indirect = sample_all_lights_indirect;

	kintegrator->sampling_pattern = sampling_pattern;
	kintegrator->aa_samples = aa_samples;

	/* sobol directions table */
	int max_samples = 1;

	if(method == BRANCHED_PATH) {
		foreach(Light *light, scene->lights)
			max_samples = max(max_samples, light->samples);

		max_samples = max(max_samples, max(diffuse_samples, max(glossy_samples, transmission_samples)));
		max_samples = max(max_samples, max(ao_samples, max(mesh_light_samples, subsurface_samples)));
		max_samples = max(max_samples, volume_samples);
	}

	max_samples *= (max_bounce + transparent_max_bounce + 3);

	int dimensions = PRNG_BASE_NUM + max_samples*PRNG_BOUNCE_NUM;
	dimensions = min(dimensions, SOBOL_MAX_DIMENSIONS);

	uint *directions = dscene->sobol_directions.resize(SOBOL_BITS*dimensions);

	sobol_generate_direction_vectors((uint(*)[SOBOL_BITS])directions, dimensions);

	device->tex_alloc("__sobol_directions", dscene->sobol_directions);

	need_update = false;
}

void Integrator::device_free(Device *device, DeviceScene *dscene)
{
	device->tex_free(dscene->sobol_directions);
	dscene->sobol_directions.clear();
}

bool Integrator::modified(const Integrator& integrator)
{
	return !(min_bounce == integrator.min_bounce &&
		max_bounce == integrator.max_bounce &&
		max_diffuse_bounce == integrator.max_diffuse_bounce &&
		max_glossy_bounce == integrator.max_glossy_bounce &&
		max_transmission_bounce == integrator.max_transmission_bounce &&
		max_volume_bounce == integrator.max_volume_bounce &&
		probalistic_termination == integrator.probalistic_termination &&
		transparent_min_bounce == integrator.transparent_min_bounce &&
		transparent_max_bounce == integrator.transparent_max_bounce &&
		transparent_probalistic == integrator.transparent_probalistic &&
		transparent_shadows == integrator.transparent_shadows &&
		volume_homogeneous_sampling == integrator.volume_homogeneous_sampling &&
		volume_max_steps == integrator.volume_max_steps &&
		volume_step_size == integrator.volume_step_size &&
		no_caustics == integrator.no_caustics &&
		filter_glossy == integrator.filter_glossy &&
		layer_flag == integrator.layer_flag &&
		seed == integrator.seed &&
		sample_clamp_direct == integrator.sample_clamp_direct &&
		sample_clamp_indirect == integrator.sample_clamp_indirect &&
		method == integrator.method &&
		aa_samples == integrator.aa_samples &&
		diffuse_samples == integrator.diffuse_samples &&
		glossy_samples == integrator.glossy_samples &&
		transmission_samples == integrator.transmission_samples &&
		ao_samples == integrator.ao_samples &&
		mesh_light_samples == integrator.mesh_light_samples &&
		subsurface_samples == integrator.subsurface_samples &&
		volume_samples == integrator.volume_samples &&
		motion_blur == integrator.motion_blur &&
		sampling_pattern == integrator.sampling_pattern &&
		sample_all_lights_direct == integrator.sample_all_lights_direct &&
		sample_all_lights_indirect == integrator.sample_all_lights_indirect);
}

void Integrator::tag_update(Scene *scene)
{
	need_update = true;
}

CCL_NAMESPACE_END


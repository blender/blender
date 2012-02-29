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

#include "device.h"
#include "integrator.h"
#include "scene.h"
#include "sobol.h"

#include "util_hash.h"

CCL_NAMESPACE_BEGIN

Integrator::Integrator()
{
	min_bounce = 2;
	max_bounce = 7;

	max_diffuse_bounce = max_bounce;
	max_glossy_bounce = max_bounce;
	max_transmission_bounce = max_bounce;
	probalistic_termination = true;

	transparent_min_bounce = min_bounce;
	transparent_max_bounce = max_bounce;
	transparent_probalistic = true;
	transparent_shadows = false;

	no_caustics = false;
	seed = 0;
	layer_flag = ~0;

	need_update = true;
}

Integrator::~Integrator()
{
}

void Integrator::device_update(Device *device, DeviceScene *dscene)
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

	kintegrator->transparent_max_bounce = transparent_max_bounce + 1;
	if(transparent_probalistic)
		kintegrator->transparent_min_bounce = transparent_min_bounce + 1;
	else
		kintegrator->transparent_min_bounce = kintegrator->transparent_max_bounce;

	kintegrator->transparent_shadows = transparent_shadows;

	kintegrator->no_caustics = no_caustics;
	kintegrator->seed = hash_int(seed);
	kintegrator->layer_flag = layer_flag << PATH_RAY_LAYER_SHIFT;

	kintegrator->use_ambient_occlusion =
		((dscene->data.film.pass_flag & PASS_AO) || dscene->data.background.ao_factor != 0.0f);

	/* sobol directions table */
	int dimensions = PRNG_BASE_NUM + (max_bounce + transparent_max_bounce + 2)*PRNG_BOUNCE_NUM;
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
		probalistic_termination == integrator.probalistic_termination &&
		transparent_min_bounce == integrator.transparent_min_bounce &&
		transparent_max_bounce == integrator.transparent_max_bounce &&
		transparent_probalistic == integrator.transparent_probalistic &&
		transparent_shadows == integrator.transparent_shadows &&
		no_caustics == integrator.no_caustics &&
		layer_flag == integrator.layer_flag &&
		seed == integrator.seed);
}

void Integrator::tag_update(Scene *scene)
{
	need_update = true;
}

CCL_NAMESPACE_END


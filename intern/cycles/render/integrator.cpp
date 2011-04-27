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

CCL_NAMESPACE_BEGIN

Integrator::Integrator()
{
	minbounce = 2;
	maxbounce = 7;
	no_caustics = false;
	blur_caustics = 0.0f;

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
	kintegrator->minbounce = minbounce + 1;
	kintegrator->maxbounce = maxbounce + 1;
	kintegrator->no_caustics = no_caustics;
	kintegrator->blur_caustics = blur_caustics;

	/* sobol directions table */
	int dimensions = PRNG_BASE_NUM + (maxbounce + 2)*PRNG_BOUNCE_NUM;
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
	return !(minbounce == integrator.minbounce &&
		maxbounce == integrator.maxbounce &&
		no_caustics == integrator.no_caustics &&
		blur_caustics == integrator.blur_caustics);
}

void Integrator::tag_update(Scene *scene)
{
	need_update = true;
}

CCL_NAMESPACE_END


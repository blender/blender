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

#include "background.h"
#include "device.h"
#include "integrator.h"
#include "graph.h"
#include "nodes.h"
#include "scene.h"
#include "shader.h"

#include "util_foreach.h"
#include "util_math.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

Background::Background()
{
	ao_factor = 0.0f;
	ao_distance = FLT_MAX;

	use = true;

	transparent = false;
	need_update = true;
}

Background::~Background()
{
}

void Background::device_update(Device *device, DeviceScene *dscene, Scene *scene)
{
	if(!need_update)
		return;
	
	device_free(device, dscene);

	/* set shader index and transparent option */
	KernelBackground *kbackground = &dscene->data.background;

	kbackground->ao_factor = ao_factor;
	kbackground->ao_distance = ao_distance;

	kbackground->transparent = transparent;
	if(use)
		kbackground->shader = scene->shader_manager->get_shader_id(scene->default_background);
	else
		kbackground->shader = scene->shader_manager->get_shader_id(scene->default_empty);

	need_update = false;
}

void Background::device_free(Device *device, DeviceScene *dscene)
{
}

bool Background::modified(const Background& background)
{
	return !(transparent == background.transparent &&
		use == background.use &&
		ao_factor == background.ao_factor &&
		ao_distance == background.ao_distance);
}

void Background::tag_update(Scene *scene)
{
	scene->integrator->tag_update(scene);
	need_update = true;
}

CCL_NAMESPACE_END


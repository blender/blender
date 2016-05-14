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

	use_shader = true;
	use_ao = false;

	visibility = PATH_RAY_ALL_VISIBILITY;
	shader = NULL;

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

	Shader *bg_shader = shader;

	if(use_shader) {
		if(!bg_shader)
			bg_shader = scene->default_background;
	}
	else
		bg_shader = scene->default_empty;

	/* set shader index and transparent option */
	KernelBackground *kbackground = &dscene->data.background;

	if(use_ao) {
		kbackground->ao_factor = ao_factor;
		kbackground->ao_distance = ao_distance;
	}
	else {
		kbackground->ao_factor = 0.0f;
		kbackground->ao_distance = FLT_MAX;
	}

	kbackground->transparent = transparent;
	kbackground->surface_shader = scene->shader_manager->get_shader_id(bg_shader);

	if(bg_shader->has_volume)
		kbackground->volume_shader = kbackground->surface_shader;
	else
		kbackground->volume_shader = SHADER_NONE;

	/* No background node, make world shader invisible to all rays, to skip evaluation in kernel. */
	if(bg_shader->graph->nodes.size() <= 1) {
		kbackground->surface_shader |= SHADER_EXCLUDE_ANY;
	}
	/* Background present, check visibilities */
	else {
		if(!(visibility & PATH_RAY_DIFFUSE))
			kbackground->surface_shader |= SHADER_EXCLUDE_DIFFUSE;
		if(!(visibility & PATH_RAY_GLOSSY))
			kbackground->surface_shader |= SHADER_EXCLUDE_GLOSSY;
		if(!(visibility & PATH_RAY_TRANSMIT))
			kbackground->surface_shader |= SHADER_EXCLUDE_TRANSMIT;
		if(!(visibility & PATH_RAY_VOLUME_SCATTER))
			kbackground->surface_shader |= SHADER_EXCLUDE_SCATTER;
		if(!(visibility & PATH_RAY_CAMERA))
			kbackground->surface_shader |= SHADER_EXCLUDE_CAMERA;
	}

	need_update = false;
}

void Background::device_free(Device * /*device*/, DeviceScene * /*dscene*/)
{
}

bool Background::modified(const Background& background)
{
	return !(transparent == background.transparent &&
		use_shader == background.use_shader &&
		use_ao == background.use_ao &&
		ao_factor == background.ao_factor &&
		ao_distance == background.ao_distance &&
		visibility == background.visibility);
}

void Background::tag_update(Scene *scene)
{
	scene->integrator->tag_update(scene);
	need_update = true;
}

CCL_NAMESPACE_END


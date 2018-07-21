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

#include "render/background.h"
#include "device/device.h"
#include "render/integrator.h"
#include "render/graph.h"
#include "render/nodes.h"
#include "render/scene.h"
#include "render/shader.h"

#include "util/util_foreach.h"
#include "util/util_math.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

NODE_DEFINE(Background)
{
	NodeType* type = NodeType::add("background", create);

	SOCKET_FLOAT(ao_factor, "AO Factor", 0.0f);
	SOCKET_FLOAT(ao_distance, "AO Distance", FLT_MAX);

	SOCKET_BOOLEAN(use_shader, "Use Shader", true);
	SOCKET_BOOLEAN(use_ao, "Use AO", false);
	SOCKET_UINT(visibility, "Visibility", PATH_RAY_ALL_VISIBILITY);

	SOCKET_BOOLEAN(transparent, "Transparent", false);
	SOCKET_BOOLEAN(transparent_glass, "Transparent Glass", false);
	SOCKET_FLOAT(transparent_roughness_threshold, "Transparent Roughness Threshold", 0.0f);

	SOCKET_NODE(shader, "Shader", &Shader::node_type);

	return type;
}

Background::Background()
: Node(node_type)
{
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

	kbackground->ao_factor = (use_ao)? ao_factor: 0.0f;
	kbackground->ao_bounces_factor = ao_factor;
	kbackground->ao_distance = ao_distance;

	kbackground->transparent = transparent;
	kbackground->surface_shader = scene->shader_manager->get_shader_id(bg_shader);

	if(transparent && transparent_glass) {
		/* Square twice, once for principled BSDF convention, and once for
		 * faster comparison in kernel with anisotropic roughness. */
		kbackground->transparent_roughness_squared_threshold = sqr(sqr(transparent_roughness_threshold));
	}
	else {
		kbackground->transparent_roughness_squared_threshold = -1.0f;
	}

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
	return !Node::equals(background);
}

void Background::tag_update(Scene *scene)
{
	scene->integrator->tag_update(scene);
	need_update = true;
}

CCL_NAMESPACE_END

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
#include "film.h"
#include "../render/filter.h"
#include "graph.h"
#include "integrator.h"
#include "light.h"
#include "mesh.h"
#include "nodes.h"
#include "object.h"
#include "scene.h"
#include "shader.h"

#include "device.h"

#include "blender_sync.h"
#include "blender_util.h"

#include "util_debug.h"
#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

/* Constructor */

BlenderSync::BlenderSync(BL::BlendData b_data_, BL::Scene b_scene_, Scene *scene_, bool preview_)
: b_data(b_data_), b_scene(b_scene_),
  shader_map(&scene_->shaders),
  object_map(&scene_->objects),
  mesh_map(&scene_->meshes),
  light_map(&scene_->lights),
  world_map(NULL),
  world_recalc(false)
{
	scene = scene_;
	preview = preview_;
}

BlenderSync::~BlenderSync()
{
}

/* Sync */

bool BlenderSync::sync_recalc()
{
	/* sync recalc flags from blender to cycles. actual update is done separate,
	   so we can do it later on if doing it immediate is not suitable */

	BL::BlendData::materials_iterator b_mat;

	for(b_data.materials.begin(b_mat); b_mat != b_data.materials.end(); ++b_mat)
		if(b_mat->recalc())
			shader_map.set_recalc(*b_mat);

	BL::BlendData::lamps_iterator b_lamp;

	for(b_data.lamps.begin(b_lamp); b_lamp != b_data.lamps.end(); ++b_lamp)
		if(b_lamp->recalc())
			shader_map.set_recalc(*b_lamp);

	BL::BlendData::objects_iterator b_ob;

	for(b_data.objects.begin(b_ob); b_ob != b_data.objects.end(); ++b_ob) {
		if(b_ob->recalc()) {
			object_map.set_recalc(*b_ob);
			light_map.set_recalc(*b_ob);
		}
		if(object_is_mesh(*b_ob)) {
			if(b_ob->recalc_data() || b_ob->data().recalc()) {
				BL::ID key = object_is_modified(*b_ob)? *b_ob: b_ob->data();
				mesh_map.set_recalc(key);
			}
		}
	}

	BL::BlendData::meshes_iterator b_mesh;

	for(b_data.meshes.begin(b_mesh); b_mesh != b_data.meshes.end(); ++b_mesh)
		if(b_mesh->recalc())
			mesh_map.set_recalc(*b_mesh);

	BL::BlendData::worlds_iterator b_world;

	for(b_data.worlds.begin(b_world); b_world != b_data.worlds.end(); ++b_world)
		if(world_map == b_world->ptr.data && b_world->recalc())
			world_recalc = true;

	bool recalc =
		shader_map.has_recalc() ||
		object_map.has_recalc() ||
		light_map.has_recalc() ||
		mesh_map.has_recalc() ||
		BlendDataObjects_recalc_get(&b_data.ptr) ||
		world_recalc;

	return recalc;
}

void BlenderSync::sync_data(BL::SpaceView3D b_v3d)
{
	sync_integrator();
	sync_film();
	sync_shaders();
	sync_objects(b_v3d);
}

/* Integrator */

void BlenderSync::sync_integrator()
{
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

	Integrator *integrator = scene->integrator;
	Integrator previntegrator = *integrator;

	integrator->min_bounce = get_int(cscene, "min_bounces");
	integrator->max_bounce = get_int(cscene, "max_bounces");

	integrator->max_diffuse_bounce = get_int(cscene, "diffuse_bounces");
	integrator->max_glossy_bounce = get_int(cscene, "glossy_bounces");
	integrator->max_transmission_bounce = get_int(cscene, "transmission_bounces");

	integrator->transparent_max_bounce = get_int(cscene, "transparent_max_bounces");
	integrator->transparent_min_bounce = get_int(cscene, "transparent_min_bounces");

	integrator->no_caustics = get_boolean(cscene, "no_caustics");
	integrator->blur_caustics = get_float(cscene, "blur_caustics");

	if(integrator->modified(previntegrator))
		integrator->tag_update(scene);
}

/* Film */

void BlenderSync::sync_film()
{
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

	Film *film = scene->film;
	Film prevfilm = *film;

	film->exposure = get_float(cscene, "film_exposure");

	if(film->modified(prevfilm))
		film->tag_update(scene);

	Filter *filter = scene->filter;
	Filter prevfilter = *filter;

	filter->filter_type = (FilterType)RNA_enum_get(&cscene, "filter_type");
	filter->filter_width = (filter->filter_type == FILTER_BOX)? 1.0f: get_float(cscene, "filter_width");

	if(filter->modified(prevfilter))
		filter->tag_update(scene);
}

/* Scene Parameters */

SceneParams BlenderSync::get_scene_params(BL::Scene b_scene)
{
	SceneParams params;
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
	int shadingsystem = RNA_enum_get(&cscene, "shading_system");

	if(shadingsystem == 0)
		params.shadingsystem = SceneParams::SVM;
	else if(shadingsystem == 1)
		params.shadingsystem = SceneParams::OSL;
	
	params.bvh_type = (SceneParams::BVHType)RNA_enum_get(&cscene, "debug_bvh_type");
	params.use_bvh_spatial_split = RNA_boolean_get(&cscene, "debug_use_spatial_splits");

	return params;
}

/* Session Parameters */

bool BlenderSync::get_session_pause(BL::Scene b_scene, bool background)
{
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
	return (background)? false: get_boolean(cscene, "preview_pause");
}

SessionParams BlenderSync::get_session_params(BL::Scene b_scene, bool background)
{
	SessionParams params;
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

	/* device type */
	DeviceType dtype = (RNA_enum_get(&cscene, "device") == 1)? DEVICE_CUDA: DEVICE_CPU;

	params.device_type = DEVICE_CPU;
	vector<DeviceType> types = Device::available_types();

	foreach(DeviceType dt, types)
		if(dt == dtype)
			params.device_type = dtype;
			
	/* Background */
	params.background = background;
			
	/* passes */
	if(background) {
		params.passes = get_int(cscene, "passes");
	}
	else {
		params.passes = get_int(cscene, "preview_passes");
		if(params.passes == 0)
			params.passes = INT_MAX;
	}

	/* other parameters */
	params.threads = b_scene.render().threads();
	params.tile_size = get_int(cscene, "debug_tile_size");
	params.min_size = get_int(cscene, "debug_min_size");
	params.cancel_timeout = get_float(cscene, "debug_cancel_timeout");
	params.reset_timeout = get_float(cscene, "debug_reset_timeout");
	params.text_timeout = get_float(cscene, "debug_text_timeout");

	if(background) {
		params.progressive = true;
		params.min_size = INT_MAX;
	}
	else
		params.progressive = true;

	return params;
}

CCL_NAMESPACE_END


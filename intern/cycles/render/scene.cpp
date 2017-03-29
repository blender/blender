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

#include <stdlib.h>

#include "render/background.h"
#include "render/bake.h"
#include "render/camera.h"
#include "render/curves.h"
#include "device/device.h"
#include "render/film.h"
#include "render/integrator.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/osl.h"
#include "render/particles.h"
#include "render/scene.h"
#include "render/shader.h"
#include "render/svm.h"
#include "render/tables.h"

#include "util/util_foreach.h"
#include "util/util_guarded_allocator.h"
#include "util/util_logging.h"
#include "util/util_progress.h"

CCL_NAMESPACE_BEGIN

Scene::Scene(const SceneParams& params_, const DeviceInfo& device_info_)
: params(params_)
{
	device = NULL;
	memset(&dscene.data, 0, sizeof(dscene.data));

	camera = new Camera();
	lookup_tables = new LookupTables();
	film = new Film();
	background = new Background();
	light_manager = new LightManager();
	mesh_manager = new MeshManager();
	object_manager = new ObjectManager();
	integrator = new Integrator();
	image_manager = new ImageManager(device_info_);
	particle_system_manager = new ParticleSystemManager();
	curve_system_manager = new CurveSystemManager();
	bake_manager = new BakeManager();

	/* OSL only works on the CPU */
	if(device_info_.type == DEVICE_CPU)
		shader_manager = ShaderManager::create(this, params.shadingsystem);
	else
		shader_manager = ShaderManager::create(this, SHADINGSYSTEM_SVM);
}

Scene::~Scene()
{
	free_memory(true);
}

void Scene::free_memory(bool final)
{
	foreach(Shader *s, shaders)
		delete s;
	foreach(Mesh *m, meshes)
		delete m;
	foreach(Object *o, objects)
		delete o;
	foreach(Light *l, lights)
		delete l;
	foreach(ParticleSystem *p, particle_systems)
		delete p;

	shaders.clear();
	meshes.clear();
	objects.clear();
	lights.clear();
	particle_systems.clear();

	if(device) {
		camera->device_free(device, &dscene, this);
		film->device_free(device, &dscene, this);
		background->device_free(device, &dscene);
		integrator->device_free(device, &dscene);

		object_manager->device_free(device, &dscene);
		mesh_manager->device_free(device, &dscene);
		shader_manager->device_free(device, &dscene, this);
		light_manager->device_free(device, &dscene);

		particle_system_manager->device_free(device, &dscene);
		curve_system_manager->device_free(device, &dscene);

		bake_manager->device_free(device, &dscene);

		if(!params.persistent_data || final)
			image_manager->device_free(device, &dscene);
		else
			image_manager->device_free_builtin(device, &dscene);

		lookup_tables->device_free(device, &dscene);
	}

	if(final) {
		delete lookup_tables;
		delete camera;
		delete film;
		delete background;
		delete integrator;
		delete object_manager;
		delete mesh_manager;
		delete shader_manager;
		delete light_manager;
		delete particle_system_manager;
		delete curve_system_manager;
		delete image_manager;
		delete bake_manager;
	}
}

void Scene::device_update(Device *device_, Progress& progress)
{
	if(!device)
		device = device_;

	bool print_stats = need_data_update();

	/* The order of updates is important, because there's dependencies between
	 * the different managers, using data computed by previous managers.
	 *
	 * - Image manager uploads images used by shaders.
	 * - Camera may be used for adaptive subdivision.
	 * - Displacement shader must have all shader data available.
	 * - Light manager needs lookup tables and final mesh data to compute emission CDF.
	 * - Film needs light manager to run for use_light_visibility
	 * - Lookup tables are done a second time to handle film tables
	 */
	
	image_manager->set_pack_images(device->info.pack_images);

	progress.set_status("Updating Shaders");
	shader_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Background");
	background->device_update(device, &dscene, this);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Camera");
	camera->device_update(device, &dscene, this);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Meshes Flags");
	mesh_manager->device_update_flags(device, &dscene, this, progress);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Objects");
	object_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Meshes");
	mesh_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Objects Flags");
	object_manager->device_update_flags(device, &dscene, this, progress);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Images");
	image_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Camera Volume");
	camera->device_update_volume(device, &dscene, this);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Hair Systems");
	curve_system_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Lookup Tables");
	lookup_tables->device_update(device, &dscene);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Lights");
	light_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Particle Systems");
	particle_system_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Integrator");
	integrator->device_update(device, &dscene, this);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Film");
	film->device_update(device, &dscene, this);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Lookup Tables");
	lookup_tables->device_update(device, &dscene);

	if(progress.get_cancel() || device->have_error()) return;

	progress.set_status("Updating Baking");
	bake_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel() || device->have_error()) return;

	if(device->have_error() == false) {
		progress.set_status("Updating Device", "Writing constant memory");
		device->const_copy_to("__data", &dscene.data, sizeof(dscene.data));
	}

	if(print_stats) {
		size_t mem_used = util_guarded_get_mem_used();
		size_t mem_peak = util_guarded_get_mem_peak();

		VLOG(1) << "System memory statistics after full device sync:\n"
		        << "  Usage: " << string_human_readable_number(mem_used)
		        << " (" << string_human_readable_size(mem_used) << ")\n"
		        << "  Peak: " << string_human_readable_number(mem_peak)
		        << " (" << string_human_readable_size(mem_peak) << ")";
	}
}

Scene::MotionType Scene::need_motion(bool advanced_shading)
{
	if(integrator->motion_blur)
		return (advanced_shading)? MOTION_BLUR: MOTION_NONE;
	else if(Pass::contains(film->passes, PASS_MOTION))
		return MOTION_PASS;
	else
		return MOTION_NONE;
}

float Scene::motion_shutter_time()
{
	if(need_motion() == Scene::MOTION_PASS)
		return 2.0f;
	else
		return camera->shuttertime;
}

bool Scene::need_global_attribute(AttributeStandard std)
{
	if(std == ATTR_STD_UV)
		return Pass::contains(film->passes, PASS_UV);
	else if(std == ATTR_STD_MOTION_VERTEX_POSITION)
		return need_motion() != MOTION_NONE;
	else if(std == ATTR_STD_MOTION_VERTEX_NORMAL)
		return need_motion() == MOTION_BLUR;
	
	return false;
}

void Scene::need_global_attributes(AttributeRequestSet& attributes)
{
	for(int std = ATTR_STD_NONE; std < ATTR_STD_NUM; std++)
		if(need_global_attribute((AttributeStandard)std))
			attributes.add((AttributeStandard)std);
}

bool Scene::need_update()
{
	return (need_reset() || film->need_update);
}

bool Scene::need_data_update()
{
	return (background->need_update
		|| image_manager->need_update
		|| object_manager->need_update
		|| mesh_manager->need_update
		|| light_manager->need_update
		|| lookup_tables->need_update
		|| integrator->need_update
		|| shader_manager->need_update
		|| particle_system_manager->need_update
		|| curve_system_manager->need_update
		|| bake_manager->need_update
		|| film->need_update);
}

bool Scene::need_reset()
{
	return need_data_update() || camera->need_update;
}

void Scene::reset()
{
	shader_manager->reset(this);
	shader_manager->add_default(this);

	/* ensure all objects are updated */
	camera->tag_update();
	film->tag_update(this);
	background->tag_update(this);
	integrator->tag_update(this);
	object_manager->tag_update(this);
	mesh_manager->tag_update(this);
	light_manager->tag_update(this);
	particle_system_manager->tag_update(this);
	curve_system_manager->tag_update(this);
}

void Scene::device_free()
{
	free_memory(false);
}

CCL_NAMESPACE_END


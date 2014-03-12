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

#include <stdlib.h>

#include "background.h"
#include "camera.h"
#include "curves.h"
#include "device.h"
#include "film.h"
#include "integrator.h"
#include "light.h"
#include "mesh.h"
#include "object.h"
#include "osl.h"
#include "particles.h"
#include "scene.h"
#include "shader.h"
#include "svm.h"
#include "tables.h"

#include "util_foreach.h"
#include "util_progress.h"

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
	image_manager = new ImageManager();
	particle_system_manager = new ParticleSystemManager();
	curve_system_manager = new CurveSystemManager();

	/* OSL only works on the CPU */
	if(device_info_.type == DEVICE_CPU)
		shader_manager = ShaderManager::create(this, params.shadingsystem);
	else
		shader_manager = ShaderManager::create(this, SceneParams::SVM);

	if (device_info_.type == DEVICE_CPU)
		image_manager->set_extended_image_limits();
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
		camera->device_free(device, &dscene);
		film->device_free(device, &dscene, this);
		background->device_free(device, &dscene);
		integrator->device_free(device, &dscene);

		object_manager->device_free(device, &dscene);
		mesh_manager->device_free(device, &dscene);
		shader_manager->device_free(device, &dscene, this);
		light_manager->device_free(device, &dscene);

		particle_system_manager->device_free(device, &dscene);
		curve_system_manager->device_free(device, &dscene);

		if(!params.persistent_data || final)
			image_manager->device_free(device, &dscene);

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
	}
}

void Scene::device_update(Device *device_, Progress& progress)
{
	if(!device)
		device = device_;
	
	/* The order of updates is important, because there's dependencies between
	 * the different managers, using data computed by previous managers.
	 *
	 * - Image manager uploads images used by shaders.
	 * - Camera may be used for adapative subdivison.
	 * - Displacement shader must have all shader data available.
	 * - Light manager needs lookup tables and final mesh data to compute emission CDF.
	 * - Film needs light manager to run for use_light_visibility
	 * - Lookup tables are done a second time to handle film tables
	 */
	
	image_manager->set_pack_images(device->info.pack_images);

	progress.set_status("Updating Shaders");
	shader_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Images");
	image_manager->device_update(device, &dscene, progress);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Background");
	background->device_update(device, &dscene, this);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Camera");
	camera->device_update(device, &dscene, this);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Objects");
	object_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Hair Systems");
	curve_system_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Lookup Tables");
	lookup_tables->device_update(device, &dscene);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Meshes");
	mesh_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Lights");
	light_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Particle Systems");
	particle_system_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Film");
	film->device_update(device, &dscene, this);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Integrator");
	integrator->device_update(device, &dscene, this);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Lookup Tables");
	lookup_tables->device_update(device, &dscene);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Device", "Writing constant memory");
	device->const_copy_to("__data", &dscene.data, sizeof(dscene.data));
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

bool Scene::need_global_attribute(AttributeStandard std)
{
	if(std == ATTR_STD_UV)
		return Pass::contains(film->passes, PASS_UV);
	if(std == ATTR_STD_MOTION_PRE || std == ATTR_STD_MOTION_POST)
		return need_motion() == MOTION_PASS;
	
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

bool Scene::need_reset()
{
	return (background->need_update
		|| image_manager->need_update
		|| camera->need_update
		|| object_manager->need_update
		|| mesh_manager->need_update
		|| light_manager->need_update
		|| lookup_tables->need_update
		|| integrator->need_update
		|| shader_manager->need_update
		|| particle_system_manager->need_update
		|| curve_system_manager->need_update);
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
}

void Scene::device_free()
{
	free_memory(false);
}

CCL_NAMESPACE_END


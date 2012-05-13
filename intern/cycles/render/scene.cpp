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

#include <stdlib.h>

#include "background.h"
#include "camera.h"
#include "device.h"
#include "film.h"
#include "filter.h"
#include "integrator.h"
#include "light.h"
#include "shader.h"
#include "mesh.h"
#include "object.h"
#include "scene.h"
#include "svm.h"
#include "osl.h"

#include "util_foreach.h"
#include "util_progress.h"

CCL_NAMESPACE_BEGIN

Scene::Scene(const SceneParams& params_)
: params(params_)
{
	device = NULL;
	memset(&dscene.data, 0, sizeof(dscene.data));

	camera = new Camera();
	filter = new Filter();
	film = new Film();
	background = new Background();
	light_manager = new LightManager();
	mesh_manager = new MeshManager();
	object_manager = new ObjectManager();
	integrator = new Integrator();
	image_manager = new ImageManager();
	shader_manager = ShaderManager::create(this);
}

Scene::~Scene()
{
	if(device) camera->device_free(device, &dscene);
	delete camera;

	if(device) filter->device_free(device, &dscene);
	delete filter;

	if(device) film->device_free(device, &dscene);
	delete film;

	if(device) background->device_free(device, &dscene);
	delete background;

	if(device) mesh_manager->device_free(device, &dscene);
	delete mesh_manager;

	if(device) object_manager->device_free(device, &dscene);
	delete object_manager;

	if(device) integrator->device_free(device, &dscene);
	delete integrator;

	if(device) shader_manager->device_free(device, &dscene);
	delete shader_manager;

	if(device) light_manager->device_free(device, &dscene);
	delete light_manager;

	foreach(Shader *s, shaders)
		delete s;
	foreach(Mesh *m, meshes)
		delete m;
	foreach(Object *o, objects)
		delete o;
	foreach(Light *l, lights)
		delete l;

	if(device) image_manager->device_free(device, &dscene);
	delete image_manager;
}

void Scene::device_update(Device *device_, Progress& progress)
{
	if(!device)
		device = device_;
	
	/* The order of updates is important, because there's dependencies between
	 * the different managers, using data computed by previous managers.
	 *
	 * - Background generates shader graph compiled by shader manager.
	 * - Image manager uploads images used by shaders.
	 * - Camera may be used for adapative subdivison.
	 * - Displacement shader must have all shader data available.
	 * - Light manager needs final mesh data to compute emission CDF.
	 */
	
	image_manager->set_pack_images(device->info.pack_images);

	progress.set_status("Updating Background");
	background->device_update(device, &dscene, this);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Shaders");
	shader_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Images");
	image_manager->device_update(device, &dscene, progress);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Camera");
	camera->device_update(device, &dscene, this);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Objects");
	object_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Meshes");
	mesh_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Lights");
	light_manager->device_update(device, &dscene, this, progress);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Filter");
	filter->device_update(device, &dscene);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Film");
	film->device_update(device, &dscene);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Integrator");
	integrator->device_update(device, &dscene);

	if(progress.get_cancel()) return;

	progress.set_status("Updating Device", "Writing constant memory");
	device->const_copy_to("__data", &dscene.data, sizeof(dscene.data));
}

Scene::MotionType Scene::need_motion()
{
	if(integrator->motion_blur)
		return MOTION_BLUR;
	else if(Pass::contains(film->passes, PASS_MOTION))
		return MOTION_PASS;
	else
		return MOTION_NONE;
}

bool Scene::need_global_attribute(AttributeStandard std)
{
	if(std == ATTR_STD_UV)
		return Pass::contains(film->passes, PASS_UV);
	if(std == ATTR_STD_MOTION_PRE || ATTR_STD_MOTION_POST)
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
		|| filter->need_update
		|| integrator->need_update
		|| shader_manager->need_update);
}

CCL_NAMESPACE_END


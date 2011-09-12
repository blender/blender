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
#include "light.h"
#include "mesh.h"
#include "object.h"
#include "scene.h"
#include "shader.h"

#include "util_foreach.h"
#include "util_progress.h"

CCL_NAMESPACE_BEGIN

/* Light */

Light::Light()
{
	co = make_float3(0.0f, 0.0f, 0.0f);
	radius = 0.0f;
	shader = 0;
}

void Light::tag_update(Scene *scene)
{
	scene->light_manager->need_update = true;
}

/* Light Manager */

LightManager::LightManager()
{
	need_update = true;
}

LightManager::~LightManager()
{
}

void LightManager::device_update_distribution(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	/* option to always sample all point lights */
	bool multi_light = false;

	/* count */
	size_t num_lights = scene->lights.size();
	size_t num_triangles = 0;

	foreach(Object *object, scene->objects) {
		Mesh *mesh = object->mesh;
		bool have_emission = false;

		/* skip if we have no emission shaders */
		foreach(uint sindex, mesh->used_shaders) {
			if(scene->shaders[sindex]->has_surface_emission) {
				have_emission = true;
				break;
			}
		}

		/* count triangles */
		if(have_emission) {
			for(size_t i = 0; i < mesh->triangles.size(); i++) {
				Shader *shader = scene->shaders[mesh->shader[i]];

				if(shader->has_surface_emission)
					num_triangles++;
			}
		}
	}

	size_t num_distribution = num_triangles;

	if(!multi_light)
		num_distribution += num_lights;

	/* emission area */
	float4 *distribution = dscene->light_distribution.resize(num_distribution + 1);
	float totarea = 0.0f;

	/* triangles */
	size_t offset = 0;
	size_t j = 0;

	foreach(Object *object, scene->objects) {
		Mesh *mesh = object->mesh;
		bool have_emission = false;

		/* skip if we have no emission shaders */
		foreach(uint sindex, mesh->used_shaders) {
			if(scene->shaders[sindex]->has_surface_emission) {
				have_emission = true;
				break;
			}
		}

		/* sum area */
		if(have_emission) {
			Transform tfm = object->tfm;
			int object_id = (mesh->transform_applied)? -j-1: j;

			for(size_t i = 0; i < mesh->triangles.size(); i++) {
				Shader *shader = scene->shaders[mesh->shader[i]];

				if(shader->has_surface_emission) {
					distribution[offset].x = totarea;
					distribution[offset].y = __int_as_float(i + mesh->tri_offset);
					distribution[offset].z = 1.0f;
					distribution[offset].w = __int_as_float(object_id);
					offset++;

					Mesh::Triangle t = mesh->triangles[i];
					float3 p1 = transform(&tfm, mesh->verts[t.v[0]]);
					float3 p2 = transform(&tfm, mesh->verts[t.v[1]]);
					float3 p3 = transform(&tfm, mesh->verts[t.v[2]]);

					totarea += triangle_area(p1, p2, p3);
				}
			}
		}

		if(progress.get_cancel()) return;

		j++;
	}

	float trianglearea = totarea;

	/* point lights */
	if(!multi_light) {
		float lightarea = (totarea > 0.0f)? totarea/scene->lights.size(): 1.0f;

		for(size_t i = 0; i < scene->lights.size(); i++, offset++) {
			distribution[offset].x = totarea;
			distribution[offset].y = __int_as_float(-i-1);
			distribution[offset].z = 1.0f;
			distribution[offset].w = scene->lights[i]->radius;
			totarea += lightarea;
		}
	}

	/* normalize cumulative distribution functions */
	distribution[num_distribution].x = totarea;
	distribution[num_distribution].y = 0.0f;
	distribution[num_distribution].z = 0.0f;
	distribution[num_distribution].w = 0.0f;

	if(totarea > 0.0f) {
		for(size_t i = 0; i < num_distribution; i++)
			distribution[i].x /= totarea;
		distribution[num_distribution].x = 1.0f;
	}

	if(progress.get_cancel()) return;

	/* update device */
	KernelIntegrator *kintegrator = &dscene->data.integrator;
	kintegrator->use_emission = (totarea > 0.0f) || (multi_light && num_lights);

	if(kintegrator->use_emission) {
		/* number of emissives */
		kintegrator->num_distribution = (totarea > 0.0f)? num_distribution: 0;

		/* precompute pdfs */
		kintegrator->pdf_triangles = 0.0f;
		kintegrator->pdf_lights = 0.0f;

		if(multi_light) {
			/* sample one of all triangles and all lights */
			kintegrator->num_all_lights = num_lights;

			if(trianglearea > 0.0f)
				kintegrator->pdf_triangles = 1.0f/trianglearea;
			if(num_lights)
				kintegrator->pdf_lights = 1.0f;
		}
		else {
			/* sample one, with 0.5 probability of light or triangle */
			kintegrator->num_all_lights = 0;

			if(trianglearea > 0.0f) {
				kintegrator->pdf_triangles = 1.0f/trianglearea;
				if(num_lights)
					kintegrator->pdf_triangles *= 0.5f;
			}

			if(num_lights) {
				kintegrator->pdf_lights = 1.0f/num_lights;
				if(trianglearea > 0.0f)
					kintegrator->pdf_lights *= 0.5f;
			}
		}

		/* CDF */
		device->tex_alloc("__light_distribution", dscene->light_distribution);
	}
	else
		dscene->light_distribution.clear();
}

void LightManager::device_update_points(Device *device, DeviceScene *dscene, Scene *scene)
{
	if(scene->lights.size() == 0)
		return;

	float4 *light_point = dscene->light_point.resize(scene->lights.size());

	for(size_t i = 0; i < scene->lights.size(); i++) {
		float3 co = scene->lights[i]->co;
		int shader_id = scene->shader_manager->get_shader_id(scene->lights[i]->shader);

		light_point[i] = make_float4(co.x, co.y, co.z, __int_as_float(shader_id));
	}
	
	device->tex_alloc("__light_point", dscene->light_point);
}

void LightManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;

	device_free(device, dscene);

	device_update_points(device, dscene, scene);
	if(progress.get_cancel()) return;

	device_update_distribution(device, dscene, scene, progress);
	if(progress.get_cancel()) return;

	need_update = false;
}

void LightManager::device_free(Device *device, DeviceScene *dscene)
{
	device->tex_free(dscene->light_distribution);
	device->tex_free(dscene->light_point);

	dscene->light_distribution.clear();
	dscene->light_point.clear();
}

void LightManager::tag_update(Scene *scene)
{
	need_update = true;
}

CCL_NAMESPACE_END


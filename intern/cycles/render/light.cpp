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

static void dump_background_pixels(Device *device, DeviceScene *dscene, int res, vector<float3>& pixels)
{
	/* create input */
	int width = res;
	int height = res;

	device_vector<uint4> d_input;
	device_vector<float4> d_output;

	uint4 *d_input_data = d_input.resize(width*height);

	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			float u = x/(float)width;
			float v = y/(float)height;

			uint4 in = make_uint4(__float_as_int(u), __float_as_int(v), 0, 0);
			d_input_data[x + y*width] = in;
		}
	}

	/* compute on device */
	float4 *d_output_data = d_output.resize(width*height);
	memset((void*)d_output.data_pointer, 0, d_output.memory_size());

	device->const_copy_to("__data", &dscene->data, sizeof(dscene->data));

	device->mem_alloc(d_input, MEM_READ_ONLY);
	device->mem_copy_to(d_input);
	device->mem_alloc(d_output, MEM_WRITE_ONLY);

	DeviceTask main_task(DeviceTask::SHADER);
	main_task.shader_input = d_input.device_pointer;
	main_task.shader_output = d_output.device_pointer;
	main_task.shader_eval_type = SHADER_EVAL_BACKGROUND;
	main_task.shader_x = 0;
	main_task.shader_w = width*height;

	list<DeviceTask> split_tasks;
	main_task.split_max_size(split_tasks, 128*128);

	foreach(DeviceTask& task, split_tasks) {
		device->task_add(task);
		device->task_wait();
	}

	device->mem_copy_from(d_output, 0, 1, d_output.size(), sizeof(float4));
	device->mem_free(d_input);
	device->mem_free(d_output);

	d_output_data = reinterpret_cast<float4*>(d_output.data_pointer);

	pixels.resize(width*height);

	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			pixels[y*width + x].x = d_output_data[y*width + x].x;
			pixels[y*width + x].y = d_output_data[y*width + x].y;
			pixels[y*width + x].z = d_output_data[y*width + x].z;
		}
	}
}

/* Light */

Light::Light()
{
	type = LIGHT_POINT;

	co = make_float3(0.0f, 0.0f, 0.0f);

	dir = make_float3(0.0f, 0.0f, 0.0f);
	size = 0.0f;

	axisu = make_float3(0.0f, 0.0f, 0.0f);
	sizeu = 1.0f;
	axisv = make_float3(0.0f, 0.0f, 0.0f);
	sizev = 1.0f;

	map_resolution = 512;

	spot_angle = M_PI_F/4.0f;
	spot_smooth = 0.0f;

	cast_shadow = true;
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
	progress.set_status("Updating Lights", "Computing distribution");

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
			Shader *shader = scene->shaders[sindex];

			if(shader->sample_as_light && shader->has_surface_emission) {
				have_emission = true;
				break;
			}
		}

		/* count triangles */
		if(have_emission) {
			for(size_t i = 0; i < mesh->triangles.size(); i++) {
				Shader *shader = scene->shaders[mesh->shader[i]];

				if(shader->sample_as_light && shader->has_surface_emission)
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
	int j = 0;

	foreach(Object *object, scene->objects) {
		Mesh *mesh = object->mesh;
		bool have_emission = false;

		/* skip if we have no emission shaders */
		foreach(uint sindex, mesh->used_shaders) {
			Shader *shader = scene->shaders[sindex];

			if(shader->sample_as_light && shader->has_surface_emission) {
				have_emission = true;
				break;
			}
		}

		/* sum area */
		if(have_emission) {
			Transform tfm = object->tfm;
			int object_id = j;

			if(mesh->transform_applied)
				object_id = ~object_id;

			for(size_t i = 0; i < mesh->triangles.size(); i++) {
				Shader *shader = scene->shaders[mesh->shader[i]];

				if(shader->sample_as_light && shader->has_surface_emission) {
					distribution[offset].x = totarea;
					distribution[offset].y = __int_as_float(i + mesh->tri_offset);
					distribution[offset].z = 1.0f;
					distribution[offset].w = __int_as_float(object_id);
					offset++;

					Mesh::Triangle t = mesh->triangles[i];
					float3 p1 = transform_point(&tfm, mesh->verts[t.v[0]]);
					float3 p2 = transform_point(&tfm, mesh->verts[t.v[1]]);
					float3 p3 = transform_point(&tfm, mesh->verts[t.v[2]]);

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

		for(int i = 0; i < scene->lights.size(); i++, offset++) {
			distribution[offset].x = totarea;
			distribution[offset].y = __int_as_float(~(int)i);
			distribution[offset].z = 1.0f;
			distribution[offset].w = scene->lights[i]->size;
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
	kintegrator->use_direct_light = (totarea > 0.0f) || (multi_light && num_lights);

	if(kintegrator->use_direct_light) {
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

void LightManager::device_update_background(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	KernelIntegrator *kintegrator = &dscene->data.integrator;
	Light *background_light = NULL;

	/* find background light */
	foreach(Light *light, scene->lights) {
		if(light->type == LIGHT_BACKGROUND) {
			background_light = light;
			break;
		}
	}

	/* no background light found, signal renderer to skip sampling */
	if(!background_light) {
		kintegrator->pdf_background_res = 0;
		return;
	}

	progress.set_status("Updating Lights", "Importance map");

	assert(kintegrator->use_direct_light);

	/* get the resolution from the light's size (we stuff it in there) */
	int res = background_light->map_resolution;
	kintegrator->pdf_background_res = res;

	assert(res > 0);

	vector<float3> pixels;
	dump_background_pixels(device, dscene, res, pixels);

	if(progress.get_cancel())
		return;

	/* build row distributions and column distribution for the infinite area environment light */
	int cdf_count = res + 1;
	float2 *marg_cdf = dscene->light_background_marginal_cdf.resize(cdf_count);
	float2 *cond_cdf = dscene->light_background_conditional_cdf.resize(cdf_count * cdf_count);

	/* conditional CDFs (rows, U direction) */
	for(int i = 0; i < res; i++) {
		float sin_theta = sinf(M_PI_F * (i + 0.5f) / res);
		float3 env_color = pixels[i * res];
		float ave_luminamce = average(env_color);

		cond_cdf[i * cdf_count].x = ave_luminamce * sin_theta;
		cond_cdf[i * cdf_count].y = 0.0f;

		for(int j = 1; j < res; j++) {
			env_color = pixels[i * res + j];
			ave_luminamce = average(env_color);

			cond_cdf[i * cdf_count + j].x = ave_luminamce * sin_theta;
			cond_cdf[i * cdf_count + j].y = cond_cdf[i * cdf_count + j - 1].y + cond_cdf[i * cdf_count + j - 1].x / res;
		}

		float cdf_total = cond_cdf[i * cdf_count + res - 1].y + cond_cdf[i * cdf_count + res - 1].x / res;

		/* stuff the total into the brightness value for the last entry, because
		   we are going to normalize the CDFs to 0.0 to 1.0 afterwards */
		cond_cdf[i * cdf_count + res].x = cdf_total;

		if(cdf_total > 0.0f)
			for(int j = 1; j < res; j++)
				cond_cdf[i * cdf_count + j].y /= cdf_total;

		cond_cdf[i * cdf_count + res].y = 1.0f;
	}

	/* marginal CDFs (column, V direction, sum of rows) */
	marg_cdf[0].x = cond_cdf[res].x;
	marg_cdf[0].y = 0.0f;

	for(int i = 1; i < res; i++) {
		marg_cdf[i].x = cond_cdf[i * cdf_count + res].x;
		marg_cdf[i].y = marg_cdf[i - 1].y + marg_cdf[i - 1].x / res;
	}

	float cdf_total = marg_cdf[res - 1].y + marg_cdf[res - 1].x / res;
	marg_cdf[res].x = cdf_total;

	if(cdf_total > 0.0f)
		for(int i = 1; i < res; i++)
			marg_cdf[i].y /= cdf_total;

	marg_cdf[res].y = 1.0f;

	/* update device */
	device->tex_alloc("__light_background_marginal_cdf", dscene->light_background_marginal_cdf);
	device->tex_alloc("__light_background_conditional_cdf", dscene->light_background_conditional_cdf);
}

void LightManager::device_update_points(Device *device, DeviceScene *dscene, Scene *scene)
{
	if(scene->lights.size() == 0)
		return;

	float4 *light_data = dscene->light_data.resize(scene->lights.size()*LIGHT_SIZE);

	if(!device->info.advanced_shading) {
		/* remove unsupported light */
		foreach(Light *light, scene->lights) {
			if(light->type == LIGHT_BACKGROUND) {
				scene->lights.erase(std::remove(scene->lights.begin(), scene->lights.end(), light), scene->lights.end());
				break;
			}
		}
	}

	for(size_t i = 0; i < scene->lights.size(); i++) {
		Light *light = scene->lights[i];
		float3 co = light->co;
		float3 dir = normalize(light->dir);
		int shader_id = scene->shader_manager->get_shader_id(scene->lights[i]->shader);

		if(!light->cast_shadow)
			shader_id &= ~SHADER_CAST_SHADOW;

		if(light->type == LIGHT_POINT) {
			shader_id &= ~SHADER_AREA_LIGHT;

			light_data[i*LIGHT_SIZE + 0] = make_float4(__int_as_float(light->type), co.x, co.y, co.z);
			light_data[i*LIGHT_SIZE + 1] = make_float4(__int_as_float(shader_id), light->size, 0.0f, 0.0f);
			light_data[i*LIGHT_SIZE + 2] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
			light_data[i*LIGHT_SIZE + 3] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
		}
		else if(light->type == LIGHT_DISTANT) {
			shader_id &= ~SHADER_AREA_LIGHT;

			light_data[i*LIGHT_SIZE + 0] = make_float4(__int_as_float(light->type), dir.x, dir.y, dir.z);
			light_data[i*LIGHT_SIZE + 1] = make_float4(__int_as_float(shader_id), light->size, 0.0f, 0.0f);
			light_data[i*LIGHT_SIZE + 2] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
			light_data[i*LIGHT_SIZE + 3] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
		}
		else if(light->type == LIGHT_BACKGROUND) {
			shader_id &= ~SHADER_AREA_LIGHT;

			light_data[i*LIGHT_SIZE + 0] = make_float4(__int_as_float(light->type), 0.0f, 0.0f, 0.0f);
			light_data[i*LIGHT_SIZE + 1] = make_float4(__int_as_float(shader_id), 0.0f, 0.0f, 0.0f);
			light_data[i*LIGHT_SIZE + 2] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
			light_data[i*LIGHT_SIZE + 3] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
		}
		else if(light->type == LIGHT_AREA) {
			float3 axisu = light->axisu*(light->sizeu*light->size);
			float3 axisv = light->axisv*(light->sizev*light->size);

			light_data[i*LIGHT_SIZE + 0] = make_float4(__int_as_float(light->type), co.x, co.y, co.z);
			light_data[i*LIGHT_SIZE + 1] = make_float4(__int_as_float(shader_id), axisu.x, axisu.y, axisu.z);
			light_data[i*LIGHT_SIZE + 2] = make_float4(0.0f, axisv.x, axisv.y, axisv.z);
			light_data[i*LIGHT_SIZE + 3] = make_float4(0.0f, dir.x, dir.y, dir.z);
		}
		else if(light->type == LIGHT_SPOT) {
			shader_id &= ~SHADER_AREA_LIGHT;

			float spot_angle = cosf(light->spot_angle*0.5f);
			float spot_smooth = (1.0f - spot_angle)*light->spot_smooth;

			light_data[i*LIGHT_SIZE + 0] = make_float4(__int_as_float(light->type), co.x, co.y, co.z);
			light_data[i*LIGHT_SIZE + 1] = make_float4(__int_as_float(shader_id), light->size, dir.x, dir.y);
			light_data[i*LIGHT_SIZE + 2] = make_float4(dir.z, spot_angle, spot_smooth, 0.0f);
			light_data[i*LIGHT_SIZE + 3] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
	
	device->tex_alloc("__light_data", dscene->light_data);
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

	device_update_background(device, dscene, scene, progress);
	if(progress.get_cancel()) return;

	need_update = false;
}

void LightManager::device_free(Device *device, DeviceScene *dscene)
{
	device->tex_free(dscene->light_distribution);
	device->tex_free(dscene->light_data);
	device->tex_free(dscene->light_background_marginal_cdf);
	device->tex_free(dscene->light_background_conditional_cdf);

	dscene->light_distribution.clear();
	dscene->light_data.clear();
	dscene->light_background_marginal_cdf.clear();
	dscene->light_background_conditional_cdf.clear();
}

void LightManager::tag_update(Scene *scene)
{
	need_update = true;
}

CCL_NAMESPACE_END


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
#include "render/film.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/shader.h"

#include "util/util_foreach.h"
#include "util/util_progress.h"
#include "util/util_logging.h"

CCL_NAMESPACE_BEGIN

static void shade_background_pixels(Device *device, DeviceScene *dscene, int res, vector<float3>& pixels, Progress& progress)
{
	/* create input */
	int width = res;
	int height = res;

	device_vector<uint4> d_input;
	device_vector<float4> d_output;

	uint4 *d_input_data = d_input.resize(width*height);

	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			float u = (x + 0.5f)/width;
			float v = (y + 0.5f)/height;

			uint4 in = make_uint4(__float_as_int(u), __float_as_int(v), 0, 0);
			d_input_data[x + y*width] = in;
		}
	}

	/* compute on device */
	d_output.resize(width*height);
	memset((void*)d_output.data_pointer, 0, d_output.memory_size());

	device->const_copy_to("__data", &dscene->data, sizeof(dscene->data));

	device->mem_alloc("shade_background_pixels_input", d_input, MEM_READ_ONLY);
	device->mem_copy_to(d_input);
	device->mem_alloc("shade_background_pixels_output", d_output, MEM_WRITE_ONLY);

	DeviceTask main_task(DeviceTask::SHADER);
	main_task.shader_input = d_input.device_pointer;
	main_task.shader_output = d_output.device_pointer;
	main_task.shader_eval_type = SHADER_EVAL_BACKGROUND;
	main_task.shader_x = 0;
	main_task.shader_w = width*height;
	main_task.num_samples = 1;
	main_task.get_cancel = function_bind(&Progress::get_cancel, &progress);

	/* disabled splitting for now, there's an issue with multi-GPU mem_copy_from */
	list<DeviceTask> split_tasks;
	main_task.split(split_tasks, 1, 128*128);

	foreach(DeviceTask& task, split_tasks) {
		device->task_add(task);
		device->task_wait();
		device->mem_copy_from(d_output, task.shader_x, 1, task.shader_w, sizeof(float4));
	}

	device->mem_free(d_input);
	device->mem_free(d_output);

	d_input.clear();

	float4 *d_output_data = reinterpret_cast<float4*>(d_output.data_pointer);

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

NODE_DEFINE(Light)
{
	NodeType* type = NodeType::add("light", create);

	static NodeEnum type_enum;
	type_enum.insert("point", LIGHT_POINT);
	type_enum.insert("distant", LIGHT_DISTANT);
	type_enum.insert("background", LIGHT_BACKGROUND);
	type_enum.insert("area", LIGHT_AREA);
	type_enum.insert("spot", LIGHT_SPOT);
	SOCKET_ENUM(type, "Type", type_enum, LIGHT_POINT);

	SOCKET_POINT(co, "Co", make_float3(0.0f, 0.0f, 0.0f));

	SOCKET_VECTOR(dir, "Dir", make_float3(0.0f, 0.0f, 0.0f));
	SOCKET_FLOAT(size, "Size", 0.0f);

	SOCKET_VECTOR(axisu, "Axis U", make_float3(0.0f, 0.0f, 0.0f));
	SOCKET_FLOAT(sizeu, "Size U", 1.0f);
	SOCKET_VECTOR(axisv, "Axis V", make_float3(0.0f, 0.0f, 0.0f));
	SOCKET_FLOAT(sizev, "Size V", 1.0f);

	SOCKET_INT(map_resolution, "Map Resolution", 512);

	SOCKET_FLOAT(spot_angle, "Spot Angle", M_PI_4_F);
	SOCKET_FLOAT(spot_smooth, "Spot Smooth", 0.0f);

	SOCKET_TRANSFORM(tfm, "Transform", transform_identity());

	SOCKET_BOOLEAN(cast_shadow, "Cast Shadow", true);
	SOCKET_BOOLEAN(use_mis, "Use Mis", false);
	SOCKET_BOOLEAN(use_diffuse, "Use Diffuse", true);
	SOCKET_BOOLEAN(use_glossy, "Use Glossy", true);
	SOCKET_BOOLEAN(use_transmission, "Use Transmission", true);
	SOCKET_BOOLEAN(use_scatter, "Use Scatter", true);

	SOCKET_INT(samples, "Samples", 1);
	SOCKET_INT(max_bounces, "Max Bounces", 1024);

	SOCKET_BOOLEAN(is_portal, "Is Portal", false);
	SOCKET_BOOLEAN(is_enabled, "Is Enabled", true);

	SOCKET_NODE(shader, "Shader", &Shader::node_type);

	return type;
}

Light::Light()
: Node(node_type)
{
}

void Light::tag_update(Scene *scene)
{
	scene->light_manager->need_update = true;
}

bool Light::has_contribution(Scene *scene)
{
	if(is_portal) {
		return false;
	}
	if(type == LIGHT_BACKGROUND) {
		return true;
	}
	return (shader) ? shader->has_surface_emission : scene->default_light->has_surface_emission;
}

/* Light Manager */

LightManager::LightManager()
{
	need_update = true;
	use_light_visibility = false;
}

LightManager::~LightManager()
{
}

bool LightManager::has_background_light(Scene *scene)
{
	foreach(Light *light, scene->lights) {
		if(light->type == LIGHT_BACKGROUND) {
			return true;
		}
	}
	return false;
}

void LightManager::disable_ineffective_light(Device *device, Scene *scene)
{
	/* Make all lights enabled by default, and perform some preliminary checks
	 * needed for finer-tuning of settings (for example, check whether we've
	 * got portals or not).
	 */
	bool has_portal = false, has_background = false;
	foreach(Light *light, scene->lights) {
		light->is_enabled = light->has_contribution(scene);
		has_portal |= light->is_portal;
		has_background |= light->type == LIGHT_BACKGROUND;
	}

	if(has_background) {
		/* Ignore background light if:
		 * - If unsupported on a device
		 * - If we don't need it (no HDRs etc.)
		 */
		Shader *shader = (scene->background->shader) ? scene->background->shader : scene->default_background;
		bool disable_mis = !(has_portal || shader->has_surface_spatial_varying) ||
		                   !(device->info.advanced_shading);
		if(disable_mis) {
			VLOG(1) << "Background MIS has been disabled.\n";
			foreach(Light *light, scene->lights) {
				if(light->type == LIGHT_BACKGROUND) {
					light->is_enabled = false;
				}
			}
		}
	}
}

bool LightManager::object_usable_as_light(Object *object) {
	Mesh *mesh = object->mesh;
	/* Skip objects with NaNs */
	if (!object->bounds.valid()) {
		return false;
	}
	/* Skip if we are not visible for BSDFs. */
	if(!(object->visibility & (PATH_RAY_DIFFUSE|PATH_RAY_GLOSSY|PATH_RAY_TRANSMIT))) {
		return false;
	}
	/* Skip motion blurred deforming meshes, not supported yet. */
	if(mesh->has_motion_blur()) {
		return false;
	}
	/* Skip if we have no emission shaders. */
	/* TODO(sergey): Ideally we want to avoid such duplicated loop, since it'll
	 * iterate all mesh shaders twice (when counting and when calculating
	 * triangle area.
	 */
	foreach(const Shader *shader, mesh->used_shaders) {
		if(shader->use_mis && shader->has_surface_emission) {
			return true;
		}
	}
	return false;
}

void LightManager::device_update_distribution(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	progress.set_status("Updating Lights", "Computing distribution");

	/* count */
	size_t num_lights = 0;
	size_t num_portals = 0;
	size_t num_background_lights = 0;
	size_t num_triangles = 0;

	bool background_mis = false;

	foreach(Light *light, scene->lights) {
		if(light->is_enabled) {
			num_lights++;
		}
		if(light->is_portal) {
			num_portals++;
		}
	}

	foreach(Object *object, scene->objects) {
		if(progress.get_cancel()) return;

		if(!object_usable_as_light(object)) {
			continue;
		}
		/* Count triangles. */
		Mesh *mesh = object->mesh;
		size_t mesh_num_triangles = mesh->num_triangles();
		for(size_t i = 0; i < mesh_num_triangles; i++) {
			int shader_index = mesh->shader[i];
			Shader *shader = (shader_index < mesh->used_shaders.size())
			                         ? mesh->used_shaders[shader_index]
			                         : scene->default_surface;

			if(shader->use_mis && shader->has_surface_emission) {
				num_triangles++;
			}
		}
	}

	size_t num_distribution = num_triangles + num_lights;
	VLOG(1) << "Total " << num_distribution << " of light distribution primitives.";

	/* emission area */
	float4 *distribution = dscene->light_distribution.resize(num_distribution + 1);
	float totarea = 0.0f;

	/* triangles */
	size_t offset = 0;
	int j = 0;

	foreach(Object *object, scene->objects) {
		if(progress.get_cancel()) return;

		if(!object_usable_as_light(object)) {
			j++;
			continue;
		}
		/* Sum area. */
		Mesh *mesh = object->mesh;
		bool transform_applied = mesh->transform_applied;
		Transform tfm = object->tfm;
		int object_id = j;
		int shader_flag = 0;

		if(!(object->visibility & PATH_RAY_DIFFUSE)) {
			shader_flag |= SHADER_EXCLUDE_DIFFUSE;
			use_light_visibility = true;
		}
		if(!(object->visibility & PATH_RAY_GLOSSY)) {
			shader_flag |= SHADER_EXCLUDE_GLOSSY;
			use_light_visibility = true;
		}
		if(!(object->visibility & PATH_RAY_TRANSMIT)) {
			shader_flag |= SHADER_EXCLUDE_TRANSMIT;
			use_light_visibility = true;
		}
		if(!(object->visibility & PATH_RAY_VOLUME_SCATTER)) {
			shader_flag |= SHADER_EXCLUDE_SCATTER;
			use_light_visibility = true;
		}

		size_t mesh_num_triangles = mesh->num_triangles();
		for(size_t i = 0; i < mesh_num_triangles; i++) {
			int shader_index = mesh->shader[i];
			Shader *shader = (shader_index < mesh->used_shaders.size())
			                         ? mesh->used_shaders[shader_index]
			                         : scene->default_surface;

			if(shader->use_mis && shader->has_surface_emission) {
				distribution[offset].x = totarea;
				distribution[offset].y = __int_as_float(i + mesh->tri_offset);
				distribution[offset].z = __int_as_float(shader_flag);
				distribution[offset].w = __int_as_float(object_id);
				offset++;

				Mesh::Triangle t = mesh->get_triangle(i);
				if(!t.valid(&mesh->verts[0])) {
					continue;
				}
				float3 p1 = mesh->verts[t.v[0]];
				float3 p2 = mesh->verts[t.v[1]];
				float3 p3 = mesh->verts[t.v[2]];

				if(!transform_applied) {
					p1 = transform_point(&tfm, p1);
					p2 = transform_point(&tfm, p2);
					p3 = transform_point(&tfm, p3);
				}

				totarea += triangle_area(p1, p2, p3);
			}
		}

		j++;
	}

	float trianglearea = totarea;

	/* point lights */
	float lightarea = (totarea > 0.0f) ? totarea / num_lights : 1.0f;
	bool use_lamp_mis = false;

	int light_index = 0;
	foreach(Light *light, scene->lights) {
		if(!light->is_enabled)
			continue;

		distribution[offset].x = totarea;
		distribution[offset].y = __int_as_float(~light_index);
		distribution[offset].z = 1.0f;
		distribution[offset].w = light->size;
		totarea += lightarea;

		if(light->size > 0.0f && light->use_mis)
			use_lamp_mis = true;
		if(light->type == LIGHT_BACKGROUND) {
			num_background_lights++;
			background_mis = light->use_mis;
		}

		light_index++;
		offset++;
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
	KernelFilm *kfilm = &dscene->data.film;
	kintegrator->use_direct_light = (totarea > 0.0f);

	if(kintegrator->use_direct_light) {
		/* number of emissives */
		kintegrator->num_distribution = num_distribution;

		/* precompute pdfs */
		kintegrator->pdf_triangles = 0.0f;
		kintegrator->pdf_lights = 0.0f;
		kintegrator->inv_pdf_lights = 0.0f;

		/* sample one, with 0.5 probability of light or triangle */
		kintegrator->num_all_lights = num_lights;

		if(trianglearea > 0.0f) {
			kintegrator->pdf_triangles = 1.0f/trianglearea;
			if(num_lights)
				kintegrator->pdf_triangles *= 0.5f;
		}

		if(num_lights) {
			kintegrator->pdf_lights = 1.0f/num_lights;
			if(trianglearea > 0.0f)
				kintegrator->pdf_lights *= 0.5f;

			kintegrator->inv_pdf_lights = 1.0f/kintegrator->pdf_lights;
		}

		kintegrator->use_lamp_mis = use_lamp_mis;

		/* bit of an ugly hack to compensate for emitting triangles influencing
		 * amount of samples we get for this pass */
		kfilm->pass_shadow_scale = 1.0f;

		if(kintegrator->pdf_triangles != 0.0f)
			kfilm->pass_shadow_scale *= 0.5f;

		if(num_background_lights < num_lights)
			kfilm->pass_shadow_scale *= (float)(num_lights - num_background_lights)/(float)num_lights;

		/* CDF */
		device->tex_alloc("__light_distribution", dscene->light_distribution);

		/* Portals */
		if(num_portals > 0) {
			kintegrator->portal_offset = light_index;
			kintegrator->num_portals = num_portals;
			kintegrator->portal_pdf = background_mis? 0.5f: 1.0f;
		}
		else {
			kintegrator->num_portals = 0;
			kintegrator->portal_offset = 0;
			kintegrator->portal_pdf = 0.0f;
		}
	}
	else {
		dscene->light_distribution.clear();

		kintegrator->num_distribution = 0;
		kintegrator->num_all_lights = 0;
		kintegrator->pdf_triangles = 0.0f;
		kintegrator->pdf_lights = 0.0f;
		kintegrator->inv_pdf_lights = 0.0f;
		kintegrator->use_lamp_mis = false;
		kintegrator->num_portals = 0;
		kintegrator->portal_offset = 0;
		kintegrator->portal_pdf = 0.0f;

		kfilm->pass_shadow_scale = 1.0f;
	}
}

static void background_cdf(int start,
                           int end,
                           int res,
                           int cdf_count,
                           const vector<float3> *pixels,
                           float2 *cond_cdf)
{
	/* Conditional CDFs (rows, U direction). */
	for(int i = start; i < end; i++) {
		float sin_theta = sinf(M_PI_F * (i + 0.5f) / res);
		float3 env_color = (*pixels)[i * res];
		float ave_luminance = average(env_color);

		cond_cdf[i * cdf_count].x = ave_luminance * sin_theta;
		cond_cdf[i * cdf_count].y = 0.0f;

		for(int j = 1; j < res; j++) {
			env_color = (*pixels)[i * res + j];
			ave_luminance = average(env_color);

			cond_cdf[i * cdf_count + j].x = ave_luminance * sin_theta;
			cond_cdf[i * cdf_count + j].y = cond_cdf[i * cdf_count + j - 1].y + cond_cdf[i * cdf_count + j - 1].x / res;
		}

		float cdf_total = cond_cdf[i * cdf_count + res - 1].y + cond_cdf[i * cdf_count + res - 1].x / res;
		float cdf_total_inv = 1.0f / cdf_total;

		/* stuff the total into the brightness value for the last entry, because
		 * we are going to normalize the CDFs to 0.0 to 1.0 afterwards */
		cond_cdf[i * cdf_count + res].x = cdf_total;

		if(cdf_total > 0.0f)
			for(int j = 1; j < res; j++)
				cond_cdf[i * cdf_count + j].y *= cdf_total_inv;

		cond_cdf[i * cdf_count + res].y = 1.0f;
	}
}

void LightManager::device_update_background(Device *device,
                                            DeviceScene *dscene,
                                            Scene *scene,
                                            Progress& progress)
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
	if(!background_light || !background_light->is_enabled) {
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
	shade_background_pixels(device, dscene, res, pixels, progress);

	if(progress.get_cancel())
		return;

	/* build row distributions and column distribution for the infinite area environment light */
	int cdf_count = res + 1;
	float2 *marg_cdf = dscene->light_background_marginal_cdf.resize(cdf_count);
	float2 *cond_cdf = dscene->light_background_conditional_cdf.resize(cdf_count * cdf_count);

	double time_start = time_dt();
	if(res < 512) {
		/* Small enough resolution, faster to do single-threaded. */
		background_cdf(0, res, res, cdf_count, &pixels, cond_cdf);
	}
	else {
		/* Threaded evaluation for large resolution. */
		const int num_blocks = TaskScheduler::num_threads();
		const int chunk_size = res / num_blocks;
		int start_row = 0;
		TaskPool pool;
		for(int i = 0; i < num_blocks; ++i) {
			const int current_chunk_size =
			    (i != num_blocks - 1) ? chunk_size
			                          : (res - i * chunk_size);
			pool.push(function_bind(&background_cdf,
			                        start_row, start_row + current_chunk_size,
			                        res,
			                        cdf_count,
			                        &pixels,
			                        cond_cdf));
			start_row += current_chunk_size;
		}
		pool.wait_work();
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

	VLOG(2) << "Background MIS build time " << time_dt() - time_start << "\n";

	/* update device */
	device->tex_alloc("__light_background_marginal_cdf", dscene->light_background_marginal_cdf);
	device->tex_alloc("__light_background_conditional_cdf", dscene->light_background_conditional_cdf);
}

void LightManager::device_update_points(Device *device,
                                        DeviceScene *dscene,
                                        Scene *scene)
{
	int num_scene_lights = scene->lights.size();

	int num_lights = 0;
	foreach(Light *light, scene->lights) {
		if(light->is_enabled || light->is_portal) {
			num_lights++;
		}
	}

	float4 *light_data = dscene->light_data.resize(num_lights*LIGHT_SIZE);

	if(num_lights == 0) {
		VLOG(1) << "No effective light, ignoring points update.";
		return;
	}

	int light_index = 0;

	foreach(Light *light, scene->lights) {
		if(!light->is_enabled) {
			continue;
		}

		float3 co = light->co;
		Shader *shader = (light->shader) ? light->shader : scene->default_light;
		int shader_id = scene->shader_manager->get_shader_id(shader);
		float samples = __int_as_float(light->samples);
		float max_bounces = __int_as_float(light->max_bounces);

		if(!light->cast_shadow)
			shader_id &= ~SHADER_CAST_SHADOW;

		if(!light->use_diffuse) {
			shader_id |= SHADER_EXCLUDE_DIFFUSE;
			use_light_visibility = true;
		}
		if(!light->use_glossy) {
			shader_id |= SHADER_EXCLUDE_GLOSSY;
			use_light_visibility = true;
		}
		if(!light->use_transmission) {
			shader_id |= SHADER_EXCLUDE_TRANSMIT;
			use_light_visibility = true;
		}
		if(!light->use_scatter) {
			shader_id |= SHADER_EXCLUDE_SCATTER;
			use_light_visibility = true;
		}

		if(light->type == LIGHT_POINT) {
			shader_id &= ~SHADER_AREA_LIGHT;

			float radius = light->size;
			float invarea = (radius > 0.0f)? 1.0f/(M_PI_F*radius*radius): 1.0f;

			if(light->use_mis && radius > 0.0f)
				shader_id |= SHADER_USE_MIS;

			light_data[light_index*LIGHT_SIZE + 0] = make_float4(__int_as_float(light->type), co.x, co.y, co.z);
			light_data[light_index*LIGHT_SIZE + 1] = make_float4(__int_as_float(shader_id), radius, invarea, 0.0f);
			light_data[light_index*LIGHT_SIZE + 2] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
			light_data[light_index*LIGHT_SIZE + 3] = make_float4(samples, 0.0f, 0.0f, 0.0f);
		}
		else if(light->type == LIGHT_DISTANT) {
			shader_id &= ~SHADER_AREA_LIGHT;

			float radius = light->size;
			float angle = atanf(radius);
			float cosangle = cosf(angle);
			float area = M_PI_F*radius*radius;
			float invarea = (area > 0.0f)? 1.0f/area: 1.0f;
			float3 dir = light->dir;

			dir = safe_normalize(dir);

			if(light->use_mis && area > 0.0f)
				shader_id |= SHADER_USE_MIS;

			light_data[light_index*LIGHT_SIZE + 0] = make_float4(__int_as_float(light->type), dir.x, dir.y, dir.z);
			light_data[light_index*LIGHT_SIZE + 1] = make_float4(__int_as_float(shader_id), radius, cosangle, invarea);
			light_data[light_index*LIGHT_SIZE + 2] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
			light_data[light_index*LIGHT_SIZE + 3] = make_float4(samples, 0.0f, 0.0f, 0.0f);
		}
		else if(light->type == LIGHT_BACKGROUND) {
			uint visibility = scene->background->visibility;

			shader_id &= ~SHADER_AREA_LIGHT;
			shader_id |= SHADER_USE_MIS;

			if(!(visibility & PATH_RAY_DIFFUSE)) {
				shader_id |= SHADER_EXCLUDE_DIFFUSE;
				use_light_visibility = true;
			}
			if(!(visibility & PATH_RAY_GLOSSY)) {
				shader_id |= SHADER_EXCLUDE_GLOSSY;
				use_light_visibility = true;
			}
			if(!(visibility & PATH_RAY_TRANSMIT)) {
				shader_id |= SHADER_EXCLUDE_TRANSMIT;
				use_light_visibility = true;
			}
			if(!(visibility & PATH_RAY_VOLUME_SCATTER)) {
				shader_id |= SHADER_EXCLUDE_SCATTER;
				use_light_visibility = true;
			}

			light_data[light_index*LIGHT_SIZE + 0] = make_float4(__int_as_float(light->type), 0.0f, 0.0f, 0.0f);
			light_data[light_index*LIGHT_SIZE + 1] = make_float4(__int_as_float(shader_id), 0.0f, 0.0f, 0.0f);
			light_data[light_index*LIGHT_SIZE + 2] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
			light_data[light_index*LIGHT_SIZE + 3] = make_float4(samples, 0.0f, 0.0f, 0.0f);
		}
		else if(light->type == LIGHT_AREA) {
			float3 axisu = light->axisu*(light->sizeu*light->size);
			float3 axisv = light->axisv*(light->sizev*light->size);
			float area = len(axisu)*len(axisv);
			float invarea = (area > 0.0f)? 1.0f/area: 1.0f;
			float3 dir = light->dir;
			
			dir = safe_normalize(dir);

			if(light->use_mis && area > 0.0f)
				shader_id |= SHADER_USE_MIS;

			light_data[light_index*LIGHT_SIZE + 0] = make_float4(__int_as_float(light->type), co.x, co.y, co.z);
			light_data[light_index*LIGHT_SIZE + 1] = make_float4(__int_as_float(shader_id), axisu.x, axisu.y, axisu.z);
			light_data[light_index*LIGHT_SIZE + 2] = make_float4(invarea, axisv.x, axisv.y, axisv.z);
			light_data[light_index*LIGHT_SIZE + 3] = make_float4(samples, dir.x, dir.y, dir.z);
		}
		else if(light->type == LIGHT_SPOT) {
			shader_id &= ~SHADER_AREA_LIGHT;

			float radius = light->size;
			float invarea = (radius > 0.0f)? 1.0f/(M_PI_F*radius*radius): 1.0f;
			float spot_angle = cosf(light->spot_angle*0.5f);
			float spot_smooth = (1.0f - spot_angle)*light->spot_smooth;
			float3 dir = light->dir;
			
			dir = safe_normalize(dir);

			if(light->use_mis && radius > 0.0f)
				shader_id |= SHADER_USE_MIS;

			light_data[light_index*LIGHT_SIZE + 0] = make_float4(__int_as_float(light->type), co.x, co.y, co.z);
			light_data[light_index*LIGHT_SIZE + 1] = make_float4(__int_as_float(shader_id), radius, invarea, spot_angle);
			light_data[light_index*LIGHT_SIZE + 2] = make_float4(spot_smooth, dir.x, dir.y, dir.z);
			light_data[light_index*LIGHT_SIZE + 3] = make_float4(samples, 0.0f, 0.0f, 0.0f);
		}

		light_data[light_index*LIGHT_SIZE + 4] = make_float4(max_bounces, 0.0f, 0.0f, 0.0f);

		Transform tfm = light->tfm;
		Transform itfm = transform_inverse(tfm);
		memcpy(&light_data[light_index*LIGHT_SIZE + 5], &tfm, sizeof(float4)*3);
		memcpy(&light_data[light_index*LIGHT_SIZE + 8], &itfm, sizeof(float4)*3);

		light_index++;
	}

	/* TODO(sergey): Consider moving portals update to their own function
	 * keeping this one more manageable.
	 */
	foreach(Light *light, scene->lights) {
		if(!light->is_portal)
			continue;
		assert(light->type == LIGHT_AREA);

		float3 co = light->co;
		float3 axisu = light->axisu*(light->sizeu*light->size);
		float3 axisv = light->axisv*(light->sizev*light->size);
		float area = len(axisu)*len(axisv);
		float invarea = (area > 0.0f) ? 1.0f / area : 1.0f;
		float3 dir = light->dir;

		dir = safe_normalize(dir);

		light_data[light_index*LIGHT_SIZE + 0] = make_float4(__int_as_float(light->type), co.x, co.y, co.z);
		light_data[light_index*LIGHT_SIZE + 1] = make_float4(area, axisu.x, axisu.y, axisu.z);
		light_data[light_index*LIGHT_SIZE + 2] = make_float4(invarea, axisv.x, axisv.y, axisv.z);
		light_data[light_index*LIGHT_SIZE + 3] = make_float4(-1, dir.x, dir.y, dir.z);
		light_data[light_index*LIGHT_SIZE + 4] = make_float4(-1, 0.0f, 0.0f, 0.0f);

		Transform tfm = light->tfm;
		Transform itfm = transform_inverse(tfm);
		memcpy(&light_data[light_index*LIGHT_SIZE + 5], &tfm, sizeof(float4)*3);
		memcpy(&light_data[light_index*LIGHT_SIZE + 8], &itfm, sizeof(float4)*3);

		light_index++;
	}

	VLOG(1) << "Number of lights sent to the device: " << light_index;

	VLOG(1) << "Number of lights without contribution: "
	        << num_scene_lights - light_index;

	device->tex_alloc("__light_data", dscene->light_data);
}

void LightManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;

	VLOG(1) << "Total " << scene->lights.size() << " lights.";

	device_free(device, dscene);

	use_light_visibility = false;

	disable_ineffective_light(device, scene);

	device_update_points(device, dscene, scene);
	if(progress.get_cancel()) return;

	device_update_distribution(device, dscene, scene, progress);
	if(progress.get_cancel()) return;

	device_update_background(device, dscene, scene, progress);
	if(progress.get_cancel()) return;

	if(use_light_visibility != scene->film->use_light_visibility) {
		scene->film->use_light_visibility = use_light_visibility;
		scene->film->tag_update(scene);
	}

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

void LightManager::tag_update(Scene * /*scene*/)
{
	need_update = true;
}

CCL_NAMESPACE_END


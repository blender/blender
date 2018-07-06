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

#include "render/camera.h"
#include "device/device.h"
#include "render/film.h"
#include "render/integrator.h"
#include "render/mesh.h"
#include "render/scene.h"
#include "render/tables.h"

#include "util/util_algorithm.h"
#include "util/util_foreach.h"
#include "util/util_math.h"
#include "util/util_math_cdf.h"

CCL_NAMESPACE_BEGIN

/* Pass */

static bool compare_pass_order(const Pass& a, const Pass& b)
{
	if(a.components == b.components)
		return (a.type < b.type);
	return (a.components > b.components);
}

void Pass::add(PassType type, array<Pass>& passes)
{
	for(size_t i = 0; i < passes.size(); i++)
		if(passes[i].type == type)
			return;

	Pass pass;

	pass.type = type;
	pass.filter = true;
	pass.exposure = false;
	pass.divide_type = PASS_NONE;

	switch(type) {
		case PASS_NONE:
			pass.components = 0;
			break;
		case PASS_COMBINED:
			pass.components = 4;
			pass.exposure = true;
			break;
		case PASS_DEPTH:
			pass.components = 1;
			pass.filter = false;
			break;
		case PASS_MIST:
			pass.components = 1;
			break;
		case PASS_NORMAL:
			pass.components = 4;
			break;
		case PASS_UV:
			pass.components = 4;
			break;
		case PASS_MOTION:
			pass.components = 4;
			pass.divide_type = PASS_MOTION_WEIGHT;
			break;
		case PASS_MOTION_WEIGHT:
			pass.components = 1;
			break;
		case PASS_OBJECT_ID:
		case PASS_MATERIAL_ID:
			pass.components = 1;
			pass.filter = false;
			break;

		case PASS_EMISSION:
		case PASS_BACKGROUND:
			pass.components = 4;
			pass.exposure = true;
			break;
		case PASS_AO:
			pass.components = 4;
			break;
		case PASS_SHADOW:
			pass.components = 4;
			pass.exposure = false;
			break;
		case PASS_LIGHT:
			/* This isn't a real pass, used by baking to see whether
			 * light data is needed or not.
			 *
			 * Set components to 0 so pass sort below happens in a
			 * determined way.
			 */
			pass.components = 0;
			break;
#ifdef WITH_CYCLES_DEBUG
		case PASS_BVH_TRAVERSED_NODES:
		case PASS_BVH_TRAVERSED_INSTANCES:
		case PASS_BVH_INTERSECTIONS:
		case PASS_RAY_BOUNCES:
			pass.components = 1;
			pass.exposure = false;
			break;
#endif
		case PASS_RENDER_TIME:
			/* This pass is handled entirely on the host side. */
			pass.components = 0;
			break;

		case PASS_DIFFUSE_COLOR:
		case PASS_GLOSSY_COLOR:
		case PASS_TRANSMISSION_COLOR:
		case PASS_SUBSURFACE_COLOR:
			pass.components = 4;
			break;
		case PASS_DIFFUSE_DIRECT:
		case PASS_DIFFUSE_INDIRECT:
			pass.components = 4;
			pass.exposure = true;
			pass.divide_type = PASS_DIFFUSE_COLOR;
			break;
		case PASS_GLOSSY_DIRECT:
		case PASS_GLOSSY_INDIRECT:
			pass.components = 4;
			pass.exposure = true;
			pass.divide_type = PASS_GLOSSY_COLOR;
			break;
		case PASS_TRANSMISSION_DIRECT:
		case PASS_TRANSMISSION_INDIRECT:
			pass.components = 4;
			pass.exposure = true;
			pass.divide_type = PASS_TRANSMISSION_COLOR;
			break;
		case PASS_SUBSURFACE_DIRECT:
		case PASS_SUBSURFACE_INDIRECT:
			pass.components = 4;
			pass.exposure = true;
			pass.divide_type = PASS_SUBSURFACE_COLOR;
			break;
		case PASS_VOLUME_DIRECT:
		case PASS_VOLUME_INDIRECT:
			pass.components = 4;
			pass.exposure = true;
			break;

		default:
			assert(false);
			break;
	}

	passes.push_back_slow(pass);

	/* order from by components, to ensure alignment so passes with size 4
	 * come first and then passes with size 1 */
	sort(&passes[0], &passes[0] + passes.size(), compare_pass_order);

	if(pass.divide_type != PASS_NONE)
		Pass::add(pass.divide_type, passes);
}

bool Pass::equals(const array<Pass>& A, const array<Pass>& B)
{
	if(A.size() != B.size())
		return false;

	for(int i = 0; i < A.size(); i++)
		if(A[i].type != B[i].type)
			return false;

	return true;
}

bool Pass::contains(const array<Pass>& passes, PassType type)
{
	for(size_t i = 0; i < passes.size(); i++)
		if(passes[i].type == type)
			return true;

	return false;
}

/* Pixel Filter */

static float filter_func_box(float /*v*/, float /*width*/)
{
	return 1.0f;
}

static float filter_func_gaussian(float v, float width)
{
	v *= 6.0f/width;
	return expf(-2.0f*v*v);
}

static float filter_func_blackman_harris(float v, float width)
{
	v = M_2PI_F * (v / width + 0.5f);
	return 0.35875f - 0.48829f*cosf(v) + 0.14128f*cosf(2.0f*v) - 0.01168f*cosf(3.0f*v);
}

static vector<float> filter_table(FilterType type, float width)
{
	vector<float> filter_table(FILTER_TABLE_SIZE);
	float (*filter_func)(float, float) = NULL;

	switch(type) {
		case FILTER_BOX:
			filter_func = filter_func_box;
			break;
		case FILTER_GAUSSIAN:
			filter_func = filter_func_gaussian;
			width *= 3.0f;
			break;
		case FILTER_BLACKMAN_HARRIS:
			filter_func = filter_func_blackman_harris;
			width *= 2.0f;
			break;
		default:
			assert(0);
	}

	/* Create importance sampling table. */

	/* TODO(sergey): With the even filter table size resolution we can not
	 * really make it nice symmetric importance map without sampling full range
	 * (meaning, we would need to sample full filter range and not use the
	 * make_symmetric argument).
	 *
	 * Current code matches exactly initial filter table code, but we should
	 * consider either making FILTER_TABLE_SIZE odd value or sample full filter.
	 */

	util_cdf_inverted(FILTER_TABLE_SIZE,
	                  0.0f,
	                  width * 0.5f,
	                  function_bind(filter_func, _1, width),
	                  true,
	                  filter_table);

	return filter_table;
}

/* Film */

NODE_DEFINE(Film)
{
	NodeType* type = NodeType::add("film", create);

	SOCKET_FLOAT(exposure, "Exposure", 0.8f);
	SOCKET_FLOAT(pass_alpha_threshold, "Pass Alpha Threshold", 0.5f);

	static NodeEnum filter_enum;
	filter_enum.insert("box", FILTER_BOX);
	filter_enum.insert("gaussian", FILTER_GAUSSIAN);
	filter_enum.insert("blackman_harris", FILTER_BLACKMAN_HARRIS);

	SOCKET_ENUM(filter_type, "Filter Type", filter_enum, FILTER_BOX);
	SOCKET_FLOAT(filter_width, "Filter Width", 1.0f);

	SOCKET_FLOAT(mist_start, "Mist Start", 0.0f);
	SOCKET_FLOAT(mist_depth, "Mist Depth", 100.0f);
	SOCKET_FLOAT(mist_falloff, "Mist Falloff", 1.0f);

	SOCKET_BOOLEAN(use_sample_clamp, "Use Sample Clamp", false);

	SOCKET_BOOLEAN(denoising_data_pass,  "Generate Denoising Data Pass",  false);
	SOCKET_BOOLEAN(denoising_clean_pass, "Generate Denoising Clean Pass", false);
	SOCKET_INT(denoising_flags, "Denoising Flags", 0);

	return type;
}

Film::Film()
: Node(node_type)
{
	Pass::add(PASS_COMBINED, passes);

	use_light_visibility = false;
	filter_table_offset = TABLE_OFFSET_INVALID;

	need_update = true;
}

Film::~Film()
{
}

void Film::device_update(Device *device, DeviceScene *dscene, Scene *scene)
{
	if(!need_update)
		return;

	device_free(device, dscene, scene);

	KernelFilm *kfilm = &dscene->data.film;

	/* update __data */
	kfilm->exposure = exposure;
	kfilm->pass_flag = 0;
	kfilm->light_pass_flag = 0;
	kfilm->pass_stride = 0;
	kfilm->use_light_pass = use_light_visibility || use_sample_clamp;

	for(size_t i = 0; i < passes.size(); i++) {
		Pass& pass = passes[i];

		if(pass.type == PASS_NONE)
			continue;

		int pass_flag = (1 << (pass.type % 32));
		if(pass.type <= PASS_CATEGORY_MAIN_END) {
			kfilm->pass_flag |= pass_flag;
		}
		else {
			assert(pass.type <= PASS_CATEGORY_LIGHT_END);
			kfilm->use_light_pass = 1;
			kfilm->light_pass_flag |= pass_flag;
		}

		switch(pass.type) {
			case PASS_COMBINED:
				kfilm->pass_combined = kfilm->pass_stride;
				break;
			case PASS_DEPTH:
				kfilm->pass_depth = kfilm->pass_stride;
				break;
			case PASS_NORMAL:
				kfilm->pass_normal = kfilm->pass_stride;
				break;
			case PASS_UV:
				kfilm->pass_uv = kfilm->pass_stride;
				break;
			case PASS_MOTION:
				kfilm->pass_motion = kfilm->pass_stride;
				break;
			case PASS_MOTION_WEIGHT:
				kfilm->pass_motion_weight = kfilm->pass_stride;
				break;
			case PASS_OBJECT_ID:
				kfilm->pass_object_id = kfilm->pass_stride;
				break;
			case PASS_MATERIAL_ID:
				kfilm->pass_material_id = kfilm->pass_stride;
				break;

			case PASS_MIST:
				kfilm->pass_mist = kfilm->pass_stride;
				break;
			case PASS_EMISSION:
				kfilm->pass_emission = kfilm->pass_stride;
				break;
			case PASS_BACKGROUND:
				kfilm->pass_background = kfilm->pass_stride;
				break;
			case PASS_AO:
				kfilm->pass_ao = kfilm->pass_stride;
				break;
			case PASS_SHADOW:
				kfilm->pass_shadow = kfilm->pass_stride;
				break;

			case PASS_LIGHT:
				break;

			case PASS_DIFFUSE_COLOR:
				kfilm->pass_diffuse_color = kfilm->pass_stride;
				break;
			case PASS_GLOSSY_COLOR:
				kfilm->pass_glossy_color = kfilm->pass_stride;
				break;
			case PASS_TRANSMISSION_COLOR:
				kfilm->pass_transmission_color = kfilm->pass_stride;
				break;
			case PASS_SUBSURFACE_COLOR:
				kfilm->pass_subsurface_color = kfilm->pass_stride;
				break;
			case PASS_DIFFUSE_INDIRECT:
				kfilm->pass_diffuse_indirect = kfilm->pass_stride;
				break;
			case PASS_GLOSSY_INDIRECT:
				kfilm->pass_glossy_indirect = kfilm->pass_stride;
				break;
			case PASS_TRANSMISSION_INDIRECT:
				kfilm->pass_transmission_indirect = kfilm->pass_stride;
				break;
			case PASS_SUBSURFACE_INDIRECT:
				kfilm->pass_subsurface_indirect = kfilm->pass_stride;
				break;
			case PASS_VOLUME_INDIRECT:
				kfilm->pass_volume_indirect = kfilm->pass_stride;
				break;
			case PASS_DIFFUSE_DIRECT:
				kfilm->pass_diffuse_direct = kfilm->pass_stride;
				break;
			case PASS_GLOSSY_DIRECT:
				kfilm->pass_glossy_direct = kfilm->pass_stride;
				break;
			case PASS_TRANSMISSION_DIRECT:
				kfilm->pass_transmission_direct = kfilm->pass_stride;
				break;
			case PASS_SUBSURFACE_DIRECT:
				kfilm->pass_subsurface_direct = kfilm->pass_stride;
				break;
			case PASS_VOLUME_DIRECT:
				kfilm->pass_volume_direct = kfilm->pass_stride;
				break;

#ifdef WITH_CYCLES_DEBUG
			case PASS_BVH_TRAVERSED_NODES:
				kfilm->pass_bvh_traversed_nodes = kfilm->pass_stride;
				break;
			case PASS_BVH_TRAVERSED_INSTANCES:
				kfilm->pass_bvh_traversed_instances = kfilm->pass_stride;
				break;
			case PASS_BVH_INTERSECTIONS:
				kfilm->pass_bvh_intersections = kfilm->pass_stride;
				break;
			case PASS_RAY_BOUNCES:
				kfilm->pass_ray_bounces = kfilm->pass_stride;
				break;
#endif
			case PASS_RENDER_TIME:
				break;

			default:
				assert(false);
				break;
		}

		kfilm->pass_stride += pass.components;
	}

	kfilm->pass_denoising_data = 0;
	kfilm->pass_denoising_clean = 0;
	kfilm->denoising_flags = 0;
	if(denoising_data_pass) {
		kfilm->pass_denoising_data = kfilm->pass_stride;
		kfilm->pass_stride += DENOISING_PASS_SIZE_BASE;
		kfilm->denoising_flags = denoising_flags;
		if(denoising_clean_pass) {
			kfilm->pass_denoising_clean = kfilm->pass_stride;
			kfilm->pass_stride += DENOISING_PASS_SIZE_CLEAN;
			kfilm->use_light_pass = 1;
		}
	}

	kfilm->pass_stride = align_up(kfilm->pass_stride, 4);
	kfilm->pass_alpha_threshold = pass_alpha_threshold;

	/* update filter table */
	vector<float> table = filter_table(filter_type, filter_width);
	scene->lookup_tables->remove_table(&filter_table_offset);
	filter_table_offset = scene->lookup_tables->add_table(dscene, table);
	kfilm->filter_table_offset = (int)filter_table_offset;

	/* mist pass parameters */
	kfilm->mist_start = mist_start;
	kfilm->mist_inv_depth = (mist_depth > 0.0f)? 1.0f/mist_depth: 0.0f;
	kfilm->mist_falloff = mist_falloff;

	pass_stride = kfilm->pass_stride;
	denoising_data_offset = kfilm->pass_denoising_data;
	denoising_clean_offset = kfilm->pass_denoising_clean;

	need_update = false;
}

void Film::device_free(Device * /*device*/,
                       DeviceScene * /*dscene*/,
                       Scene *scene)
{
	scene->lookup_tables->remove_table(&filter_table_offset);
}

bool Film::modified(const Film& film)
{
	return !Node::equals(film) || !Pass::equals(passes, film.passes);
}

void Film::tag_passes_update(Scene *scene, const array<Pass>& passes_)
{
	if(Pass::contains(passes, PASS_UV) != Pass::contains(passes_, PASS_UV)) {
		scene->mesh_manager->tag_update(scene);

		foreach(Shader *shader, scene->shaders)
			shader->need_update_mesh = true;
	}
	else if(Pass::contains(passes, PASS_MOTION) != Pass::contains(passes_, PASS_MOTION))
		scene->mesh_manager->tag_update(scene);

	passes = passes_;
}

void Film::tag_update(Scene * /*scene*/)
{
	need_update = true;
}

CCL_NAMESPACE_END

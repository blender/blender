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
#include "blackbody.h"
#include "device.h"
#include "graph.h"
#include "light.h"
#include "mesh.h"
#include "nodes.h"
#include "object.h"
#include "osl.h"
#include "scene.h"
#include "shader.h"
#include "svm.h"
#include "tables.h"

#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

/* Beckmann sampling precomputed table, see bsdf_microfacet.h */

/* 2D slope distribution (alpha = 1.0) */
static float beckmann_table_P22(const float slope_x, const float slope_y)
{
	return expf(-(slope_x*slope_x + slope_y*slope_y));
}

/* maximal slope amplitude (range that contains 99.99% of the distribution) */
static float beckmann_table_slope_max()
{
	return 6.0;
}

/* Paper used: Importance Sampling Microfacet-Based BSDFs with the
 * Distribution of Visible Normals. Supplemental Material 2/2.
 *
 * http://hal.inria.fr/docs/01/00/66/20/ANNEX/supplemental2.pdf
 */
static void beckmann_table_rows(float *table, int row_from, int row_to)
{
	/* allocate temporary data */
	const int DATA_TMP_SIZE = 512;
	vector<double> slope_x(DATA_TMP_SIZE);
	vector<double> CDF_P22_omega_i(DATA_TMP_SIZE);

	/* loop over incident directions */
	for(int index_theta = row_from; index_theta < row_to; index_theta++) {
		/* incident vector */
		const float cos_theta = index_theta / (BECKMANN_TABLE_SIZE - 1.0f);
		const float sin_theta = safe_sqrtf(1.0f - cos_theta*cos_theta);

		/* for a given incident vector
		 * integrate P22_{omega_i}(x_slope, 1, 1), Eq. (10) */
		slope_x[0] = -beckmann_table_slope_max();
		CDF_P22_omega_i[0] = 0;

		for(int index_slope_x = 1; index_slope_x < DATA_TMP_SIZE; ++index_slope_x) {
			/* slope_x */
			slope_x[index_slope_x] = -beckmann_table_slope_max() + 2.0f * beckmann_table_slope_max() * index_slope_x/(DATA_TMP_SIZE - 1.0f);

			/* dot product with incident vector */
			float dot_product = fmaxf(0.0f, -(float)slope_x[index_slope_x]*sin_theta + cos_theta);
			/* marginalize P22_{omega_i}(x_slope, 1, 1), Eq. (10) */
			float P22_omega_i = 0.0f;

			for(int j = 0; j < 100; ++j) {
				float slope_y = -beckmann_table_slope_max() + 2.0f * beckmann_table_slope_max() * j * (1.0f/99.0f);
				P22_omega_i += dot_product * beckmann_table_P22((float)slope_x[index_slope_x], slope_y);
			}

			/* CDF of P22_{omega_i}(x_slope, 1, 1), Eq. (10) */
			CDF_P22_omega_i[index_slope_x] = CDF_P22_omega_i[index_slope_x - 1] + (double)P22_omega_i;
		}

		/* renormalize CDF_P22_omega_i */
		for(int index_slope_x = 1; index_slope_x < DATA_TMP_SIZE; ++index_slope_x)
			CDF_P22_omega_i[index_slope_x] /= CDF_P22_omega_i[DATA_TMP_SIZE - 1];

		/* loop over random number U1 */
		int index_slope_x = 0;

		for(int index_U = 0; index_U < BECKMANN_TABLE_SIZE; ++index_U) {
			const double U = 0.0000001 + 0.9999998 * index_U / (double)(BECKMANN_TABLE_SIZE - 1);

			/* inverse CDF_P22_omega_i, solve Eq.(11) */
			while(CDF_P22_omega_i[index_slope_x] <= U)
				++index_slope_x;

			const double interp =
				(CDF_P22_omega_i[index_slope_x] - U) /
				(CDF_P22_omega_i[index_slope_x] - CDF_P22_omega_i[index_slope_x - 1]);

			/* store value */
			table[index_U + index_theta*BECKMANN_TABLE_SIZE] = (float)(
				interp * slope_x[index_slope_x - 1] +
				    (1.0 - interp) * slope_x[index_slope_x]);
		}
	}
}

static void beckmann_table_build(vector<float>& table)
{
	table.resize(BECKMANN_TABLE_SIZE*BECKMANN_TABLE_SIZE);

	/* multithreaded build */
	TaskPool pool;

	for(int i = 0; i < BECKMANN_TABLE_SIZE; i+=8)
		pool.push(function_bind(&beckmann_table_rows, &table[0], i, i+8));

	pool.wait_work();
}

/* Shader */

Shader::Shader()
{
	name = "";
	pass_id = 0;

	graph = NULL;
	graph_bump = NULL;

	use_mis = true;
	use_transparent_shadow = true;
	heterogeneous_volume = true;
	volume_sampling_method = VOLUME_SAMPLING_DISTANCE;
	volume_interpolation_method = VOLUME_INTERPOLATION_LINEAR;

	has_surface = false;
	has_surface_transparent = false;
	has_surface_emission = false;
	has_surface_bssrdf = false;
	has_converter_blackbody = false;
	has_volume = false;
	has_displacement = false;
	has_bssrdf_bump = false;
	has_heterogeneous_volume = false;

	used = false;

	need_update = true;
	need_update_attributes = true;
}

Shader::~Shader()
{
	delete graph;
	delete graph_bump;
}

void Shader::set_graph(ShaderGraph *graph_)
{
	/* do this here already so that we can detect if mesh or object attributes
	 * are needed, since the node attribute callbacks check if their sockets
	 * are connected but proxy nodes should not count */
	if(graph_)
		graph_->remove_unneeded_nodes();

	/* assign graph */
	delete graph;
	delete graph_bump;
	graph = graph_;
	graph_bump = NULL;
}

void Shader::tag_update(Scene *scene)
{
	/* update tag */
	need_update = true;
	scene->shader_manager->need_update = true;

	/* if the shader previously was emissive, update light distribution,
	 * if the new shader is emissive, a light manager update tag will be
	 * done in the shader manager device update. */
	if(use_mis && has_surface_emission)
		scene->light_manager->need_update = true;

	/* quick detection of which kind of shaders we have to avoid loading
	 * e.g. surface attributes when there is only a volume shader. this could
	 * be more fine grained but it's better than nothing */
	OutputNode *output = graph->output();
	bool prev_has_volume = has_volume;
	has_surface = has_surface || output->input("Surface")->link;
	has_volume = has_volume || output->input("Volume")->link;
	has_displacement = has_displacement || output->input("Displacement")->link;

	/* get requested attributes. this could be optimized by pruning unused
	 * nodes here already, but that's the job of the shader manager currently,
	 * and may not be so great for interactive rendering where you temporarily
	 * disconnect a node */

	AttributeRequestSet prev_attributes = attributes;

	attributes.clear();
	foreach(ShaderNode *node, graph->nodes)
		node->attributes(this, &attributes);
	
	/* compare if the attributes changed, mesh manager will check
	 * need_update_attributes, update the relevant meshes and clear it. */
	if(attributes.modified(prev_attributes)) {
		need_update_attributes = true;
		scene->mesh_manager->need_update = true;
	}

	if(has_volume != prev_has_volume) {
		scene->mesh_manager->need_flags_update = true;
		scene->object_manager->need_flags_update = true;
	}
}

void Shader::tag_used(Scene *scene)
{
	/* if an unused shader suddenly gets used somewhere, it needs to be
	 * recompiled because it was skipped for compilation before */
	if(!used) {
		need_update = true;
		scene->shader_manager->need_update = true;
	}
}

/* Shader Manager */

ShaderManager::ShaderManager()
{
	need_update = true;
	blackbody_table_offset = TABLE_OFFSET_INVALID;
	beckmann_table_offset = TABLE_OFFSET_INVALID;
}

ShaderManager::~ShaderManager()
{
}

ShaderManager *ShaderManager::create(Scene *scene, int shadingsystem)
{
	ShaderManager *manager;

#ifdef WITH_OSL
	if(shadingsystem == SHADINGSYSTEM_OSL)
		manager = new OSLShaderManager();
	else
#endif
		manager = new SVMShaderManager();
	
	add_default(scene);

	return manager;
}

uint ShaderManager::get_attribute_id(ustring name)
{
	/* get a unique id for each name, for SVM attribute lookup */
	AttributeIDMap::iterator it = unique_attribute_id.find(name);

	if(it != unique_attribute_id.end())
		return it->second;
	
	uint id = (uint)ATTR_STD_NUM + unique_attribute_id.size();
	unique_attribute_id[name] = id;
	return id;
}

uint ShaderManager::get_attribute_id(AttributeStandard std)
{
	return (uint)std;
}

int ShaderManager::get_shader_id(uint shader, Mesh *mesh, bool smooth)
{
	/* get a shader id to pass to the kernel */
	int id = shader*2;
	
	/* index depends bump since this setting is not in the shader */
	if(mesh && mesh->displacement_method != Mesh::DISPLACE_TRUE)
		id += 1;
	/* smooth flag */
	if(smooth)
		id |= SHADER_SMOOTH_NORMAL;
	
	/* default flags */
	id |= SHADER_CAST_SHADOW|SHADER_AREA_LIGHT;
	
	return id;
}

void ShaderManager::device_update_shaders_used(Scene *scene)
{
	/* figure out which shaders are in use, so SVM/OSL can skip compiling them
	 * for speed and avoid loading image textures into memory */
	foreach(Shader *shader, scene->shaders)
		shader->used = false;

	scene->shaders[scene->background->shader]->used = true;
	scene->shaders[scene->default_surface]->used = true;
	scene->shaders[scene->default_light]->used = true;
	scene->shaders[scene->default_background]->used = true;
	scene->shaders[scene->default_empty]->used = true;

	foreach(Mesh *mesh, scene->meshes)
		foreach(uint shader, mesh->used_shaders)
			scene->shaders[shader]->used = true;

	foreach(Light *light, scene->lights)
		scene->shaders[light->shader]->used = true;
}

void ShaderManager::device_update_common(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	device->tex_free(dscene->shader_flag);
	dscene->shader_flag.clear();

	if(scene->shaders.size() == 0)
		return;

	uint shader_flag_size = scene->shaders.size()*4;
	uint *shader_flag = dscene->shader_flag.resize(shader_flag_size);
	uint i = 0;
	bool has_converter_blackbody = false;
	bool has_volumes = false;

	foreach(Shader *shader, scene->shaders) {
		uint flag = 0;

		if(shader->use_mis)
			flag |= SD_USE_MIS;
		if(shader->has_surface_transparent && shader->use_transparent_shadow)
			flag |= SD_HAS_TRANSPARENT_SHADOW;
		if(shader->has_volume) {
			flag |= SD_HAS_VOLUME;
			has_volumes = true;

			/* in this case we can assume transparent surface */
			if(!shader->has_surface)
				flag |= SD_HAS_ONLY_VOLUME;

			/* todo: this could check more fine grained, to skip useless volumes
			 * enclosed inside an opaque bsdf, although we still need to handle
			 * the case with camera inside volumes too */
			flag |= SD_HAS_TRANSPARENT_SHADOW;
		}
		if(shader->heterogeneous_volume && shader->has_heterogeneous_volume)
			flag |= SD_HETEROGENEOUS_VOLUME;
		if(shader->has_bssrdf_bump)
			flag |= SD_HAS_BSSRDF_BUMP;
		if(shader->has_converter_blackbody)
			has_converter_blackbody = true;
		if(shader->volume_sampling_method == VOLUME_SAMPLING_EQUIANGULAR)
			flag |= SD_VOLUME_EQUIANGULAR;
		if(shader->volume_sampling_method == VOLUME_SAMPLING_MULTIPLE_IMPORTANCE)
			flag |= SD_VOLUME_MIS;
		if(shader->volume_interpolation_method == VOLUME_INTERPOLATION_CUBIC)
			flag |= SD_VOLUME_CUBIC;
		if(shader->graph_bump)
			flag |= SD_HAS_BUMP;

		/* regular shader */
		shader_flag[i++] = flag;
		shader_flag[i++] = shader->pass_id;

		/* shader with bump mapping */
		if(shader->graph_bump)
			flag |= SD_HAS_BSSRDF_BUMP;

		shader_flag[i++] = flag;
		shader_flag[i++] = shader->pass_id;
	}

	device->tex_alloc("__shader_flag", dscene->shader_flag);

	/* blackbody lookup table */
	KernelTables *ktables = &dscene->data.tables;
	
	if(has_converter_blackbody && blackbody_table_offset == TABLE_OFFSET_INVALID) {
		vector<float> table = blackbody_table();
		blackbody_table_offset = scene->lookup_tables->add_table(dscene, table);
		
		ktables->blackbody_offset = (int)blackbody_table_offset;
	}
	else if(!has_converter_blackbody && blackbody_table_offset != TABLE_OFFSET_INVALID) {
		scene->lookup_tables->remove_table(blackbody_table_offset);
		blackbody_table_offset = TABLE_OFFSET_INVALID;
	}

	/* beckmann lookup table */
	if(beckmann_table_offset == TABLE_OFFSET_INVALID) {
		vector<float> table;
		beckmann_table_build(table);
		beckmann_table_offset = scene->lookup_tables->add_table(dscene, table);
		
		ktables->beckmann_offset = (int)beckmann_table_offset;
	}

	/* integrator */
	KernelIntegrator *kintegrator = &dscene->data.integrator;
	kintegrator->use_volumes = has_volumes;
}

void ShaderManager::device_free_common(Device *device, DeviceScene *dscene, Scene *scene)
{
	if(blackbody_table_offset != TABLE_OFFSET_INVALID) {
		scene->lookup_tables->remove_table(blackbody_table_offset);
		blackbody_table_offset = TABLE_OFFSET_INVALID;
	}

	if(beckmann_table_offset != TABLE_OFFSET_INVALID) {
		scene->lookup_tables->remove_table(beckmann_table_offset);
		beckmann_table_offset = TABLE_OFFSET_INVALID;
	}

	device->tex_free(dscene->shader_flag);
	dscene->shader_flag.clear();
}

void ShaderManager::add_default(Scene *scene)
{
	Shader *shader;
	ShaderGraph *graph;
	ShaderNode *closure, *out;

	/* default surface */
	{
		graph = new ShaderGraph();

		closure = graph->add(new DiffuseBsdfNode());
		closure->input("Color")->value = make_float3(0.8f, 0.8f, 0.8f);
		out = graph->output();

		graph->connect(closure->output("BSDF"), out->input("Surface"));

		shader = new Shader();
		shader->name = "default_surface";
		shader->graph = graph;
		scene->shaders.push_back(shader);
		scene->default_surface = scene->shaders.size() - 1;
	}

	/* default light */
	{
		graph = new ShaderGraph();

		closure = graph->add(new EmissionNode());
		closure->input("Color")->value = make_float3(0.8f, 0.8f, 0.8f);
		closure->input("Strength")->value.x = 0.0f;
		out = graph->output();

		graph->connect(closure->output("Emission"), out->input("Surface"));

		shader = new Shader();
		shader->name = "default_light";
		shader->graph = graph;
		scene->shaders.push_back(shader);
		scene->default_light = scene->shaders.size() - 1;
	}

	/* default background */
	{
		graph = new ShaderGraph();

		shader = new Shader();
		shader->name = "default_background";
		shader->graph = graph;
		scene->shaders.push_back(shader);
		scene->default_background = scene->shaders.size() - 1;
	}

	/* default empty */
	{
		graph = new ShaderGraph();

		shader = new Shader();
		shader->name = "default_empty";
		shader->graph = graph;
		scene->shaders.push_back(shader);
		scene->default_empty = scene->shaders.size() - 1;
	}
}

CCL_NAMESPACE_END


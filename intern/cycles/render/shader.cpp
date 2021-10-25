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
#include "render/camera.h"
#include "device/device.h"
#include "render/graph.h"
#include "render/integrator.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/nodes.h"
#include "render/object.h"
#include "render/osl.h"
#include "render/scene.h"
#include "render/shader.h"
#include "render/svm.h"
#include "render/tables.h"

#include "util/util_foreach.h"

CCL_NAMESPACE_BEGIN

thread_mutex ShaderManager::lookup_table_mutex;
vector<float> ShaderManager::beckmann_table;
bool ShaderManager::beckmann_table_ready = false;

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


/* MSVC 2015 needs this ugly hack to prevent a codegen bug on x86
 * see T50176 for details
 */
#if defined(_MSC_VER) && (_MSC_VER == 1900)
#  define MSVC_VOLATILE volatile
#else
#  define MSVC_VOLATILE
#endif

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
		slope_x[0] = (double)-beckmann_table_slope_max();
		CDF_P22_omega_i[0] = 0;

		for(MSVC_VOLATILE int index_slope_x = 1; index_slope_x < DATA_TMP_SIZE; ++index_slope_x) {
			/* slope_x */
			slope_x[index_slope_x] = (double)(-beckmann_table_slope_max() + 2.0f * beckmann_table_slope_max() * index_slope_x/(DATA_TMP_SIZE - 1.0f));

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

#undef MSVC_VOLATILE

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

NODE_DEFINE(Shader)
{
	NodeType* type = NodeType::add("shader", create);

	SOCKET_BOOLEAN(use_mis, "Use MIS", true);
	SOCKET_BOOLEAN(use_transparent_shadow, "Use Transparent Shadow", true);
	SOCKET_BOOLEAN(heterogeneous_volume, "Heterogeneous Volume", true);

	static NodeEnum volume_sampling_method_enum;
	volume_sampling_method_enum.insert("distance", VOLUME_SAMPLING_DISTANCE);
	volume_sampling_method_enum.insert("equiangular", VOLUME_SAMPLING_EQUIANGULAR);
	volume_sampling_method_enum.insert("multiple_importance", VOLUME_SAMPLING_MULTIPLE_IMPORTANCE);
	SOCKET_ENUM(volume_sampling_method, "Volume Sampling Method", volume_sampling_method_enum, VOLUME_SAMPLING_DISTANCE);

	static NodeEnum volume_interpolation_method_enum;
	volume_interpolation_method_enum.insert("linear", VOLUME_INTERPOLATION_LINEAR);
	volume_interpolation_method_enum.insert("cubic", VOLUME_INTERPOLATION_CUBIC);
	SOCKET_ENUM(volume_interpolation_method, "Volume Interpolation Method", volume_interpolation_method_enum, VOLUME_INTERPOLATION_LINEAR);

	static NodeEnum displacement_method_enum;
	displacement_method_enum.insert("bump", DISPLACE_BUMP);
	displacement_method_enum.insert("true", DISPLACE_TRUE);
	displacement_method_enum.insert("both", DISPLACE_BOTH);
	SOCKET_ENUM(displacement_method, "Displacement Method", displacement_method_enum, DISPLACE_BUMP);

	return type;
}

Shader::Shader()
: Node(node_type)
{
	pass_id = 0;

	graph = NULL;
	graph_bump = NULL;

	has_surface = false;
	has_surface_transparent = false;
	has_surface_emission = false;
	has_surface_bssrdf = false;
	has_volume = false;
	has_displacement = false;
	has_bssrdf_bump = false;
	has_surface_spatial_varying = false;
	has_volume_spatial_varying = false;
	has_object_dependency = false;
	has_integrator_dependency = false;
	has_volume_connected = false;

	displacement_method = DISPLACE_BUMP;

	id = -1;
	used = false;

	need_update = true;
	need_update_attributes = true;
}

Shader::~Shader()
{
	delete graph;
	delete graph_bump;
}

bool Shader::is_constant_emission(float3 *emission)
{
	ShaderInput *surf = graph->output()->input("Surface");

	if(!surf->link || surf->link->parent->type != EmissionNode::node_type) {
		return false;
	}

	EmissionNode *node = (EmissionNode*) surf->link->parent;

	assert(node->input("Color"));
	assert(node->input("Strength"));

	if(node->input("Color")->link || node->input("Strength")->link) {
		return false;
	}

	*emission = node->color*node->strength;

	return true;
}

void Shader::set_graph(ShaderGraph *graph_)
{
	/* do this here already so that we can detect if mesh or object attributes
	 * are needed, since the node attribute callbacks check if their sockets
	 * are connected but proxy nodes should not count */
	if(graph_)
		graph_->remove_proxy_nodes();

	/* assign graph */
	delete graph;
	delete graph_bump;
	graph = graph_;
	graph_bump = NULL;

	/* Store info here before graph optimization to make sure that
	 * nodes that get optimized away still count. */
	has_volume_connected = (graph->output()->input("Volume")->link != NULL);
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

	/* Special handle of background MIS light for now: for some reason it
	 * has use_mis set to false. We are quite close to release now, so
	 * better to be safe.
	 */
	if(this == scene->default_background &&
	   scene->light_manager->has_background_light(scene))
	{
		scene->light_manager->need_update = true;
	}

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

	if(has_displacement && displacement_method == DISPLACE_BOTH) {
		attributes.add(ATTR_STD_POSITION_UNDISPLACED);
	}
	
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
	beckmann_table_offset = TABLE_OFFSET_INVALID;
}

ShaderManager::~ShaderManager()
{
}

ShaderManager *ShaderManager::create(Scene *scene, int shadingsystem)
{
	ShaderManager *manager;

	(void)shadingsystem;  /* Ignored when built without OSL. */

#ifdef WITH_OSL
	if(shadingsystem == SHADINGSYSTEM_OSL) {
		manager = new OSLShaderManager();
	}
	else
#endif
	{
		manager = new SVMShaderManager();
	}
	
	add_default(scene);

	return manager;
}

uint ShaderManager::get_attribute_id(ustring name)
{
	thread_scoped_spin_lock lock(attribute_lock_);

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

int ShaderManager::get_shader_id(Shader *shader, bool smooth)
{
	/* get a shader id to pass to the kernel */
	int id = shader->id;

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
	uint id = 0;
	foreach(Shader *shader, scene->shaders) {
		shader->used = false;
		shader->id = id++;
	}

	scene->default_surface->used = true;
	scene->default_light->used = true;
	scene->default_background->used = true;
	scene->default_empty->used = true;

	if(scene->background->shader)
		scene->background->shader->used = true;

	foreach(Mesh *mesh, scene->meshes)
		foreach(Shader *shader, mesh->used_shaders)
			shader->used = true;

	foreach(Light *light, scene->lights)
		if(light->shader)
			light->shader->used = true;
}

void ShaderManager::device_update_common(Device *device,
                                         DeviceScene *dscene,
                                         Scene *scene,
                                         Progress& /*progress*/)
{
	device->tex_free(dscene->shader_flag);
	dscene->shader_flag.clear();

	if(scene->shaders.size() == 0)
		return;

	uint shader_flag_size = scene->shaders.size()*SHADER_SIZE;
	uint *shader_flag = dscene->shader_flag.resize(shader_flag_size);
	uint i = 0;
	bool has_volumes = false;
	bool has_transparent_shadow = false;

	foreach(Shader *shader, scene->shaders) {
		uint flag = 0;

		if(shader->use_mis)
			flag |= SD_USE_MIS;
		if(shader->has_surface_transparent && shader->use_transparent_shadow)
			flag |= SD_HAS_TRANSPARENT_SHADOW;
		if(shader->has_volume) {
			flag |= SD_HAS_VOLUME;
			has_volumes = true;

			/* todo: this could check more fine grained, to skip useless volumes
			 * enclosed inside an opaque bsdf.
			 */
			flag |= SD_HAS_TRANSPARENT_SHADOW;
		}
		/* in this case we can assume transparent surface */
		if(shader->has_volume_connected && !shader->has_surface)
			flag |= SD_HAS_ONLY_VOLUME;
		if(shader->heterogeneous_volume && shader->has_volume_spatial_varying)
			flag |= SD_HETEROGENEOUS_VOLUME;
		if(shader->has_bssrdf_bump)
			flag |= SD_HAS_BSSRDF_BUMP;
		if(shader->volume_sampling_method == VOLUME_SAMPLING_EQUIANGULAR)
			flag |= SD_VOLUME_EQUIANGULAR;
		if(shader->volume_sampling_method == VOLUME_SAMPLING_MULTIPLE_IMPORTANCE)
			flag |= SD_VOLUME_MIS;
		if(shader->volume_interpolation_method == VOLUME_INTERPOLATION_CUBIC)
			flag |= SD_VOLUME_CUBIC;
		if(shader->graph_bump)
			flag |= SD_HAS_BUMP;
		if(shader->displacement_method != DISPLACE_BUMP)
			flag |= SD_HAS_DISPLACEMENT;

		/* shader with bump mapping */
		if(shader->displacement_method != DISPLACE_TRUE && shader->graph_bump)
			flag |= SD_HAS_BSSRDF_BUMP;

		/* constant emission check */
		float3 constant_emission = make_float3(0.0f, 0.0f, 0.0f);
		if(shader->is_constant_emission(&constant_emission))
			flag |= SD_HAS_CONSTANT_EMISSION;

		/* regular shader */
		shader_flag[i++] = flag;
		shader_flag[i++] = shader->pass_id;
		shader_flag[i++] = __float_as_int(constant_emission.x);
		shader_flag[i++] = __float_as_int(constant_emission.y);
		shader_flag[i++] = __float_as_int(constant_emission.z);

		has_transparent_shadow |= (flag & SD_HAS_TRANSPARENT_SHADOW) != 0;
	}

	device->tex_alloc("__shader_flag", dscene->shader_flag);

	/* lookup tables */
	KernelTables *ktables = &dscene->data.tables;

	/* beckmann lookup table */
	if(beckmann_table_offset == TABLE_OFFSET_INVALID) {
		if(!beckmann_table_ready) {
			thread_scoped_lock lock(lookup_table_mutex);
			if(!beckmann_table_ready) {
				beckmann_table_build(beckmann_table);
				beckmann_table_ready = true;
			}
		}
		beckmann_table_offset = scene->lookup_tables->add_table(dscene, beckmann_table);
	}
	ktables->beckmann_offset = (int)beckmann_table_offset;

	/* integrator */
	KernelIntegrator *kintegrator = &dscene->data.integrator;
	kintegrator->use_volumes = has_volumes;
	/* TODO(sergey): De-duplicate with flags set in integrator.cpp. */
	if(scene->integrator->transparent_shadows) {
		kintegrator->transparent_shadows = has_transparent_shadow;
	}
}

void ShaderManager::device_free_common(Device *device, DeviceScene *dscene, Scene *scene)
{
	scene->lookup_tables->remove_table(&beckmann_table_offset);

	device->tex_free(dscene->shader_flag);
	dscene->shader_flag.clear();
}

void ShaderManager::add_default(Scene *scene)
{
	/* default surface */
	{
		ShaderGraph *graph = new ShaderGraph();

		DiffuseBsdfNode *diffuse = new DiffuseBsdfNode();
		diffuse->color = make_float3(0.8f, 0.8f, 0.8f);
		graph->add(diffuse);

		graph->connect(diffuse->output("BSDF"), graph->output()->input("Surface"));

		Shader *shader = new Shader();
		shader->name = "default_surface";
		shader->graph = graph;
		scene->shaders.push_back(shader);
		scene->default_surface = shader;
	}

	/* default light */
	{
		ShaderGraph *graph = new ShaderGraph();

		EmissionNode *emission = new EmissionNode();
		emission->color = make_float3(0.8f, 0.8f, 0.8f);
		emission->strength = 0.0f;
		graph->add(emission);

		graph->connect(emission->output("Emission"), graph->output()->input("Surface"));

		Shader *shader = new Shader();
		shader->name = "default_light";
		shader->graph = graph;
		scene->shaders.push_back(shader);
		scene->default_light = shader;
	}

	/* default background */
	{
		ShaderGraph *graph = new ShaderGraph();

		Shader *shader = new Shader();
		shader->name = "default_background";
		shader->graph = graph;
		scene->shaders.push_back(shader);
		scene->default_background = shader;
	}

	/* default empty */
	{
		ShaderGraph *graph = new ShaderGraph();

		Shader *shader = new Shader();
		shader->name = "default_empty";
		shader->graph = graph;
		scene->shaders.push_back(shader);
		scene->default_empty = shader;
	}
}

void ShaderManager::get_requested_graph_features(ShaderGraph *graph,
                                                 DeviceRequestedFeatures *requested_features)
{
	foreach(ShaderNode *node, graph->nodes) {
		requested_features->max_nodes_group = max(requested_features->max_nodes_group,
		                                          node->get_group());
		requested_features->nodes_features |= node->get_feature();
		if(node->special_type == SHADER_SPECIAL_TYPE_CLOSURE) {
			BsdfNode *bsdf_node = static_cast<BsdfNode*>(node);
			if(CLOSURE_IS_VOLUME(bsdf_node->closure)) {
				requested_features->nodes_features |= NODE_FEATURE_VOLUME;
			}
			else if(CLOSURE_IS_PRINCIPLED(bsdf_node->closure)) {
				requested_features->use_principled = true;
			}
		}
		if(node->has_surface_bssrdf()) {
			requested_features->use_subsurface = true;
		}
		if(node->has_surface_transparent()) {
			requested_features->use_transparent = true;
		}
	}
}

void ShaderManager::get_requested_features(Scene *scene,
                                           DeviceRequestedFeatures *requested_features)
{
	requested_features->max_nodes_group = NODE_GROUP_LEVEL_0;
	requested_features->nodes_features = 0;
	for(int i = 0; i < scene->shaders.size(); i++) {
		Shader *shader = scene->shaders[i];
		/* Gather requested features from all the nodes from the graph nodes. */
		get_requested_graph_features(shader->graph, requested_features);
		/* Gather requested features from the graph itself. */
		if(shader->graph_bump) {
			get_requested_graph_features(shader->graph_bump,
			                             requested_features);
		}
		ShaderNode *output_node = shader->graph->output();
		if(output_node->input("Displacement")->link != NULL) {
			requested_features->nodes_features |= NODE_FEATURE_BUMP;
			if(shader->displacement_method == DISPLACE_BOTH && requested_features->experimental) {
				requested_features->nodes_features |= NODE_FEATURE_BUMP_STATE;
			}
		}
		/* On top of volume nodes, also check if we need volume sampling because
		 * e.g. an Emission node would slip through the NODE_FEATURE_VOLUME check */
		if(shader->has_volume)
			requested_features->use_volume |= true;
	}
}

void ShaderManager::free_memory()
{
	beckmann_table.free_memory();
}

CCL_NAMESPACE_END


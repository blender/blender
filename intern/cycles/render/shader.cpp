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
#include "graph.h"
#include "light.h"
#include "mesh.h"
#include "nodes.h"
#include "osl.h"
#include "scene.h"
#include "shader.h"
#include "svm.h"

#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

/* Shader */

Shader::Shader()
{
	name = "";
	pass_id = 0;

	graph = NULL;
	graph_bump = NULL;

	sample_as_light = true;
	homogeneous_volume = false;

	has_surface = false;
	has_surface_transparent = false;
	has_surface_emission = false;
	has_volume = false;
	has_displacement = false;

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
	if(sample_as_light && has_surface_emission)
		scene->light_manager->need_update = true;

	/* get requested attributes. this could be optimized by pruning unused
	   nodes here already, but that's the job of the shader manager currently,
	   and may not be so great for interactive rendering where you temporarily
	   disconnect a node */
	AttributeRequestSet prev_attributes = attributes;

	attributes.clear();
	foreach(ShaderNode *node, graph->nodes)
		node->attributes(&attributes);
	
	/* compare if the attributes changed, mesh manager will check
	   need_update_attributes, update the relevant meshes and clear it. */
	if(attributes.modified(prev_attributes)) {
		need_update_attributes = true;
		scene->mesh_manager->need_update = true;
	}
}

/* Shader Manager */

ShaderManager::ShaderManager()
{
	need_update = true;
}

ShaderManager::~ShaderManager()
{
}

ShaderManager *ShaderManager::create(Scene *scene)
{
	ShaderManager *manager;

#ifdef WITH_OSL
	if(scene->params.shadingsystem == SceneParams::OSL)
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
	
	uint id = (uint)Attribute::STD_NUM + unique_attribute_id.size();
	unique_attribute_id[name] = id;
	return id;
}

uint ShaderManager::get_attribute_id(Attribute::Standard std)
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

void ShaderManager::device_update_common(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	device_free_common(device, dscene);

	if(scene->shaders.size() == 0)
		return;

	uint shader_flag_size = scene->shaders.size()*4;
	uint *shader_flag = dscene->shader_flag.resize(shader_flag_size);
	uint i = 0;

	foreach(Shader *shader, scene->shaders) {
		uint flag = 0;

		if(shader->sample_as_light)
			flag |= SD_SAMPLE_AS_LIGHT;
		if(shader->has_surface_transparent)
			flag |= SD_HAS_SURFACE_TRANSPARENT;
		if(shader->has_volume)
			flag |= SD_HAS_VOLUME;
		if(shader->homogeneous_volume)
			flag |= SD_HOMOGENEOUS_VOLUME;

		shader_flag[i++] = flag;
		shader_flag[i++] = shader->pass_id;
		shader_flag[i++] = flag;
		shader_flag[i++] = shader->pass_id;
	}

	device->tex_alloc("__shader_flag", dscene->shader_flag);
}

void ShaderManager::device_free_common(Device *device, DeviceScene *dscene)
{
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

		closure = graph->add(new BackgroundNode());
		closure->input("Color")->value = make_float3(0.8f, 0.8f, 0.8f);
		out = graph->output();

		graph->connect(closure->output("Background"), out->input("Surface"));

		shader = new Shader();
		shader->name = "default_background";
		shader->graph = graph;
		scene->shaders.push_back(shader);
		scene->default_background = scene->shaders.size() - 1;
	}
}

CCL_NAMESPACE_END


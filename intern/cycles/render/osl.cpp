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
#include "osl.h"
#include "scene.h"
#include "shader.h"

#ifdef WITH_OSL

#include "osl_globals.h"
#include "osl_services.h"
#include "osl_shader.h"

#include "util_foreach.h"
#include "util_path.h"
#include "util_progress.h"

#endif

CCL_NAMESPACE_BEGIN

#ifdef WITH_OSL

/* Shader Manager */

OSLShaderManager::OSLShaderManager()
{
	services = new OSLRenderServices();

	/* if we let OSL create it, it leaks */
	ts = TextureSystem::create(true);
	ts->attribute("automip",  1);
	ts->attribute("autotile", 64);

	ss = OSL::ShadingSystem::create(services, ts, &errhandler);
	ss->attribute("lockgeom", 1);
	ss->attribute("commonspace", "world");
	ss->attribute("optimize", 2);
	//ss->attribute("debug", 1);
	//ss->attribute("statistics:level", 1);
	ss->attribute("searchpath:shader", path_get("shader").c_str());

	OSLShader::register_closures(ss);
}

OSLShaderManager::~OSLShaderManager()
{
	OSL::ShadingSystem::destroy(ss);
	OSL::TextureSystem::destroy(ts);
	delete services;
}

void OSLShaderManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	/* test if we need to update */
	bool need_update = false;

	foreach(Shader *shader, scene->shaders)
		if(shader->need_update)
			need_update = true;
	
	if(!need_update)
		return;

	device_free(device, dscene);

	/* create shaders */
	OSLGlobals *og = (OSLGlobals*)device->osl_memory();

	foreach(Shader *shader, scene->shaders) {
		assert(shader->graph);

		if(progress.get_cancel()) return;

		if(shader->sample_as_light && shader->has_surface_emission)
			scene->light_manager->need_update = true;

		OSLCompiler compiler((void*)ss);
		compiler.background = (shader == scene->shaders[scene->default_background]);
		compiler.compile(og, shader);
	}

	/* setup shader engine */
	og->ss = ss;
	int background_id = scene->shader_manager->get_shader_id(scene->default_background);
	og->background_state = og->surface_state[background_id];
	og->use = true;

	tls_create(OSLGlobals::ThreadData, og->thread_data);

	foreach(Shader *shader, scene->shaders)
		shader->need_update = false;
	
	/* set texture system */
	scene->image_manager->set_osl_texture_system((void*)ts);

	device_update_common(device, dscene, scene, progress);
}

void OSLShaderManager::device_free(Device *device, DeviceScene *dscene)
{
	OSLGlobals *og = (OSLGlobals*)device->osl_memory();

	device_free_common(device, dscene);

	/* clear shader engine */
	og->use = false;
	og->ss = NULL;

	tls_delete(OSLGlobals::ThreadData, og->thread_data);

	og->surface_state.clear();
	og->volume_state.clear();
	og->displacement_state.clear();
	og->background_state.reset();
}

/* Graph Compiler */

OSLCompiler::OSLCompiler(void *shadingsys_)
{
	shadingsys = shadingsys_;
	current_type = SHADER_TYPE_SURFACE;
	current_shader = NULL;
	background = false;
}

string OSLCompiler::id(ShaderNode *node)
{
	/* assign layer unique name based on pointer address + bump mode */
	stringstream stream;
	stream << "node_" << node->name << "_" << node;

	return stream.str();
}

string OSLCompiler::compatible_name(const char *name)
{
	string sname = name;
	size_t i;

	while((i = sname.find(" ")) != string::npos)
		sname.replace(i, 1, "");
	
	return sname;
}

bool OSLCompiler::node_skip_input(ShaderNode *node, ShaderInput *input)
{
	/* exception for output node, only one input is actually used
	 * depending on the current shader type */

	if(node->name == ustring("output")) {
		if(strcmp(input->name, "Surface") == 0 && current_type != SHADER_TYPE_SURFACE)
			return true;
		if(strcmp(input->name, "Volume") == 0 && current_type != SHADER_TYPE_VOLUME)
			return true;
		if(strcmp(input->name, "Displacement") == 0 && current_type != SHADER_TYPE_DISPLACEMENT)
			return true;
	}
	else if(current_type == SHADER_TYPE_DISPLACEMENT && input->link && input->link->parent->name == ustring("bump"))
		return true;

	return false;
}

void OSLCompiler::add(ShaderNode *node, const char *name)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;

	/* pass in fixed parameter values */
	foreach(ShaderInput *input, node->inputs) {
		if(!input->link) {
			/* checks to untangle graphs */
			if(node_skip_input(node, input))
				continue;
			/* already has default value assigned */
			else if(input->default_value != ShaderInput::NONE)
				continue;

			switch(input->type) {
				case SHADER_SOCKET_COLOR:
					parameter_color(input->name, input->value);
					break;
				case SHADER_SOCKET_POINT:
					parameter_point(input->name, input->value);
					break;
				case SHADER_SOCKET_VECTOR:
					parameter_vector(input->name, input->value);
					break;
				case SHADER_SOCKET_NORMAL:
					parameter_normal(input->name, input->value);
					break;
				case SHADER_SOCKET_FLOAT:
					parameter(input->name, input->value.x);
					break;
				case SHADER_SOCKET_CLOSURE:
					break;
			}
		}
	}

	/* create shader of the appropriate type. we pass "surface" to all shaders,
	 * because "volume" and "displacement" don't work yet in OSL. the shaders
	 * work fine, but presumably these values would be used for more strict
	 * checking, so when that is fixed, we should update the code here too. */
	if(current_type == SHADER_TYPE_SURFACE)
		ss->Shader("surface", name, id(node).c_str());
	else if(current_type == SHADER_TYPE_VOLUME)
		ss->Shader("surface", name, id(node).c_str());
	else if(current_type == SHADER_TYPE_DISPLACEMENT)
		ss->Shader("surface", name, id(node).c_str());
	else
		assert(0);
	
	/* link inputs to other nodes */
	foreach(ShaderInput *input, node->inputs) {
		if(input->link) {
			if(node_skip_input(node, input))
				continue;

			/* connect shaders */
			string id_from = id(input->link->parent);
			string id_to = id(node);
			string param_from = compatible_name(input->link->name);
			string param_to = compatible_name(input->name);

			/* avoid name conflict with same input/output socket name */
			if(input->link->parent->input(input->link->name))
				param_from += "_";

			ss->ConnectShaders(id_from.c_str(), param_from.c_str(), id_to.c_str(), param_to.c_str());
		}
	}
}

void OSLCompiler::parameter(const char *name, float f)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	ss->Parameter(name, TypeDesc::TypeFloat, &f);
}

void OSLCompiler::parameter_color(const char *name, float3 f)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	ss->Parameter(name, TypeDesc::TypeColor, &f);
}

void OSLCompiler::parameter_point(const char *name, float3 f)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	ss->Parameter(name, TypeDesc::TypePoint, &f);
}

void OSLCompiler::parameter_normal(const char *name, float3 f)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	ss->Parameter(name, TypeDesc::TypeNormal, &f);
}

void OSLCompiler::parameter_vector(const char *name, float3 f)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	ss->Parameter(name, TypeDesc::TypeVector, &f);
}

void OSLCompiler::parameter(const char *name, int f)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	ss->Parameter(name, TypeDesc::TypeInt, &f);
}

void OSLCompiler::parameter(const char *name, const char *s)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	ss->Parameter(name, TypeDesc::TypeString, &s);
}

void OSLCompiler::parameter(const char *name, ustring s)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	const char *str = s.c_str();
	ss->Parameter(name, TypeDesc::TypeString, &str);
}

void OSLCompiler::parameter(const char *name, const Transform& tfm)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	ss->Parameter(name, TypeDesc::TypeMatrix, (float*)&tfm);
}

void OSLCompiler::find_dependencies(set<ShaderNode*>& dependencies, ShaderInput *input)
{
	ShaderNode *node = (input->link)? input->link->parent: NULL;

	if(node) {
		foreach(ShaderInput *in, node->inputs)
			if(!node_skip_input(node, in))
				find_dependencies(dependencies, in);

		dependencies.insert(node);
	}
}

void OSLCompiler::generate_nodes(const set<ShaderNode*>& nodes)
{
	set<ShaderNode*> done;
	bool nodes_done;

	do {
		nodes_done = true;

		foreach(ShaderNode *node, nodes) {
			if(done.find(node) == done.end()) {
				bool inputs_done = true;

				foreach(ShaderInput *input, node->inputs)
					if(!node_skip_input(node, input))
						if(input->link && done.find(input->link->parent) == done.end())
							inputs_done = false;

				if(inputs_done) {
					node->compile(*this);
					done.insert(node);

					if(node->name == ustring("emission"))
						current_shader->has_surface_emission = true;
					if(node->name == ustring("transparent"))
						current_shader->has_surface_transparent = true;
				}
				else
					nodes_done = false;
			}
		}
	} while(!nodes_done);
}

void OSLCompiler::compile_type(Shader *shader, ShaderGraph *graph, ShaderType type)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;

	current_type = type;

	ss->ShaderGroupBegin();

	ShaderNode *output = graph->output();
	set<ShaderNode*> dependencies;

	if(type == SHADER_TYPE_SURFACE) {
		/* generate surface shader */
		find_dependencies(dependencies, output->input("Surface"));
		generate_nodes(dependencies);
		output->compile(*this);
	}
	else if(type == SHADER_TYPE_VOLUME) {
		/* generate volume shader */
		find_dependencies(dependencies, output->input("Volume"));
		generate_nodes(dependencies);
		output->compile(*this);
	}
	else if(type == SHADER_TYPE_DISPLACEMENT) {
		/* generate displacement shader */
		find_dependencies(dependencies, output->input("Displacement"));
		generate_nodes(dependencies);
		output->compile(*this);
	}
	else
		assert(0);

	ss->ShaderGroupEnd();
}

void OSLCompiler::compile(OSLGlobals *og, Shader *shader)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	ShaderGraph *graph = shader->graph;
	ShaderNode *output = (graph)? graph->output(): NULL;

	/* copy graph for shader with bump mapping */
	if(output->input("Surface")->link && output->input("Displacement")->link)
		if(!shader->graph_bump)
			shader->graph_bump = shader->graph->copy();

	/* finalize */
	shader->graph->finalize(false, true);
	if(shader->graph_bump)
		shader->graph_bump->finalize(true, true);

	current_shader = shader;

	shader->has_surface = false;
	shader->has_surface_emission = false;
	shader->has_surface_transparent = false;
	shader->has_volume = false;
	shader->has_displacement = false;

	/* generate surface shader */
	if(graph && output->input("Surface")->link) {
		compile_type(shader, shader->graph, SHADER_TYPE_SURFACE);
		og->surface_state.push_back(ss->state());

		if(shader->graph_bump) {
			ss->clear_state();
			compile_type(shader, shader->graph_bump, SHADER_TYPE_SURFACE);
			og->surface_state.push_back(ss->state());
		}
		else
			og->surface_state.push_back(ss->state());

		ss->clear_state();

		shader->has_surface = true;
	}
	else {
		og->surface_state.push_back(OSL::ShadingAttribStateRef());
		og->surface_state.push_back(OSL::ShadingAttribStateRef());
	}

	/* generate volume shader */
	if(graph && output->input("Volume")->link) {
		compile_type(shader, shader->graph, SHADER_TYPE_VOLUME);
		shader->has_volume = true;

		og->volume_state.push_back(ss->state());
		og->volume_state.push_back(ss->state());
		ss->clear_state();
	}
	else {
		og->volume_state.push_back(OSL::ShadingAttribStateRef());
		og->volume_state.push_back(OSL::ShadingAttribStateRef());
	}

	/* generate displacement shader */
	if(graph && output->input("Displacement")->link) {
		compile_type(shader, shader->graph, SHADER_TYPE_DISPLACEMENT);
		shader->has_displacement = true;

		og->displacement_state.push_back(ss->state());
		og->displacement_state.push_back(ss->state());
		ss->clear_state();
	}
	else {
		og->displacement_state.push_back(OSL::ShadingAttribStateRef());
		og->displacement_state.push_back(OSL::ShadingAttribStateRef());
	}
}

#else

void OSLCompiler::add(ShaderNode *node, const char *name)
{
}

void OSLCompiler::parameter(const char *name, float f)
{
}

void OSLCompiler::parameter_color(const char *name, float3 f)
{
}

void OSLCompiler::parameter_vector(const char *name, float3 f)
{
}

void OSLCompiler::parameter(const char *name, int f)
{
}

void OSLCompiler::parameter(const char *name, const char *s)
{
}

void OSLCompiler::parameter(const char *name, ustring s)
{
}

void OSLCompiler::parameter(const char *name, const Transform& tfm)
{
}

#endif /* WITH_OSL */

CCL_NAMESPACE_END


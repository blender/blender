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
#include "util_md5.h"
#include "util_path.h"
#include "util_progress.h"

#endif

CCL_NAMESPACE_BEGIN

#ifdef WITH_OSL

/* Shader Manager */

OSLShaderManager::OSLShaderManager()
{
	thread_data_initialized = false;

	services = new OSLRenderServices();

	shading_system_init();
	texture_system_init();
}

OSLShaderManager::~OSLShaderManager()
{
	OSL::ShadingSystem::destroy(ss);
	OSL::TextureSystem::destroy(ts);
	delete services;
}

void OSLShaderManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;

	device_free(device, dscene);

	/* determine which shaders are in use */
	device_update_shaders_used(scene);

	/* create shaders */
	OSLGlobals *og = (OSLGlobals*)device->osl_memory();

	foreach(Shader *shader, scene->shaders) {
		assert(shader->graph);

		if(progress.get_cancel()) return;

		if(shader->sample_as_light && shader->has_surface_emission)
			scene->light_manager->need_update = true;

		OSLCompiler compiler((void*)this, (void*)ss, scene->image_manager);
		compiler.background = (shader == scene->shaders[scene->default_background]);
		compiler.compile(og, shader);
	}

	/* setup shader engine */
	og->ss = ss;
	og->services = services;
	int background_id = scene->shader_manager->get_shader_id(scene->default_background);
	og->background_state = og->surface_state[background_id & SHADER_MASK];
	og->use = true;

	foreach(Shader *shader, scene->shaders)
		shader->need_update = false;

	need_update = false;
	
	/* set texture system */
	scene->image_manager->set_osl_texture_system((void*)ts);

	device_update_common(device, dscene, scene, progress);

	if(!thread_data_initialized) {
		og->thread_data_init();
		thread_data_initialized = true;
	}
}

void OSLShaderManager::device_free(Device *device, DeviceScene *dscene)
{
	OSLGlobals *og = (OSLGlobals*)device->osl_memory();

	device_free_common(device, dscene);

	/* clear shader engine */
	og->use = false;
	og->ss = NULL;

	og->surface_state.clear();
	og->volume_state.clear();
	og->displacement_state.clear();
	og->background_state.reset();

	if(thread_data_initialized) {
		og->thread_data_free();
		thread_data_initialized = false;
	}
}

void OSLShaderManager::texture_system_init()
{
	/* if we let OSL create it, it leaks */
	ts = TextureSystem::create(true);
	ts->attribute("automip",  1);
	ts->attribute("autotile", 64);

	/* effectively unlimited for now, until we support proper mipmap lookups */
	ts->attribute("max_memory_MB", 16384);
}

void OSLShaderManager::shading_system_init()
{
	ss = OSL::ShadingSystem::create(services, ts, &errhandler);
	ss->attribute("lockgeom", 1);
	ss->attribute("commonspace", "world");
	ss->attribute("optimize", 2);
	//ss->attribute("debug", 1);
	//ss->attribute("statistics:level", 1);
	ss->attribute("searchpath:shader", path_get("shader"));

	/* our own ray types */
	static const char *raytypes[] = {
		"camera",		/* PATH_RAY_CAMERA */
		"reflection",	/* PATH_RAY_REFLECT */
		"refraction",	/* PATH_RAY_TRANSMIT */
		"diffuse",		/* PATH_RAY_DIFFUSE */
		"glossy",		/* PATH_RAY_GLOSSY */
		"singular",		/* PATH_RAY_SINGULAR */
		"transparent",	/* PATH_RAY_TRANSPARENT */
		"shadow",		/* PATH_RAY_SHADOW_OPAQUE */
		"shadow",		/* PATH_RAY_SHADOW_TRANSPARENT */
	};

	const int nraytypes = sizeof(raytypes)/sizeof(raytypes[0]);
	ss->attribute("raytypes", TypeDesc(TypeDesc::STRING, nraytypes), raytypes);

	OSLShader::register_closures(ss);

	loaded_shaders.clear();
}

bool OSLShaderManager::osl_compile(const string& inputfile, const string& outputfile)
{
	vector<string> options;
	string stdosl_path;

	/* specify output file name */
	options.push_back("-o");
	options.push_back(outputfile);

	/* specify standard include path */
	options.push_back("-I" + path_get("shader"));
	stdosl_path = path_get("shader/stdosl.h");

	/* compile */
	OSL::OSLCompiler *compiler = OSL::OSLCompiler::create();
	bool ok = compiler->compile(inputfile, options, stdosl_path);
	delete compiler;

	return ok;
}

bool OSLShaderManager::osl_query(OSL::OSLQuery& query, const string& filepath)
{
	string searchpath = path_user_get("shaders");
	return query.open(filepath, searchpath);
}

static string shader_filepath_hash(const string& filepath, uint64_t modified_time)
{
	/* compute a hash from filepath and modified time to detect changes */
	MD5Hash md5;
	md5.append((const uint8_t*)filepath.c_str(), filepath.size());
	md5.append((const uint8_t*)&modified_time, sizeof(modified_time));

	return md5.get_hex();
}

const char *OSLShaderManager::shader_test_loaded(const string& hash)
{
	set<string>::iterator it = loaded_shaders.find(hash);
	return (it == loaded_shaders.end())? NULL: it->c_str();
}

const char *OSLShaderManager::shader_load_filepath(string filepath)
{
	size_t len = filepath.size();
	string extension = filepath.substr(len - 4);
	uint64_t modified_time = path_modified_time(filepath);

	if(extension == ".osl") {
		/* .OSL File */
		string osopath = filepath.substr(0, len - 4) + ".oso";
		uint64_t oso_modified_time = path_modified_time(osopath);

		/* test if we have loaded the corresponding .OSO already */
		if(oso_modified_time != 0) {
			const char *hash = shader_test_loaded(shader_filepath_hash(osopath, oso_modified_time));

			if(hash)
				return hash;
		}

		/* autocompile .OSL to .OSO if needed */
		if(oso_modified_time == 0 || (oso_modified_time < modified_time)) {
			OSLShaderManager::osl_compile(filepath, osopath);
			modified_time = path_modified_time(osopath);
		}
		else
			modified_time = oso_modified_time;

		filepath = osopath;
	}
	else {
		if(extension == ".oso") {
			/* .OSO File, nothing to do */
		}
		else if(path_dirname(filepath) == "") {
			/* .OSO File in search path */
			filepath = path_join(path_user_get("shaders"), filepath + ".oso");
		}
		else {
			/* unknown file */
			return NULL;
		}

		/* test if we have loaded this .OSO already */
		const char *hash = shader_test_loaded(shader_filepath_hash(filepath, modified_time));

		if(hash)
			return hash;
	}

	/* read oso bytecode from file */
	string bytecode_hash = shader_filepath_hash(filepath, modified_time);
	string bytecode;

	if(!path_read_text(filepath, bytecode)) {
		fprintf(stderr, "Cycles shader graph: failed to read file %s\n", filepath.c_str());
		loaded_shaders.insert(bytecode_hash); /* to avoid repeat tries */
		return NULL;
	}

	return shader_load_bytecode(bytecode_hash, bytecode);
}

const char *OSLShaderManager::shader_load_bytecode(const string& hash, const string& bytecode)
{
	ss->LoadMemoryShader(hash.c_str(), bytecode.c_str());

	return loaded_shaders.insert(hash).first->c_str();
}

/* Graph Compiler */

OSLCompiler::OSLCompiler(void *manager_, void *shadingsys_, ImageManager *image_manager_)
{
	manager = manager_;
	shadingsys = shadingsys_;
	image_manager = image_manager_;
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

string OSLCompiler::compatible_name(ShaderNode *node, ShaderInput *input)
{
	string sname(input->name);
	size_t i;

	/* strip whitespace */
	while((i = sname.find(" ")) != string::npos)
		sname.replace(i, 1, "");
	
	/* if output exists with the same name, add "In" suffix */
	foreach(ShaderOutput *output, node->outputs) {
		if (strcmp(input->name, output->name)==0) {
			sname += "In";
			break;
		}
	}
	
	return sname;
}

string OSLCompiler::compatible_name(ShaderNode *node, ShaderOutput *output)
{
	string sname(output->name);
	size_t i;

	/* strip whitespace */
	while((i = sname.find(" ")) != string::npos)
		sname.replace(i, 1, "");
	
	/* if output exists with the same name, add "In" suffix */
	foreach(ShaderInput *input, node->inputs) {
		if (strcmp(input->name, output->name)==0) {
			sname += "Out";
			break;
		}
	}
	
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
		if(strcmp(input->name, "Normal") == 0)
			return true;
	}
	else if(node->name == ustring("bump")) {
		if(strcmp(input->name, "Height") == 0)
			return true;
	}
	else if(current_type == SHADER_TYPE_DISPLACEMENT && input->link && input->link->parent->name == ustring("bump"))
		return true;

	return false;
}

void OSLCompiler::add(ShaderNode *node, const char *name, bool isfilepath)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;

	/* load filepath */
	if(isfilepath) {
		name = ((OSLShaderManager*)manager)->shader_load_filepath(name);

		if(name == NULL)
			return;
	}

	/* pass in fixed parameter values */
	foreach(ShaderInput *input, node->inputs) {
		if(!input->link) {
			/* checks to untangle graphs */
			if(node_skip_input(node, input))
				continue;
			/* already has default value assigned */
			else if(input->default_value != ShaderInput::NONE)
				continue;

			string param_name = compatible_name(node, input);
			switch(input->type) {
				case SHADER_SOCKET_COLOR:
					parameter_color(param_name.c_str(), input->value);
					break;
				case SHADER_SOCKET_POINT:
					parameter_point(param_name.c_str(), input->value);
					break;
				case SHADER_SOCKET_VECTOR:
					parameter_vector(param_name.c_str(), input->value);
					break;
				case SHADER_SOCKET_NORMAL:
					parameter_normal(param_name.c_str(), input->value);
					break;
				case SHADER_SOCKET_FLOAT:
					parameter(param_name.c_str(), input->value.x);
					break;
				case SHADER_SOCKET_INT:
					parameter(param_name.c_str(), (int)input->value.x);
					break;
				case SHADER_SOCKET_STRING:
					parameter(param_name.c_str(), input->value_string);
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
			string param_from = compatible_name(input->link->parent, input->link);
			string param_to = compatible_name(node, input);

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

void OSLCompiler::parameter_array(const char *name, const float f[], int arraylen)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	TypeDesc type = TypeDesc::TypeFloat;
	type.arraylen = arraylen;
	ss->Parameter(name, type, f);
}

void OSLCompiler::parameter_color_array(const char *name, const float f[][3], int arraylen)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	TypeDesc type = TypeDesc::TypeColor;
	type.arraylen = arraylen;
	ss->Parameter(name, type, f);
}

void OSLCompiler::parameter_vector_array(const char *name, const float f[][3], int arraylen)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	TypeDesc type = TypeDesc::TypeVector;
	type.arraylen = arraylen;
	ss->Parameter(name, type, f);
}

void OSLCompiler::parameter_normal_array(const char *name, const float f[][3], int arraylen)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	TypeDesc type = TypeDesc::TypeNormal;
	type.arraylen = arraylen;
	ss->Parameter(name, type, f);
}

void OSLCompiler::parameter_point_array(const char *name, const float f[][3], int arraylen)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	TypeDesc type = TypeDesc::TypePoint;
	type.arraylen = arraylen;
	ss->Parameter(name, type, f);
}

void OSLCompiler::parameter_array(const char *name, const int f[], int arraylen)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	TypeDesc type = TypeDesc::TypeInt;
	type.arraylen = arraylen;
	ss->Parameter(name, type, f);
}

void OSLCompiler::parameter_array(const char *name, const char * const s[], int arraylen)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	TypeDesc type = TypeDesc::TypeString;
	type.arraylen = arraylen;
	ss->Parameter(name, type, s);
}

void OSLCompiler::parameter_array(const char *name, const Transform tfm[], int arraylen)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)shadingsys;
	TypeDesc type = TypeDesc::TypeMatrix;
	type.arraylen = arraylen;
	ss->Parameter(name, type, (const float *)tfm);
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
	if(shader->need_update) {
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
		if(shader->used && graph && output->input("Surface")->link) {
			compile_type(shader, shader->graph, SHADER_TYPE_SURFACE);
			shader->osl_surface_ref = ss->state();

			if(shader->graph_bump) {
				ss->clear_state();
				compile_type(shader, shader->graph_bump, SHADER_TYPE_SURFACE);
			}

			shader->osl_surface_bump_ref = ss->state();
			ss->clear_state();

			shader->has_surface = true;
		}
		else {
			shader->osl_surface_ref = OSL::ShadingAttribStateRef();
			shader->osl_surface_bump_ref = OSL::ShadingAttribStateRef();
		}

		/* generate volume shader */
		if(shader->used && graph && output->input("Volume")->link) {
			compile_type(shader, shader->graph, SHADER_TYPE_VOLUME);
			shader->has_volume = true;

			shader->osl_volume_ref = ss->state();
			ss->clear_state();
		}
		else
			shader->osl_volume_ref = OSL::ShadingAttribStateRef();

		/* generate displacement shader */
		if(shader->used && graph && output->input("Displacement")->link) {
			compile_type(shader, shader->graph, SHADER_TYPE_DISPLACEMENT);
			shader->has_displacement = true;
			shader->osl_displacement_ref = ss->state();
			ss->clear_state();
		}
		else
			shader->osl_displacement_ref = OSL::ShadingAttribStateRef();
	}

	/* push state to array for lookup */
	og->surface_state.push_back(shader->osl_surface_ref);
	og->surface_state.push_back(shader->osl_surface_bump_ref);

	og->volume_state.push_back(shader->osl_volume_ref);
	og->volume_state.push_back(shader->osl_volume_ref);

	og->displacement_state.push_back(shader->osl_displacement_ref);
	og->displacement_state.push_back(shader->osl_displacement_ref);
}

#else

void OSLCompiler::add(ShaderNode *node, const char *name, bool isfilepath)
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

void OSLCompiler::parameter_point(const char *name, float3 f)
{
}

void OSLCompiler::parameter_normal(const char *name, float3 f)
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

void OSLCompiler::parameter_array(const char *name, const float f[], int arraylen)
{
}

void OSLCompiler::parameter_color_array(const char *name, const float f[][3], int arraylen)
{
}

void OSLCompiler::parameter_vector_array(const char *name, const float f[][3], int arraylen)
{
}

void OSLCompiler::parameter_normal_array(const char *name, const float f[][3], int arraylen)
{
}

void OSLCompiler::parameter_point_array(const char *name, const float f[][3], int arraylen)
{
}

void OSLCompiler::parameter_array(const char *name, const int f[], int arraylen)
{
}

void OSLCompiler::parameter_array(const char *name, const char * const s[], int arraylen)
{
}

void OSLCompiler::parameter_array(const char *name, const Transform tfm[], int arraylen)
{
}

#endif /* WITH_OSL */

CCL_NAMESPACE_END


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

#ifndef __OSL_H__
#define __OSL_H__

#include "util_set.h"
#include "util_string.h"

#include "shader.h"

#ifdef WITH_OSL
#include <OSL/oslcomp.h>
#include <OSL/oslexec.h>
#include <OSL/oslquery.h>
#endif

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class ImageManager;
class OSLRenderServices;
class OSLGlobals;
class Scene;
class ShaderGraph;
class ShaderNode;
class ShaderInput;
class ShaderOutput;

#ifdef WITH_OSL

/* Shader Manage */

class OSLShaderManager : public ShaderManager {
public:
	OSLShaderManager();
	~OSLShaderManager();

	bool use_osl() { return true; }

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	/* osl compile and query */
	static bool osl_compile(const string& inputfile, const string& outputfile);
	static bool osl_query(OSL::OSLQuery& query, const string& filepath);

	/* shader file loading, all functions return pointer to hash string if found */
	const char *shader_test_loaded(const string& hash);
	const char *shader_load_bytecode(const string& hash, const string& bytecode);
	const char *shader_load_filepath(string filepath);

protected:
	void texture_system_init();
	void shading_system_init();

	OSL::ShadingSystem *ss;
	OSL::TextureSystem *ts;
	OSLRenderServices *services;
	OSL::ErrorHandler errhandler;
	set<string> loaded_shaders;
};

#endif

/* Graph Compiler */

class OSLCompiler {
public:
	OSLCompiler(void *manager, void *shadingsys, ImageManager *image_manager);
	void compile(OSLGlobals *og, Shader *shader);

	void add(ShaderNode *node, const char *name, bool isfilepath = false);

	void parameter(const char *name, float f);
	void parameter_color(const char *name, float3 f);
	void parameter_vector(const char *name, float3 f);
	void parameter_normal(const char *name, float3 f);
	void parameter_point(const char *name, float3 f);
	void parameter(const char *name, int f);
	void parameter(const char *name, const char *s);
	void parameter(const char *name, ustring str);
	void parameter(const char *name, const Transform& tfm);

	void parameter_array(const char *name, const float f[], int arraylen);
	void parameter_color_array(const char *name, const float f[][3], int arraylen);
	void parameter_vector_array(const char *name, const float f[][3], int arraylen);
	void parameter_normal_array(const char *name, const float f[][3], int arraylen);
	void parameter_point_array(const char *name, const float f[][3], int arraylen);
	void parameter_array(const char *name, const int f[], int arraylen);
	void parameter_array(const char *name, const char * const s[], int arraylen);
	void parameter_array(const char *name, const Transform tfm[], int arraylen);

	ShaderType output_type() { return current_type; }

	bool background;
	ImageManager *image_manager;

private:
	string id(ShaderNode *node);
	void compile_type(Shader *shader, ShaderGraph *graph, ShaderType type);
	bool node_skip_input(ShaderNode *node, ShaderInput *input);
	string compatible_name(ShaderNode *node, ShaderInput *input);
	string compatible_name(ShaderNode *node, ShaderOutput *output);

	void find_dependencies(set<ShaderNode*>& dependencies, ShaderInput *input);
	void generate_nodes(const set<ShaderNode*>& nodes);

	void *shadingsys;
	void *manager;
	ShaderType current_type;
	Shader *current_shader;
};

CCL_NAMESPACE_END

#endif /* __OSL_H__  */


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

#ifndef __OSL_H__
#define __OSL_H__

#include "util/util_array.h"
#include "util/util_set.h"
#include "util/util_string.h"
#include "util/util_thread.h"

#include "render/graph.h"
#include "render/nodes.h"
#include "render/shader.h"

#ifdef WITH_OSL
#  include <OSL/llvm_util.h>
#  include <OSL/oslcomp.h>
#  include <OSL/oslexec.h>
#  include <OSL/oslquery.h>
#endif

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class ImageManager;
class OSLRenderServices;
struct OSLGlobals;
class Scene;
class ShaderGraph;
class ShaderNode;
class ShaderInput;
class ShaderOutput;

#ifdef WITH_OSL

/* OSL Shader Info
 * to auto detect closures in the shader for MIS and transparent shadows */

struct OSLShaderInfo {
  OSLShaderInfo()
      : has_surface_emission(false), has_surface_transparent(false), has_surface_bssrdf(false)
  {
  }

  OSL::OSLQuery query;
  bool has_surface_emission;
  bool has_surface_transparent;
  bool has_surface_bssrdf;
};

/* Shader Manage */

class OSLShaderManager : public ShaderManager {
 public:
  OSLShaderManager();
  ~OSLShaderManager();

  static void free_memory();

  void reset(Scene *scene);

  bool use_osl()
  {
    return true;
  }

  void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_free(Device *device, DeviceScene *dscene, Scene *scene);

  /* osl compile and query */
  static bool osl_compile(const string &inputfile, const string &outputfile);
  static bool osl_query(OSL::OSLQuery &query, const string &filepath);

  /* shader file loading, all functions return pointer to hash string if found */
  const char *shader_test_loaded(const string &hash);
  const char *shader_load_bytecode(const string &hash, const string &bytecode);
  const char *shader_load_filepath(string filepath);
  OSLShaderInfo *shader_loaded_info(const string &hash);

  /* create OSL node using OSLQuery */
  OSLNode *osl_node(const std::string &filepath,
                    const std::string &bytecode_hash = "",
                    const std::string &bytecode = "");

 protected:
  void texture_system_init();
  void texture_system_free();

  void shading_system_init();
  void shading_system_free();

  OSL::ShadingSystem *ss;
  OSL::TextureSystem *ts;
  OSLRenderServices *services;
  OSL::ErrorHandler errhandler;
  map<string, OSLShaderInfo> loaded_shaders;

  static OSL::TextureSystem *ts_shared;
  static thread_mutex ts_shared_mutex;
  static int ts_shared_users;

  static OSL::ShadingSystem *ss_shared;
  static OSLRenderServices *services_shared;
  static thread_mutex ss_shared_mutex;
  static thread_mutex ss_mutex;
  static int ss_shared_users;
};

#endif

/* Graph Compiler */

class OSLCompiler {
 public:
  OSLCompiler(void *manager,
              void *shadingsys,
              OSLGlobals *osl_globals,
              ImageManager *image_manager,
              LightManager *light_manager);
  void compile(Scene *scene, Shader *shader);

  void add(ShaderNode *node, const char *name, bool isfilepath = false);

  void parameter(ShaderNode *node, const char *name);

  void parameter(const char *name, float f);
  void parameter_color(const char *name, float3 f);
  void parameter_vector(const char *name, float3 f);
  void parameter_normal(const char *name, float3 f);
  void parameter_point(const char *name, float3 f);
  void parameter(const char *name, int f);
  void parameter(const char *name, const char *s);
  void parameter(const char *name, ustring str);
  void parameter(const char *name, const Transform &tfm);

  void parameter_array(const char *name, const float f[], int arraylen);
  void parameter_color_array(const char *name, const array<float3> &f);

  void parameter_attribute(const char *name, ustring s);

  ShaderType output_type()
  {
    return current_type;
  }

  bool background;
  ImageManager *image_manager;
  LightManager *light_manager;

 private:
#ifdef WITH_OSL
  string id(ShaderNode *node);
  OSL::ShaderGroupRef compile_type(Shader *shader, ShaderGraph *graph, ShaderType type);
  bool node_skip_input(ShaderNode *node, ShaderInput *input);
  string compatible_name(ShaderNode *node, ShaderInput *input);
  string compatible_name(ShaderNode *node, ShaderOutput *output);

  void find_dependencies(ShaderNodeSet &dependencies, ShaderInput *input);
  void generate_nodes(const ShaderNodeSet &nodes);
#endif

  void *shadingsys;
  void *manager;
  OSLGlobals *osl_globals;
  ShaderType current_type;
  Shader *current_shader;
};

CCL_NAMESPACE_END

#endif /* __OSL_H__  */

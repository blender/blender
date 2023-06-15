/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/device.h"

#include "scene/background.h"
#include "scene/colorspace.h"
#include "scene/light.h"
#include "scene/osl.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/stats.h"

#ifdef WITH_OSL

#  include "kernel/osl/globals.h"
#  include "kernel/osl/services.h"

#  include "util/aligned_malloc.h"
#  include "util/foreach.h"
#  include "util/log.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/progress.h"
#  include "util/projection.h"

#endif

CCL_NAMESPACE_BEGIN

#ifdef WITH_OSL

/* Shared Texture and Shading System */

OSL::TextureSystem *OSLShaderManager::ts_shared = NULL;
int OSLShaderManager::ts_shared_users = 0;
thread_mutex OSLShaderManager::ts_shared_mutex;

OSL::ErrorHandler OSLShaderManager::errhandler;
map<int, OSL::ShadingSystem *> OSLShaderManager::ss_shared;
int OSLShaderManager::ss_shared_users = 0;
thread_mutex OSLShaderManager::ss_shared_mutex;
thread_mutex OSLShaderManager::ss_mutex;

int OSLCompiler::texture_shared_unique_id = 0;

/* Shader Manager */

OSLShaderManager::OSLShaderManager(Device *device) : device_(device)
{
  texture_system_init();
  shading_system_init();
}

OSLShaderManager::~OSLShaderManager()
{
  shading_system_free();
  texture_system_free();
}

void OSLShaderManager::free_memory()
{
#  ifdef OSL_HAS_BLENDER_CLEANUP_FIX
  /* There is a problem with LLVM+OSL: The order global destructors across
   * different compilation units run cannot be guaranteed, on windows this means
   * that the LLVM destructors run before the osl destructors, causing a crash
   * when the process exits. the OSL in svn has a special cleanup hack to
   * sidestep this behavior */
  OSL::pvt::LLVM_Util::Cleanup();
#  endif
}

void OSLShaderManager::reset(Scene * /*scene*/)
{
  shading_system_free();
  shading_system_init();
}

uint64_t OSLShaderManager::get_attribute_id(ustring name)
{
  return name.hash();
}

uint64_t OSLShaderManager::get_attribute_id(AttributeStandard std)
{
  /* if standard attribute, use geom: name convention */
  ustring stdname(string("geom:") + string(Attribute::standard_name(std)));
  return stdname.hash();
}

void OSLShaderManager::device_update_specific(Device *device,
                                              DeviceScene *dscene,
                                              Scene *scene,
                                              Progress &progress)
{
  if (!need_update())
    return;

  scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->osl.times.add_entry({"device_update", time});
    }
  });

  VLOG_INFO << "Total " << scene->shaders.size() << " shaders.";

  device_free(device, dscene, scene);

  /* set texture system (only on CPU devices, since GPU devices cannot use OIIO) */
  if (device->info.type == DEVICE_CPU) {
    scene->image_manager->set_osl_texture_system((void *)ts_shared);
  }

  /* create shaders */
  Shader *background_shader = scene->background->get_shader(scene);

  foreach (Shader *shader, scene->shaders) {
    assert(shader->graph);

    if (progress.get_cancel())
      return;

    /* we can only compile one shader at the time as the OSL ShadingSytem
     * has a single state, but we put the lock here so different renders can
     * compile shaders alternating */
    thread_scoped_lock lock(ss_mutex);

    device->foreach_device(
        [this, scene, shader, background = (shader == background_shader)](Device *sub_device) {
          OSLGlobals *og = (OSLGlobals *)sub_device->get_cpu_osl_memory();
          OSL::ShadingSystem *ss = ss_shared[sub_device->info.type];

          OSLCompiler compiler(this, ss, scene);
          compiler.background = background;
          compiler.compile(og, shader);
        });

    if (shader->emission_sampling != EMISSION_SAMPLING_NONE)
      scene->light_manager->tag_update(scene, LightManager::SHADER_COMPILED);
  }

  /* setup shader engine */
  int background_id = scene->shader_manager->get_shader_id(background_shader);

  device->foreach_device([background_id](Device *sub_device) {
    OSLGlobals *og = (OSLGlobals *)sub_device->get_cpu_osl_memory();
    OSL::ShadingSystem *ss = ss_shared[sub_device->info.type];

    og->ss = ss;
    og->ts = ts_shared;
    og->services = static_cast<OSLRenderServices *>(ss->renderer());

    og->background_state = og->surface_state[background_id & SHADER_MASK];
    og->use = true;
  });

  foreach (Shader *shader, scene->shaders)
    shader->clear_modified();

  update_flags = UPDATE_NONE;

  /* add special builtin texture types */
  for (const auto &[device_type, ss] : ss_shared) {
    OSLRenderServices *services = static_cast<OSLRenderServices *>(ss->renderer());

    services->textures.insert(ustring("@ao"), new OSLTextureHandle(OSLTextureHandle::AO));
    services->textures.insert(ustring("@bevel"), new OSLTextureHandle(OSLTextureHandle::BEVEL));
  }

  device_update_common(device, dscene, scene, progress);

  {
    /* Perform greedyjit optimization.
     *
     * This might waste time on optimizing groups which are never actually
     * used, but this prevents OSL from allocating data on TLS at render
     * time.
     *
     * This is much better for us because this way we aren't required to
     * stop task scheduler threads to make sure all TLS is clean and don't
     * have issues with TLS data free accessing freed memory if task scheduler
     * is being freed after the Session is freed.
     */
    thread_scoped_lock lock(ss_shared_mutex);

    /* Set current image manager during the lock, so that there is no conflict with other shader
     * manager instances.
     *
     * It is used in "OSLRenderServices::get_texture_handle" called during optimization below to
     * load images for the GPU. */
    OSLRenderServices::image_manager = scene->image_manager;

    for (const auto &[device_type, ss] : ss_shared) {
      ss->optimize_all_groups();
    }

    OSLRenderServices::image_manager = nullptr;
  }

  /* load kernels */
  if (!device->load_osl_kernels()) {
    progress.set_error(device->error_message());
  }
}

void OSLShaderManager::device_free(Device *device, DeviceScene *dscene, Scene *scene)
{
  device_free_common(device, dscene, scene);

  /* clear shader engine */
  device->foreach_device([](Device *sub_device) {
    OSLGlobals *og = (OSLGlobals *)sub_device->get_cpu_osl_memory();

    og->use = false;
    og->ss = NULL;
    og->ts = NULL;

    og->surface_state.clear();
    og->volume_state.clear();
    og->displacement_state.clear();
    og->bump_state.clear();
    og->background_state.reset();
  });

  /* Remove any textures specific to an image manager from shared render services textures, since
   * the image manager may get destroyed next. */
  for (const auto &[device_type, ss] : ss_shared) {
    OSLRenderServices *services = static_cast<OSLRenderServices *>(ss->renderer());

    for (auto it = services->textures.begin(); it != services->textures.end(); ++it) {
      if (it->second->handle.get_manager() == scene->image_manager) {
        /* Don't lock again, since the iterator already did so. */
        services->textures.erase(it->first, false);
        it.clear();
        /* Iterator was invalidated, start from the beginning again. */
        it = services->textures.begin();
      }
    }
  }
}

void OSLShaderManager::texture_system_init()
{
  /* create texture system, shared between different renders to reduce memory usage */
  thread_scoped_lock lock(ts_shared_mutex);

  if (ts_shared_users++ == 0) {
    ts_shared = TextureSystem::create(true);

    ts_shared->attribute("automip", 1);
    ts_shared->attribute("autotile", 64);
    ts_shared->attribute("gray_to_rgb", 1);

    /* effectively unlimited for now, until we support proper mipmap lookups */
    ts_shared->attribute("max_memory_MB", 16384);
  }
}

void OSLShaderManager::texture_system_free()
{
  /* shared texture system decrease users and destroy if no longer used */
  thread_scoped_lock lock(ts_shared_mutex);

  if (--ts_shared_users == 0) {
    ts_shared->invalidate_all(true);
    OSL::TextureSystem::destroy(ts_shared);
    ts_shared = NULL;
  }
}

void OSLShaderManager::shading_system_init()
{
  /* create shading system, shared between different renders to reduce memory usage */
  thread_scoped_lock lock(ss_shared_mutex);

  device_->foreach_device([](Device *sub_device) {
    const DeviceType device_type = sub_device->info.type;

    if (ss_shared_users++ == 0 || ss_shared.find(device_type) == ss_shared.end()) {
      /* Must use aligned new due to concurrent hash map. */
      OSLRenderServices *services = util_aligned_new<OSLRenderServices>(ts_shared, device_type);

      string shader_path = path_get("shader");
#  ifdef _WIN32
      /* Annoying thing, Cycles stores paths in UTF-8 codepage, so it can
       * operate with file paths with any character. This requires to use wide
       * char functions, but OSL uses old fashioned ANSI functions which means:
       *
       * - We have to convert our paths to ANSI before passing to OSL
       * - OSL can't be used when there's a multi-byte character in the path
       *   to the shaders folder.
       */
      shader_path = string_to_ansi(shader_path);
#  endif

      OSL::ShadingSystem *ss = new OSL::ShadingSystem(services, ts_shared, &errhandler);
      ss->attribute("lockgeom", 1);
      ss->attribute("commonspace", "world");
      ss->attribute("searchpath:shader", shader_path);
      ss->attribute("greedyjit", 1);

      VLOG_INFO << "Using shader search path: " << shader_path;

      /* our own ray types */
      static const char *raytypes[] = {
          "camera",          /* PATH_RAY_CAMERA */
          "reflection",      /* PATH_RAY_REFLECT */
          "refraction",      /* PATH_RAY_TRANSMIT */
          "diffuse",         /* PATH_RAY_DIFFUSE */
          "glossy",          /* PATH_RAY_GLOSSY */
          "singular",        /* PATH_RAY_SINGULAR */
          "transparent",     /* PATH_RAY_TRANSPARENT */
          "volume_scatter",  /* PATH_RAY_VOLUME_SCATTER */
          "importance_bake", /* PATH_RAY_IMPORTANCE_BAKE */

          "shadow", /* PATH_RAY_SHADOW_OPAQUE */
          "shadow", /* PATH_RAY_SHADOW_TRANSPARENT */

          "__unused__", /* PATH_RAY_NODE_UNALIGNED */
          "__unused__", /* PATH_RAY_MIS_SKIP */

          "diffuse_ancestor", /* PATH_RAY_DIFFUSE_ANCESTOR */

          /* Remaining irrelevant bits up to 32. */
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
          "__unused__",
      };

      const int nraytypes = sizeof(raytypes) / sizeof(raytypes[0]);
      ss->attribute("raytypes", TypeDesc(TypeDesc::STRING, nraytypes), raytypes);

      OSLRenderServices::register_closures(ss);

      ss_shared[device_type] = ss;
    }
  });

  loaded_shaders.clear();
}

void OSLShaderManager::shading_system_free()
{
  /* shared shading system decrease users and destroy if no longer used */
  thread_scoped_lock lock(ss_shared_mutex);

  device_->foreach_device([](Device * /*sub_device*/) {
    if (--ss_shared_users == 0) {
      for (const auto &[device_type, ss] : ss_shared) {
        OSLRenderServices *services = static_cast<OSLRenderServices *>(ss->renderer());

        delete ss;

        util_aligned_delete(services);
      }

      ss_shared.clear();
    }
  });
}

bool OSLShaderManager::osl_compile(const string &inputfile, const string &outputfile)
{
  vector<string> options;
  string stdosl_path;
  string shader_path = path_get("shader");

  /* Specify output file name. */
  options.push_back("-o");
  options.push_back(outputfile);

  /* Specify standard include path. */
  string include_path_arg = string("-I") + shader_path;
  options.push_back(include_path_arg);

  stdosl_path = path_join(shader_path, "stdcycles.h");

  /* Compile.
   *
   * Mutex protected because the OSL compiler does not appear to be thread safe, see #92503. */
  static thread_mutex osl_compiler_mutex;
  thread_scoped_lock lock(osl_compiler_mutex);

  OSL::OSLCompiler *compiler = new OSL::OSLCompiler(&OSL::ErrorHandler::default_handler());
  bool ok = compiler->compile(string_view(inputfile), options, string_view(stdosl_path));
  delete compiler;

  return ok;
}

bool OSLShaderManager::osl_query(OSL::OSLQuery &query, const string &filepath)
{
  string searchpath = path_user_get("shaders");
  return query.open(filepath, searchpath);
}

static string shader_filepath_hash(const string &filepath, uint64_t modified_time)
{
  /* compute a hash from filepath and modified time to detect changes */
  MD5Hash md5;
  md5.append((const uint8_t *)filepath.c_str(), filepath.size());
  md5.append((const uint8_t *)&modified_time, sizeof(modified_time));

  return md5.get_hex();
}

const char *OSLShaderManager::shader_test_loaded(const string &hash)
{
  map<string, OSLShaderInfo>::iterator it = loaded_shaders.find(hash);
  return (it == loaded_shaders.end()) ? NULL : it->first.c_str();
}

OSLShaderInfo *OSLShaderManager::shader_loaded_info(const string &hash)
{
  map<string, OSLShaderInfo>::iterator it = loaded_shaders.find(hash);
  return (it == loaded_shaders.end()) ? NULL : &it->second;
}

const char *OSLShaderManager::shader_load_filepath(string filepath)
{
  size_t len = filepath.size();
  string extension = filepath.substr(len - 4);
  uint64_t modified_time = path_modified_time(filepath);

  if (extension == ".osl") {
    /* .OSL File */
    string osopath = filepath.substr(0, len - 4) + ".oso";
    uint64_t oso_modified_time = path_modified_time(osopath);

    /* test if we have loaded the corresponding .OSO already */
    if (oso_modified_time != 0) {
      const char *hash = shader_test_loaded(shader_filepath_hash(osopath, oso_modified_time));

      if (hash)
        return hash;
    }

    /* Auto-compile .OSL to .OSO if needed. */
    if (oso_modified_time == 0 || (oso_modified_time < modified_time)) {
      OSLShaderManager::osl_compile(filepath, osopath);
      modified_time = path_modified_time(osopath);
    }
    else
      modified_time = oso_modified_time;

    filepath = osopath;
  }
  else {
    if (extension == ".oso") {
      /* .OSO File, nothing to do */
    }
    else if (path_dirname(filepath) == "") {
      /* .OSO File in search path */
      filepath = path_join(path_user_get("shaders"), filepath + ".oso");
    }
    else {
      /* unknown file */
      return NULL;
    }

    /* test if we have loaded this .OSO already */
    const char *hash = shader_test_loaded(shader_filepath_hash(filepath, modified_time));

    if (hash)
      return hash;
  }

  /* read oso bytecode from file */
  string bytecode_hash = shader_filepath_hash(filepath, modified_time);
  string bytecode;

  if (!path_read_text(filepath, bytecode)) {
    fprintf(stderr, "Cycles shader graph: failed to read file %s\n", filepath.c_str());
    OSLShaderInfo info;
    loaded_shaders[bytecode_hash] = info; /* to avoid repeat tries */
    return NULL;
  }

  return shader_load_bytecode(bytecode_hash, bytecode);
}

const char *OSLShaderManager::shader_load_bytecode(const string &hash, const string &bytecode)
{
  for (const auto &[device_type, ss] : ss_shared) {
    ss->LoadMemoryCompiledShader(hash.c_str(), bytecode.c_str());
  }

  OSLShaderInfo info;

  if (!info.query.open_bytecode(bytecode)) {
    fprintf(stderr, "OSL query error: %s\n", info.query.geterror().c_str());
  }

  /* this is a bit weak, but works */
  info.has_surface_emission = (bytecode.find("\"emission\"") != string::npos);
  info.has_surface_transparent = (bytecode.find("\"transparent\"") != string::npos);
  info.has_surface_bssrdf = (bytecode.find("\"bssrdf\"") != string::npos);

  loaded_shaders[hash] = info;

  return loaded_shaders.find(hash)->first.c_str();
}

/* This is a static function to avoid RTTI link errors with only this
 * file being compiled without RTTI to match OSL and LLVM libraries. */
OSLNode *OSLShaderManager::osl_node(ShaderGraph *graph,
                                    ShaderManager *manager,
                                    const std::string &filepath,
                                    const std::string &bytecode_hash,
                                    const std::string &bytecode)
{
  if (!manager->use_osl()) {
    return NULL;
  }

  /* create query */
  OSLShaderManager *osl_manager = static_cast<OSLShaderManager *>(manager);
  const char *hash;

  if (!filepath.empty()) {
    hash = osl_manager->shader_load_filepath(filepath);
  }
  else {
    hash = osl_manager->shader_test_loaded(bytecode_hash);
    if (!hash)
      hash = osl_manager->shader_load_bytecode(bytecode_hash, bytecode);
  }

  if (!hash) {
    return NULL;
  }

  OSLShaderInfo *info = osl_manager->shader_loaded_info(hash);

  /* count number of inputs */
  size_t num_inputs = 0;

  for (int i = 0; i < info->query.nparams(); i++) {
    const OSL::OSLQuery::Parameter *param = info->query.getparam(i);

    /* skip unsupported types */
    if (param->varlenarray || param->isstruct || param->type.arraylen > 1)
      continue;

    if (!param->isoutput)
      num_inputs++;
  }

  /* create node */
  OSLNode *node = OSLNode::create(graph, num_inputs);

  /* add new sockets from parameters */
  set<void *> used_sockets;

  for (int i = 0; i < info->query.nparams(); i++) {
    const OSL::OSLQuery::Parameter *param = info->query.getparam(i);

    /* skip unsupported types */
    if (param->varlenarray || param->isstruct || param->type.arraylen > 1)
      continue;

    SocketType::Type socket_type;

    /* Read type and default value. */
    if (param->isclosure) {
      socket_type = SocketType::CLOSURE;
    }
    else if (param->type.vecsemantics != TypeDesc::NOSEMANTICS) {
      if (param->type.vecsemantics == TypeDesc::COLOR)
        socket_type = SocketType::COLOR;
      else if (param->type.vecsemantics == TypeDesc::POINT)
        socket_type = SocketType::POINT;
      else if (param->type.vecsemantics == TypeDesc::VECTOR)
        socket_type = SocketType::VECTOR;
      else if (param->type.vecsemantics == TypeDesc::NORMAL)
        socket_type = SocketType::NORMAL;
      else
        continue;

      if (!param->isoutput && param->validdefault) {
        float3 *default_value = (float3 *)node->input_default_value();
        default_value->x = param->fdefault[0];
        default_value->y = param->fdefault[1];
        default_value->z = param->fdefault[2];
      }
    }
    else if (param->type.aggregate == TypeDesc::SCALAR) {
      if (param->type.basetype == TypeDesc::INT) {
        socket_type = SocketType::INT;

        if (!param->isoutput && param->validdefault) {
          *(int *)node->input_default_value() = param->idefault[0];
        }
      }
      else if (param->type.basetype == TypeDesc::FLOAT) {
        socket_type = SocketType::FLOAT;

        if (!param->isoutput && param->validdefault) {
          *(float *)node->input_default_value() = param->fdefault[0];
        }
      }
      else if (param->type.basetype == TypeDesc::STRING) {
        socket_type = SocketType::STRING;

        if (!param->isoutput && param->validdefault) {
          *(ustring *)node->input_default_value() = param->sdefault[0];
        }
      }
      else
        continue;
    }
    else
      continue;

    if (param->isoutput) {
      node->add_output(param->name, socket_type);
    }
    else {
      /* Detect if we should leave parameter initialization to OSL, either though
       * not constant default or widget metadata. */
      int socket_flags = 0;
      if (!param->validdefault) {
        socket_flags |= SocketType::LINK_OSL_INITIALIZER;
      }
      for (const OSL::OSLQuery::Parameter &metadata : param->metadata) {
        if (metadata.type == TypeDesc::STRING) {
          if (metadata.name == "widget" && metadata.sdefault[0] == "null") {
            socket_flags |= SocketType::LINK_OSL_INITIALIZER;
          }
        }
      }

      node->add_input(param->name, socket_type, socket_flags);
    }
  }

  /* Set byte-code hash or file-path. */
  if (!bytecode_hash.empty()) {
    node->bytecode_hash = bytecode_hash;
  }
  else {
    node->filepath = filepath;
  }

  /* Generate inputs and outputs */
  node->create_inputs_outputs(node->type);

  return node;
}

/* Static function, so only this file needs to be compile with RTTT. */
void OSLShaderManager::osl_image_slots(Device *device,
                                       ImageManager *image_manager,
                                       set<int> &image_slots)
{
  set<OSLRenderServices *> services_shared;
  device->foreach_device([&services_shared](Device *sub_device) {
    OSLGlobals *og = (OSLGlobals *)sub_device->get_cpu_osl_memory();
    services_shared.insert(og->services);
  });

  for (OSLRenderServices *services : services_shared) {
    for (auto it = services->textures.begin(); it != services->textures.end(); ++it) {
      if (it->second->handle.get_manager() == image_manager) {
        const int slot = it->second->handle.svm_slot();
        image_slots.insert(slot);
      }
    }
  }
}

/* Graph Compiler */

OSLCompiler::OSLCompiler(OSLShaderManager *manager, OSL::ShadingSystem *ss, Scene *scene)
    : scene(scene),
      manager(manager),
      services(static_cast<OSLRenderServices *>(ss->renderer())),
      ss(ss)
{
  current_type = SHADER_TYPE_SURFACE;
  current_shader = NULL;
  background = false;
}

string OSLCompiler::id(ShaderNode *node)
{
  /* assign layer unique name based on pointer address + bump mode */
  stringstream stream;

  /* Ensure that no grouping characters (e.g. commas with en_US locale)
   * are added to the pointer string. */
  stream.imbue(std::locale("C"));

  stream << "node_" << node->type->name << "_" << node;

  return stream.str();
}

string OSLCompiler::compatible_name(ShaderNode *node, ShaderInput *input)
{
  string sname(input->name().string());
  size_t i;

  /* Strip white-space. */
  while ((i = sname.find(" ")) != string::npos)
    sname.replace(i, 1, "");

  /* if output exists with the same name, add "In" suffix */
  foreach (ShaderOutput *output, node->outputs) {
    if (input->name() == output->name()) {
      sname += "In";
      break;
    }
  }

  return sname;
}

string OSLCompiler::compatible_name(ShaderNode *node, ShaderOutput *output)
{
  string sname(output->name().string());
  size_t i;

  /* Strip white-space. */
  while ((i = sname.find(" ")) != string::npos)
    sname.replace(i, 1, "");

  /* if input exists with the same name, add "Out" suffix */
  foreach (ShaderInput *input, node->inputs) {
    if (input->name() == output->name()) {
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

  if (input->flags() & SocketType::SVM_INTERNAL)
    return true;

  if (node->special_type == SHADER_SPECIAL_TYPE_OUTPUT) {
    if (input->name() == "Surface" && current_type != SHADER_TYPE_SURFACE)
      return true;
    if (input->name() == "Volume" && current_type != SHADER_TYPE_VOLUME)
      return true;
    if (input->name() == "Displacement" && current_type != SHADER_TYPE_DISPLACEMENT)
      return true;
    if (input->name() == "Normal" && current_type != SHADER_TYPE_BUMP)
      return true;
  }
  else if (node->special_type == SHADER_SPECIAL_TYPE_BUMP) {
    if (input->name() == "Height")
      return true;
  }
  else if (current_type == SHADER_TYPE_DISPLACEMENT && input->link &&
           input->link->parent->special_type == SHADER_SPECIAL_TYPE_BUMP)
    return true;

  return false;
}

void OSLCompiler::add(ShaderNode *node, const char *name, bool isfilepath)
{
  /* load filepath */
  if (isfilepath) {
    name = manager->shader_load_filepath(name);

    if (name == NULL)
      return;
  }

  /* pass in fixed parameter values */
  foreach (ShaderInput *input, node->inputs) {
    if (!input->link) {
      /* checks to untangle graphs */
      if (node_skip_input(node, input)) {
        continue;
      }
      if ((input->flags() & SocketType::LINK_OSL_INITIALIZER) && !(input->constant_folded_in)) {
        continue;
      }

      string param_name = compatible_name(node, input);
      const SocketType &socket = input->socket_type;
      switch (input->type()) {
        case SocketType::COLOR:
          parameter_color(param_name.c_str(), node->get_float3(socket));
          break;
        case SocketType::POINT:
          parameter_point(param_name.c_str(), node->get_float3(socket));
          break;
        case SocketType::VECTOR:
          parameter_vector(param_name.c_str(), node->get_float3(socket));
          break;
        case SocketType::NORMAL:
          parameter_normal(param_name.c_str(), node->get_float3(socket));
          break;
        case SocketType::FLOAT:
          parameter(param_name.c_str(), node->get_float(socket));
          break;
        case SocketType::INT:
          parameter(param_name.c_str(), node->get_int(socket));
          break;
        case SocketType::STRING:
          parameter(param_name.c_str(), node->get_string(socket));
          break;
        case SocketType::CLOSURE:
        case SocketType::UNDEFINED:
        default:
          break;
      }
    }
  }

  /* Create shader of the appropriate type. OSL only distinguishes between "surface"
   * and "displacement" at the moment. */
  if (current_type == SHADER_TYPE_SURFACE)
    ss->Shader("surface", name, id(node).c_str());
  else if (current_type == SHADER_TYPE_VOLUME)
    ss->Shader("surface", name, id(node).c_str());
  else if (current_type == SHADER_TYPE_DISPLACEMENT)
    ss->Shader("displacement", name, id(node).c_str());
  else if (current_type == SHADER_TYPE_BUMP)
    ss->Shader("displacement", name, id(node).c_str());
  else
    assert(0);

  /* link inputs to other nodes */
  foreach (ShaderInput *input, node->inputs) {
    if (input->link) {
      if (node_skip_input(node, input))
        continue;

      /* connect shaders */
      string id_from = id(input->link->parent);
      string id_to = id(node);
      string param_from = compatible_name(input->link->parent, input->link);
      string param_to = compatible_name(node, input);

      ss->ConnectShaders(id_from.c_str(), param_from.c_str(), id_to.c_str(), param_to.c_str());
    }
  }

  /* test if we shader contains specific closures */
  OSLShaderInfo *info = manager->shader_loaded_info(name);

  if (current_type == SHADER_TYPE_SURFACE) {
    if (info) {
      if (info->has_surface_emission && node->special_type == SHADER_SPECIAL_TYPE_OSL) {
        /* Will be used by Shader::estimate_emission. */
        OSLNode *oslnode = static_cast<OSLNode *>(node);
        oslnode->has_emission = true;
      }
      if (info->has_surface_transparent)
        current_shader->has_surface_transparent = true;
      if (info->has_surface_bssrdf) {
        current_shader->has_surface_bssrdf = true;
        current_shader->has_bssrdf_bump = true; /* can't detect yet */
      }
      current_shader->has_bump = true;             /* can't detect yet */
      current_shader->has_surface_raytrace = true; /* can't detect yet */
    }

    if (node->has_spatial_varying()) {
      current_shader->has_surface_spatial_varying = true;
    }
  }
  else if (current_type == SHADER_TYPE_VOLUME) {
    if (node->has_spatial_varying())
      current_shader->has_volume_spatial_varying = true;
    if (node->has_attribute_dependency())
      current_shader->has_volume_attribute_dependency = true;
  }

  if (node->has_integrator_dependency()) {
    current_shader->has_integrator_dependency = true;
  }
}

static TypeDesc array_typedesc(TypeDesc typedesc, int arraylength)
{
  return TypeDesc((TypeDesc::BASETYPE)typedesc.basetype,
                  (TypeDesc::AGGREGATE)typedesc.aggregate,
                  (TypeDesc::VECSEMANTICS)typedesc.vecsemantics,
                  arraylength);
}

void OSLCompiler::parameter(ShaderNode *node, const char *name)
{
  ustring uname = ustring(name);
  const SocketType &socket = *(node->type->find_input(uname));

  switch (socket.type) {
    case SocketType::BOOLEAN: {
      int value = node->get_bool(socket);
      ss->Parameter(name, TypeDesc::TypeInt, &value);
      break;
    }
    case SocketType::FLOAT: {
      float value = node->get_float(socket);
      ss->Parameter(uname, TypeDesc::TypeFloat, &value);
      break;
    }
    case SocketType::INT: {
      int value = node->get_int(socket);
      ss->Parameter(uname, TypeDesc::TypeInt, &value);
      break;
    }
    case SocketType::COLOR: {
      float3 value = node->get_float3(socket);
      ss->Parameter(uname, TypeDesc::TypeColor, &value);
      break;
    }
    case SocketType::VECTOR: {
      float3 value = node->get_float3(socket);
      ss->Parameter(uname, TypeDesc::TypeVector, &value);
      break;
    }
    case SocketType::POINT: {
      float3 value = node->get_float3(socket);
      ss->Parameter(uname, TypeDesc::TypePoint, &value);
      break;
    }
    case SocketType::NORMAL: {
      float3 value = node->get_float3(socket);
      ss->Parameter(uname, TypeDesc::TypeNormal, &value);
      break;
    }
    case SocketType::POINT2: {
      float2 value = node->get_float2(socket);
      ss->Parameter(uname, TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC2, TypeDesc::POINT), &value);
      break;
    }
    case SocketType::STRING: {
      ustring value = node->get_string(socket);
      ss->Parameter(uname, TypeDesc::TypeString, &value);
      break;
    }
    case SocketType::ENUM: {
      ustring value = node->get_string(socket);
      ss->Parameter(uname, TypeDesc::TypeString, &value);
      break;
    }
    case SocketType::TRANSFORM: {
      Transform value = node->get_transform(socket);
      ProjectionTransform projection(value);
      projection = projection_transpose(projection);
      ss->Parameter(uname, TypeDesc::TypeMatrix, &projection);
      break;
    }
    case SocketType::BOOLEAN_ARRAY: {
      // OSL does not support booleans, so convert to int
      const array<bool> &value = node->get_bool_array(socket);
      array<int> intvalue(value.size());
      for (size_t i = 0; i < value.size(); i++)
        intvalue[i] = value[i];
      ss->Parameter(uname, array_typedesc(TypeDesc::TypeInt, value.size()), intvalue.data());
      break;
    }
    case SocketType::FLOAT_ARRAY: {
      const array<float> &value = node->get_float_array(socket);
      ss->Parameter(uname, array_typedesc(TypeDesc::TypeFloat, value.size()), value.data());
      break;
    }
    case SocketType::INT_ARRAY: {
      const array<int> &value = node->get_int_array(socket);
      ss->Parameter(uname, array_typedesc(TypeDesc::TypeInt, value.size()), value.data());
      break;
    }
    case SocketType::COLOR_ARRAY:
    case SocketType::VECTOR_ARRAY:
    case SocketType::POINT_ARRAY:
    case SocketType::NORMAL_ARRAY: {
      TypeDesc typedesc;

      switch (socket.type) {
        case SocketType::COLOR_ARRAY:
          typedesc = TypeDesc::TypeColor;
          break;
        case SocketType::VECTOR_ARRAY:
          typedesc = TypeDesc::TypeVector;
          break;
        case SocketType::POINT_ARRAY:
          typedesc = TypeDesc::TypePoint;
          break;
        case SocketType::NORMAL_ARRAY:
          typedesc = TypeDesc::TypeNormal;
          break;
        default:
          assert(0);
          break;
      }

      // convert to tightly packed array since float3 has padding
      const array<float3> &value = node->get_float3_array(socket);
      array<float> fvalue(value.size() * 3);
      for (size_t i = 0, j = 0; i < value.size(); i++) {
        fvalue[j++] = value[i].x;
        fvalue[j++] = value[i].y;
        fvalue[j++] = value[i].z;
      }

      ss->Parameter(uname, array_typedesc(typedesc, value.size()), fvalue.data());
      break;
    }
    case SocketType::POINT2_ARRAY: {
      const array<float2> &value = node->get_float2_array(socket);
      ss->Parameter(
          uname,
          array_typedesc(TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC2, TypeDesc::POINT), value.size()),
          value.data());
      break;
    }
    case SocketType::STRING_ARRAY: {
      const array<ustring> &value = node->get_string_array(socket);
      ss->Parameter(uname, array_typedesc(TypeDesc::TypeString, value.size()), value.data());
      break;
    }
    case SocketType::TRANSFORM_ARRAY: {
      const array<Transform> &value = node->get_transform_array(socket);
      array<ProjectionTransform> fvalue(value.size());
      for (size_t i = 0; i < value.size(); i++) {
        fvalue[i] = projection_transpose(ProjectionTransform(value[i]));
      }
      ss->Parameter(uname, array_typedesc(TypeDesc::TypeMatrix, fvalue.size()), fvalue.data());
      break;
    }
    case SocketType::CLOSURE:
    case SocketType::NODE:
    case SocketType::NODE_ARRAY:
    case SocketType::UINT:
    case SocketType::UINT64:
    case SocketType::UNDEFINED:
    case SocketType::NUM_TYPES: {
      assert(0);
      break;
    }
  }
}

void OSLCompiler::parameter(const char *name, float f)
{
  ss->Parameter(name, TypeDesc::TypeFloat, &f);
}

void OSLCompiler::parameter_color(const char *name, float3 f)
{
  ss->Parameter(name, TypeDesc::TypeColor, &f);
}

void OSLCompiler::parameter_point(const char *name, float3 f)
{
  ss->Parameter(name, TypeDesc::TypePoint, &f);
}

void OSLCompiler::parameter_normal(const char *name, float3 f)
{
  ss->Parameter(name, TypeDesc::TypeNormal, &f);
}

void OSLCompiler::parameter_vector(const char *name, float3 f)
{
  ss->Parameter(name, TypeDesc::TypeVector, &f);
}

void OSLCompiler::parameter(const char *name, int f)
{
  ss->Parameter(name, TypeDesc::TypeInt, &f);
}

void OSLCompiler::parameter(const char *name, const char *s)
{
  ss->Parameter(name, TypeDesc::TypeString, &s);
}

void OSLCompiler::parameter(const char *name, ustring s)
{
  const char *str = s.c_str();
  ss->Parameter(name, TypeDesc::TypeString, &str);
}

void OSLCompiler::parameter(const char *name, const Transform &tfm)
{
  ProjectionTransform projection(tfm);
  projection = projection_transpose(projection);
  ss->Parameter(name, TypeDesc::TypeMatrix, (float *)&projection);
}

void OSLCompiler::parameter_array(const char *name, const float f[], int arraylen)
{
  TypeDesc type = TypeDesc::TypeFloat;
  type.arraylen = arraylen;
  ss->Parameter(name, type, f);
}

void OSLCompiler::parameter_color_array(const char *name, const array<float3> &f)
{
  /* NOTE: cycles float3 type is actually 4 floats! need to use an explicit array. */
  array<float[3]> table(f.size());

  for (int i = 0; i < f.size(); ++i) {
    table[i][0] = f[i].x;
    table[i][1] = f[i].y;
    table[i][2] = f[i].z;
  }

  TypeDesc type = TypeDesc::TypeColor;
  type.arraylen = table.size();
  ss->Parameter(name, type, table.data());
}

void OSLCompiler::parameter_attribute(const char *name, ustring s)
{
  if (Attribute::name_standard(s.c_str()))
    parameter(name, (string("geom:") + s.c_str()).c_str());
  else
    parameter(name, s.c_str());
}

void OSLCompiler::find_dependencies(ShaderNodeSet &dependencies, ShaderInput *input)
{
  ShaderNode *node = (input->link) ? input->link->parent : NULL;

  if (node != NULL && dependencies.find(node) == dependencies.end()) {
    foreach (ShaderInput *in, node->inputs)
      if (!node_skip_input(node, in))
        find_dependencies(dependencies, in);

    dependencies.insert(node);
  }
}

void OSLCompiler::generate_nodes(const ShaderNodeSet &nodes)
{
  ShaderNodeSet done;
  bool nodes_done;

  do {
    nodes_done = true;

    foreach (ShaderNode *node, nodes) {
      if (done.find(node) == done.end()) {
        bool inputs_done = true;

        foreach (ShaderInput *input, node->inputs)
          if (!node_skip_input(node, input))
            if (input->link && done.find(input->link->parent) == done.end())
              inputs_done = false;

        if (inputs_done) {
          node->compile(*this);
          done.insert(node);

          if (current_type == SHADER_TYPE_SURFACE) {
            if (node->has_surface_transparent())
              current_shader->has_surface_transparent = true;
            if (node->get_feature() & KERNEL_FEATURE_NODE_RAYTRACE)
              current_shader->has_surface_raytrace = true;
            if (node->has_spatial_varying())
              current_shader->has_surface_spatial_varying = true;
            if (node->has_surface_bssrdf()) {
              current_shader->has_surface_bssrdf = true;
              if (node->has_bssrdf_bump())
                current_shader->has_bssrdf_bump = true;
            }
            if (node->has_bump()) {
              current_shader->has_bump = true;
            }
          }
          else if (current_type == SHADER_TYPE_VOLUME) {
            if (node->has_spatial_varying())
              current_shader->has_volume_spatial_varying = true;
          }
        }
        else
          nodes_done = false;
      }
    }
  } while (!nodes_done);
}

OSL::ShaderGroupRef OSLCompiler::compile_type(Shader *shader, ShaderGraph *graph, ShaderType type)
{
  current_type = type;

  /* Use name hash to identify shader group to avoid issues with non-alphanumeric characters */
  stringstream name;
  name.imbue(std::locale("C"));
  name << "shader_" << shader->name.hash();

  OSL::ShaderGroupRef group = ss->ShaderGroupBegin(name.str());

  ShaderNode *output = graph->output();
  ShaderNodeSet dependencies;

  if (type == SHADER_TYPE_SURFACE) {
    /* generate surface shader */
    find_dependencies(dependencies, output->input("Surface"));
    generate_nodes(dependencies);
    output->compile(*this);
  }
  else if (type == SHADER_TYPE_BUMP) {
    /* generate bump shader */
    find_dependencies(dependencies, output->input("Normal"));
    generate_nodes(dependencies);
    output->compile(*this);
  }
  else if (type == SHADER_TYPE_VOLUME) {
    /* generate volume shader */
    find_dependencies(dependencies, output->input("Volume"));
    generate_nodes(dependencies);
    output->compile(*this);
  }
  else if (type == SHADER_TYPE_DISPLACEMENT) {
    /* generate displacement shader */
    find_dependencies(dependencies, output->input("Displacement"));
    generate_nodes(dependencies);
    output->compile(*this);
  }
  else
    assert(0);

  ss->ShaderGroupEnd();

  return group;
}

void OSLCompiler::compile(OSLGlobals *og, Shader *shader)
{
  if (shader->is_modified()) {
    ShaderGraph *graph = shader->graph;
    ShaderNode *output = (graph) ? graph->output() : NULL;

    bool has_bump = (shader->get_displacement_method() != DISPLACE_TRUE) &&
                    output->input("Surface")->link && output->input("Displacement")->link;

    /* finalize */
    shader->graph->finalize(scene,
                            has_bump,
                            shader->has_integrator_dependency,
                            shader->get_displacement_method() == DISPLACE_BOTH);

    current_shader = shader;

    shader->has_surface = false;
    shader->has_surface_transparent = false;
    shader->has_surface_raytrace = false;
    shader->has_surface_bssrdf = false;
    shader->has_bump = has_bump;
    shader->has_bssrdf_bump = has_bump;
    shader->has_volume = false;
    shader->has_displacement = false;
    shader->has_surface_spatial_varying = false;
    shader->has_volume_spatial_varying = false;
    shader->has_volume_attribute_dependency = false;
    shader->has_integrator_dependency = false;

    /* generate surface shader */
    if (shader->reference_count() && graph && output->input("Surface")->link) {
      shader->osl_surface_ref = compile_type(shader, shader->graph, SHADER_TYPE_SURFACE);

      if (has_bump)
        shader->osl_surface_bump_ref = compile_type(shader, shader->graph, SHADER_TYPE_BUMP);
      else
        shader->osl_surface_bump_ref = OSL::ShaderGroupRef();

      shader->has_surface = true;
    }
    else {
      shader->osl_surface_ref = OSL::ShaderGroupRef();
      shader->osl_surface_bump_ref = OSL::ShaderGroupRef();
    }

    /* generate volume shader */
    if (shader->reference_count() && graph && output->input("Volume")->link) {
      shader->osl_volume_ref = compile_type(shader, shader->graph, SHADER_TYPE_VOLUME);
      shader->has_volume = true;
    }
    else
      shader->osl_volume_ref = OSL::ShaderGroupRef();

    /* generate displacement shader */
    if (shader->reference_count() && graph && output->input("Displacement")->link) {
      shader->osl_displacement_ref = compile_type(shader, shader->graph, SHADER_TYPE_DISPLACEMENT);
      shader->has_displacement = true;
    }
    else
      shader->osl_displacement_ref = OSL::ShaderGroupRef();

    /* Estimate emission for MIS. */
    shader->estimate_emission();
  }

  /* push state to array for lookup */
  og->surface_state.push_back(shader->osl_surface_ref);
  og->volume_state.push_back(shader->osl_volume_ref);
  og->displacement_state.push_back(shader->osl_displacement_ref);
  og->bump_state.push_back(shader->osl_surface_bump_ref);
}

void OSLCompiler::parameter_texture(const char *name, ustring filename, ustring colorspace)
{
  /* Textured loaded through the OpenImageIO texture cache. For this
   * case we need to do runtime color space conversion. */
  OSLTextureHandle *handle = new OSLTextureHandle(OSLTextureHandle::OIIO);
  handle->processor = ColorSpaceManager::get_processor(colorspace);
  services->textures.insert(filename, handle);
  parameter(name, filename);
}

void OSLCompiler::parameter_texture(const char *name, const ImageHandle &handle)
{
  /* Texture loaded through SVM image texture system. We generate a unique
   * name, which ends up being used in OSLRenderServices::get_texture_handle
   * to get handle again. Note that this name must be unique between multiple
   * render sessions as the render services are shared. */
  ustring filename(string_printf("@svm%d", texture_shared_unique_id++).c_str());
  services->textures.insert(filename,
                            new OSLTextureHandle(OSLTextureHandle::SVM, handle.get_svm_slots()));
  parameter(name, filename);
}

void OSLCompiler::parameter_texture_ies(const char *name, int svm_slot)
{
  /* IES light textures stored in SVM. */
  ustring filename(string_printf("@svm%d", texture_shared_unique_id++).c_str());
  services->textures.insert(filename, new OSLTextureHandle(OSLTextureHandle::IES, svm_slot));
  parameter(name, filename);
}

#else

void OSLCompiler::add(ShaderNode * /*node*/, const char * /*name*/, bool /*isfilepath*/) {}

void OSLCompiler::parameter(ShaderNode * /*node*/, const char * /*name*/) {}

void OSLCompiler::parameter(const char * /*name*/, float /*f*/) {}

void OSLCompiler::parameter_color(const char * /*name*/, float3 /*f*/) {}

void OSLCompiler::parameter_vector(const char * /*name*/, float3 /*f*/) {}

void OSLCompiler::parameter_point(const char * /*name*/, float3 /*f*/) {}

void OSLCompiler::parameter_normal(const char * /*name*/, float3 /*f*/) {}

void OSLCompiler::parameter(const char * /*name*/, int /*f*/) {}

void OSLCompiler::parameter(const char * /*name*/, const char * /*s*/) {}

void OSLCompiler::parameter(const char * /*name*/, ustring /*s*/) {}

void OSLCompiler::parameter(const char * /*name*/, const Transform & /*tfm*/) {}

void OSLCompiler::parameter_array(const char * /*name*/, const float /*f*/[], int /*arraylen*/) {}

void OSLCompiler::parameter_color_array(const char * /*name*/, const array<float3> & /*f*/) {}

void OSLCompiler::parameter_texture(const char * /* name */,
                                    ustring /* filename */,
                                    ustring /* colorspace */)
{
}

void OSLCompiler::parameter_texture(const char * /* name */, const ImageHandle & /*handle*/) {}

void OSLCompiler::parameter_texture_ies(const char * /* name */, int /* svm_slot */) {}

#endif /* WITH_OSL */

CCL_NAMESPACE_END

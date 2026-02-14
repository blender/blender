/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/device.h"

#include "scene/background.h"
#include "scene/camera.h"
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
#  include "util/log.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/progress.h"
#  include "util/projection.h"
#  include "util/task.h"

#endif

CCL_NAMESPACE_BEGIN

#ifdef WITH_OSL

/* Shared Texture and Shading System */

std::shared_ptr<OSL::TextureSystem> ts_shared;
thread_mutex ts_shared_mutex;

map<DeviceType, std::shared_ptr<OSL::ShadingSystem>> ss_shared;
thread_mutex ss_shared_mutex;
OSL::ErrorHandler errhandler;

std::atomic<int> OSLCompiler::texture_shared_unique_id = 0;

/* Shader Manager */

OSLManager::OSLManager(Device *device) : device_(device), need_update_(true) {}

OSLManager::~OSLManager()
{
  shading_system_free();
  texture_system_free();
}

void OSLManager::free_memory()
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

void OSLManager::reset(Scene * /*scene*/)
{
  shading_system_free();
  tag_update();
}

OSL::TextureSystem *OSLManager::get_texture_system()
{
  if (!ts) {
    texture_system_init();
  }
  return ts.get();
}

OSL::ShadingSystem *OSLManager::get_shading_system(Device *sub_device)
{
  return ss_map[sub_device->info.type].get();
}

void OSLManager::foreach_shading_system(const std::function<void(OSL::ShadingSystem *)> &callback)
{
  for (const auto &[device_type, ss] : ss_map) {
    callback(ss.get());
  }
}

void OSLManager::foreach_render_services(const std::function<void(OSLRenderServices *)> &callback)
{
  for (const auto &[device_type, ss] : ss_map) {
    callback(static_cast<OSLRenderServices *>(ss->renderer()));
  }
}

void OSLManager::foreach_osl_device(Device *device,
                                    const std::function<void(Device *, OSLGlobals *)> &callback)
{
  device->foreach_device([callback](Device *sub_device) {
    OSLGlobals *og = sub_device->get_cpu_osl_memory();
    if (og != nullptr) {
      callback(sub_device, og);
    }
  });
}

void OSLManager::tag_update()
{
  need_update_ = true;
}

bool OSLManager::need_update() const
{
  return need_update_;
}

void OSLManager::device_update_pre(Device *device, Scene *scene)
{
  if (scene->shader_manager->use_osl() || !scene->camera->script_name.empty()) {
    shading_system_init(scene->shader_manager->get_scene_linear_interop_id());
  }

  if (!need_update()) {
    return;
  }

  /* set texture system (only on CPU devices, since GPU devices cannot use OIIO) */
  if (scene->shader_manager->use_osl()) {
    /* add special builtin texture types */
    foreach_render_services([](OSLRenderServices *services) {
      services->textures.insert(OSLUStringHash("@ao"), OSLTextureHandle(OSLTextureHandle::AO));
      services->textures.insert(OSLUStringHash("@bevel"),
                                OSLTextureHandle(OSLTextureHandle::BEVEL));
    });

    if (device->info.type == DEVICE_CPU) {
      scene->image_manager->set_osl_texture_system((void *)get_texture_system());
    }
  }
}

void OSLManager::device_update_post(Device *device,
                                    Scene *scene,
                                    Progress &progress,
                                    const bool reload_kernels)
{
  /* Create the camera shader. */
  if (need_update() && !scene->camera->script_name.empty()) {
    if (progress.get_cancel()) {
      return;
    }
    foreach_osl_device(device, [this, scene](Device *sub_device, OSLGlobals *og) {
      OSL::ShadingSystem *ss = get_shading_system(sub_device);

      OSL::ShaderGroupRef group = ss->ShaderGroupBegin("camera_group");
      for (const auto &param : scene->camera->script_params) {
        const ustring &name = param.first;
        const vector<uint8_t> &data = param.second.first;
        const TypeDesc &type = param.second.second;
        if (type.basetype == TypeDesc::STRING) {
          const void *string = data.data();
          ss->Parameter(*group, name, type, (const void *)&string);
        }
        else {
          ss->Parameter(*group, name, type, (const void *)data.data());
        }
      }
      ss->Shader(*group, "shader", scene->camera->script_name, "camera");
      ss->ShaderGroupEnd(*group);

      og->ss = ss;
      og->ts = get_texture_system();
      og->services = static_cast<OSLRenderServices *>(ss->renderer());

      og->camera_state = group;
      og->use_camera = true;

      /* Memory layout is {P, dPdx, dPdy, D, dDdx, dDdy, T}.
       * If we request derivs from OSL, it will automatically output them after the main parameter.
       * However, some scripts might have more efficient ways to compute them explicitly, so if a
       * script has any of the derivative outputs we use those instead. */

      OSLShaderInfo *info = shader_loaded_info(scene->camera->script_name);
      const string deriv_args[] = {"dPdx", "dPdy", "dDdx", "dDdy"};
      bool explicit_derivs = false;
      for (const auto &arg : deriv_args) {
        if (info->query.getparam(arg) != nullptr) {
          explicit_derivs = true;
        }
      }

      auto add_param = [&](const char *name, OIIO::TypeDesc type, bool derivs, int offset) {
        ss->add_symlocs(group.get(),
                        OSL::SymLocationDesc(string_printf("camera.%s", name),
                                             type,
                                             derivs,
                                             OSL::SymArena::Outputs,
                                             offset * sizeof(float)));
      };

      if (explicit_derivs) {
        add_param("dPdx", OIIO::TypeVector, false, 3);
        add_param("dPdy", OIIO::TypeVector, false, 6);
        add_param("dDdx", OIIO::TypeVector, false, 12);
        add_param("dDdy", OIIO::TypeVector, false, 15);
      }
      add_param("position", OIIO::TypePoint, !explicit_derivs, 0);
      add_param("direction", OIIO::TypeVector, !explicit_derivs, 9);
      add_param("throughput", OIIO::TypeColor, false, 18);
    });
  }
  else if (need_update()) {
    foreach_osl_device(device, [](Device *, OSLGlobals *og) {
      og->camera_state.reset();
      og->use_camera = false;
    });
  }

  if (need_update()) {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->osl.times.add_entry({"jit", time});
      }
    });

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
    const thread_scoped_lock lock(ss_shared_mutex);

    /* Set current image manager during the lock, so that there is no conflict with other shader
     * manager instances.
     *
     * It is used in "OSLRenderServices::get_texture_handle" called during optimization below to
     * load images for the GPU. */
    OSLRenderServices::image_manager = scene->image_manager.get();

    foreach_shading_system([](OSL::ShadingSystem *ss) { ss->optimize_all_groups(); });

    OSLRenderServices::image_manager = nullptr;
  }

  /* Load OSL kernels on changes to shaders, or when main kernels got reloaded. */
  if (need_update() || reload_kernels) {
    foreach_osl_device(device, [this, &progress](Device *sub_device, OSLGlobals *og) {
      if (og->use_shading || og->use_camera) {
        OSL::ShadingSystem *ss = get_shading_system(sub_device);

        og->ss = ss;
        og->ts = get_texture_system();
        og->services = static_cast<OSLRenderServices *>(ss->renderer());

        /* load kernels */
        if (!sub_device->load_osl_kernels()) {
          progress.set_error(sub_device->error_message());
        }
      }
    });
  }

  need_update_ = false;
}

void OSLManager::device_free(Device *device, DeviceScene * /*dscene*/, Scene *scene)
{
  /* clear shader engine */
  foreach_osl_device(device, [](Device *, OSLGlobals *og) {
    og->use_shading = false;
    og->use_camera = false;
    og->ss = nullptr;
    og->ts = nullptr;
    og->camera_state.reset();
  });

  /* Remove any textures specific to an image manager from shared render services textures, since
   * the image manager may get destroyed next. */
  foreach_render_services([scene](OSLRenderServices *services) {
    for (auto it = services->textures.begin(); it != services->textures.end();) {
      if (it->second.handle.get_manager() == scene->image_manager.get()) {
        /* Don't lock again, since the iterator already did so. */
        services->textures.erase(it->first, false);
        it.clear();
        /* Iterator was invalidated, start from the beginning again. */
        it = services->textures.begin();
      }
      else {
        ++it;
      }
    }
  });
}

void OSLManager::texture_system_init()
{
  /* create texture system, shared between different renders to reduce memory usage */
  const thread_scoped_lock lock(ts_shared_mutex);

  if (!ts_shared) {
#  if OIIO_VERSION_MAJOR >= 3
    ts_shared = OSL::TextureSystem::create(false);
#  else
    ts_shared = std::shared_ptr<OSL::TextureSystem>(
        OSL::TextureSystem::create(false),
        [](OSL::TextureSystem *ts) { OSL::TextureSystem::destroy(ts); });
#  endif

    ts_shared->attribute("automip", 1);
    ts_shared->attribute("autotile", 64);
    ts_shared->attribute("gray_to_rgb", 1);

    /* effectively unlimited for now, until we support proper mipmap lookups */
    ts_shared->attribute("max_memory_MB", 16384);
  }

  /* make local copy to increase use count */
  ts = ts_shared;
}

void OSLManager::texture_system_free()
{
  ts.reset();

  /* if ts_shared is the only reference to the underlying texture system,
   * no users remain, so free it. */
  const thread_scoped_lock lock(ts_shared_mutex);
  if (ts_shared.use_count() == 1) {
    ts_shared.reset();
  }
}

void OSLManager::shading_system_init(const string &colorspace_interop_id)
{
  /* No need to do anything if we already have shading systems. */
  if (!ss_map.empty()) {
    return;
  }

  /* create shading system, shared between different renders to reduce memory usage */
  const thread_scoped_lock lock(ss_shared_mutex);

  foreach_osl_device(device_, [this, colorspace_interop_id](Device *sub_device, OSLGlobals *) {
    const DeviceType device_type = sub_device->info.type;

    if (!ss_shared[device_type]) {
      OSLRenderServices *services = util_aligned_new<OSLRenderServices>(get_texture_system(),
                                                                        device_type);
#  ifdef _WIN32
      /* Annoying thing, Cycles stores paths in UTF8 code-page, so it can
       * operate with file paths with any character. This requires to use wide
       * char functions, but OSL uses old fashioned ANSI functions which means:
       *
       * - We have to convert our paths to ANSI before passing to OSL
       * - OSL can't be used when there's a multi-byte character in the path
       *   to the shaders folder.
       */
      const string shader_path = string_to_ansi(path_get("shader"));
#  else
      const string shader_path = path_get("shader");
#  endif

      auto ss = std::shared_ptr<OSL::ShadingSystem>(
          new OSL::ShadingSystem(services, get_texture_system(), &errhandler),
          [](OSL::ShadingSystem *ss) {
            util_aligned_delete(static_cast<OSLRenderServices *>(ss->renderer()));
            delete ss;
          });
      ss->attribute("lockgeom", 1);
      ss->attribute("commonspace", "world");
      ss->attribute("searchpath:shader", shader_path);
      ss->attribute("greedyjit", 1);

      /* OSL doesn't accept an arbitrary space, so support a few specific spaces. */
      if (colorspace_interop_id == "lin_rec709_scene") {
        ss->attribute("colorspace", OSL::Strings::Rec709);
      }
      else if (colorspace_interop_id == "lin_rec2020_scene") {
        ss->attribute("colorspace", OSL::Strings::HDTV);
      }
      else if (colorspace_interop_id == "lin_ap1_scene") {
        ss->attribute("colorspace", OSL::Strings::ACEScg);
      }

      const char *groupdata_alloc_str = getenv("CYCLES_OSL_GROUPDATA_ALLOC");
      if (groupdata_alloc_str) {
        ss->attribute("max_optix_groupdata_alloc", atoi(groupdata_alloc_str));
      }
      else {
        ss->attribute("max_optix_groupdata_alloc", 2048);
      }

      LOG_INFO << "Using shader search path: " << shader_path;

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
      ss->attribute("raytypes", TypeDesc(TypeDesc::STRING, nraytypes), (const void *)raytypes);

      OSLRenderServices::register_closures(ss.get());
      ss_shared[device_type] = std::move(ss);
    }
    ss_map[device_type] = ss_shared[device_type];
  });
}

void OSLManager::shading_system_free()
{
  ss_map.clear();

  /* if ss_shared is the only reference to the underlying shading system,
   * no users remain, so free it. */
  const thread_scoped_lock lock(ss_shared_mutex);
  for (auto &[device_type, ss] : ss_shared) {
    if (ss.use_count() == 1) {
      ss.reset();
    }
  }

  loaded_shaders.clear();
}

bool OSLManager::osl_compile(const string &inputfile, const string &outputfile)
{
  vector<string> options;
  string stdosl_path;
  const string shader_path = path_get("shader");

  /* Specify output file name. */
  options.push_back("-o");
  options.push_back(outputfile);

  /* Specify standard include path. */
  const string include_path_arg = string("-I") + shader_path;
  options.push_back(include_path_arg);

  stdosl_path = path_join(shader_path, "stdcycles.h");

  /* Compile.
   *
   * Mutex protected because the OSL compiler does not appear to be thread safe, see #92503. */
  static thread_mutex osl_compiler_mutex;
  const thread_scoped_lock lock(osl_compiler_mutex);

  OSL::OSLCompiler compiler = OSL::OSLCompiler(&OSL::ErrorHandler::default_handler());
  const bool ok = compiler.compile(string_view(inputfile), options, string_view(stdosl_path));

  return ok;
}

bool OSLManager::osl_query(OSL::OSLQuery &query, const string &filepath)
{
  const string searchpath = path_user_get("shaders");
  return query.open(filepath, searchpath);
}

static string shader_filepath_hash(const string &filepath, const uint64_t modified_time)
{
  /* compute a hash from filepath and modified time to detect changes */
  MD5Hash md5;
  md5.append((const uint8_t *)filepath.c_str(), filepath.size());
  md5.append((const uint8_t *)&modified_time, sizeof(modified_time));

  return md5.get_hex();
}

const char *OSLManager::shader_test_loaded(const string &hash)
{
  const map<string, OSLShaderInfo>::iterator it = loaded_shaders.find(hash);
  return (it == loaded_shaders.end()) ? nullptr : it->first.c_str();
}

OSLShaderInfo *OSLManager::shader_loaded_info(const string &hash)
{
  const map<string, OSLShaderInfo>::iterator it = loaded_shaders.find(hash);
  return (it == loaded_shaders.end()) ? nullptr : &it->second;
}

const char *OSLManager::shader_load_filepath(string filepath)
{
  const size_t len = filepath.size();
  const string extension = filepath.substr(len - 4);
  uint64_t modified_time = path_modified_time(filepath);

  if (extension == ".osl") {
    /* .OSL File */
    const string osopath = filepath.substr(0, len - 4) + ".oso";
    const uint64_t oso_modified_time = path_modified_time(osopath);

    /* test if we have loaded the corresponding .OSO already */
    if (oso_modified_time != 0) {
      const char *hash = shader_test_loaded(shader_filepath_hash(osopath, oso_modified_time));

      if (hash) {
        return hash;
      }
    }

    /* Auto-compile .OSL to .OSO if needed. */
    if (oso_modified_time == 0 || (oso_modified_time < modified_time)) {
      OSLManager::osl_compile(filepath, osopath);
      modified_time = path_modified_time(osopath);
    }
    else {
      modified_time = oso_modified_time;
    }

    filepath = osopath;
  }
  else {
    if (extension == ".oso") {
      /* .OSO File, nothing to do */
    }
    else if (path_dirname(filepath).empty()) {
      /* .OSO File in search path */
      filepath = path_join(path_user_get("shaders"), filepath + ".oso");
    }
    else {
      /* unknown file */
      return nullptr;
    }

    /* test if we have loaded this .OSO already */
    const char *hash = shader_test_loaded(shader_filepath_hash(filepath, modified_time));

    if (hash) {
      return hash;
    }
  }

  /* read oso bytecode from file */
  const string bytecode_hash = shader_filepath_hash(filepath, modified_time);
  string bytecode;

  if (!path_read_text(filepath, bytecode)) {
    LOG_ERROR << "Shader graph: failed to read file " << filepath;
    const OSLShaderInfo info;
    loaded_shaders[bytecode_hash] = info; /* to avoid repeat tries */
    return nullptr;
  }

  return shader_load_bytecode(bytecode_hash, bytecode);
}

const char *OSLManager::shader_load_bytecode(const string &hash, const string &bytecode)
{
  foreach_shading_system(
      [hash, bytecode](OSL::ShadingSystem *ss) { ss->LoadMemoryCompiledShader(hash, bytecode); });

  tag_update();

  OSLShaderInfo info;

  if (!info.query.open_bytecode(bytecode)) {
    LOG_ERROR << "OSL query error: " << info.query.geterror();
  }

  /* this is a bit weak, but works */
  info.has_surface_emission = (bytecode.find("\"emission\"") != string::npos);
  info.has_surface_transparent = (bytecode.find("\"transparent\"") != string::npos);
  info.has_surface_bssrdf = (bytecode.find("\"bssrdf\"") != string::npos);

  loaded_shaders[hash] = info;

  return loaded_shaders.find(hash)->first.c_str();
}

uint64_t OSLShaderManager::get_attribute_id(ustring name)
{
  return name.hash();
}

uint64_t OSLShaderManager::get_attribute_id(AttributeStandard std)
{
  /* if standard attribute, use geom: name convention */
  const ustring stdname(string("geom:") + string(Attribute::standard_name(std)));
  return stdname.hash();
}

void OSLShaderManager::device_update_specific(Device *device,
                                              DeviceScene *dscene,
                                              Scene *scene,
                                              Progress &progress)
{
  if (!need_update()) {
    return;
  }

  scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->osl.times.add_entry({"device_update", time});
    }
  });

  LOG_INFO << "Total " << scene->shaders.size() << " shaders.";

  /* setup shader engine */
  OSLManager::foreach_osl_device(device, [scene](Device *sub_device, OSLGlobals *og) {
    OSL::ShadingSystem *ss = scene->osl_manager->get_shading_system(sub_device);
    og->ss = ss;
    og->ts = scene->osl_manager->get_texture_system();
    og->services = static_cast<OSLRenderServices *>(ss->renderer());

    og->use_shading = true;

    og->surface_state.clear();
    og->volume_state.clear();
    og->displacement_state.clear();
    og->bump_state.clear();
    og->background_state.reset();
  });

  /* create shaders */
  Shader *background_shader = scene->background->get_shader(scene);

  /* compile each shader to OSL shader groups */
  TaskPool task_pool;
  for (Shader *shader : scene->shaders) {
    assert(shader->graph);

    auto compile = [scene, shader, background_shader](Device *sub_device, OSLGlobals *) {
      OSL::ShadingSystem *ss = scene->osl_manager->get_shading_system(sub_device);

      OSLCompiler compiler(ss, scene, sub_device);
      compiler.background = (shader == background_shader);
      compiler.compile(shader);
    };

    task_pool.push([device, compile] { OSLManager::foreach_osl_device(device, compile); });
  }
  task_pool.wait_work();

  if (progress.get_cancel()) {
    return;
  }

  /* collect shader groups from all shaders */
  for (Shader *shader : scene->shaders) {
    OSLManager::OSLManager::foreach_osl_device(
        device, [shader, background_shader](Device *sub_device, OSLGlobals *og) {
          /* push state to array for lookup */
          const Shader::OSLCache &cache = shader->osl_cache[sub_device];
          og->surface_state.push_back(cache.surface);
          og->volume_state.push_back(cache.volume);
          og->displacement_state.push_back(cache.displacement);
          og->bump_state.push_back(cache.bump);

          if (shader == background_shader) {
            og->background_state = cache.surface;
          }
        });

    if (shader->emission_sampling != EMISSION_SAMPLING_NONE) {
      scene->light_manager->tag_update(scene, LightManager::SHADER_COMPILED);
    }

    scene->osl_manager->tag_update();
  }

  /* set background shader */
  int background_id = scene->shader_manager->get_shader_id(background_shader);

  OSLManager::foreach_osl_device(device, [background_id](Device *, OSLGlobals *og) {
    og->background_state = og->surface_state[background_id & SHADER_MASK];
  });

  for (Shader *shader : scene->shaders) {
    shader->clear_modified();
  }

  update_flags = UPDATE_NONE;

  device_update_common(device, dscene, scene, progress);
}

void OSLShaderManager::device_free(Device *device, DeviceScene *dscene, Scene *scene)
{
  device_free_common(device, dscene, scene);

  /* clear shader engine */
  OSLManager::foreach_osl_device(device, [](Device *, OSLGlobals *og) {
    og->use_shading = false;

    og->surface_state.clear();
    og->volume_state.clear();
    og->displacement_state.clear();
    og->bump_state.clear();
    og->background_state.reset();
  });
}

/* This is a static function to avoid RTTI link errors with only this
 * file being compiled without RTTI to match OSL and LLVM libraries. */
OSLNode *OSLShaderManager::osl_node(ShaderGraph *graph,
                                    Scene *scene,
                                    const std::string &filepath,
                                    const std::string &bytecode_hash,
                                    const std::string &bytecode)
{
  if (!scene->shader_manager->use_osl()) {
    return nullptr;
  }

  /* Ensure shading system exists before we try to load a shader. */
  scene->osl_manager->shading_system_init(scene->shader_manager->get_scene_linear_interop_id());

  /* Load shader code. */
  const char *hash;

  if (!filepath.empty()) {
    hash = scene->osl_manager->shader_load_filepath(filepath);
  }
  else {
    hash = scene->osl_manager->shader_test_loaded(bytecode_hash);
    if (!hash) {
      hash = scene->osl_manager->shader_load_bytecode(bytecode_hash, bytecode);
    }
  }

  if (!hash) {
    return nullptr;
  }

  OSLShaderInfo *info = scene->osl_manager->shader_loaded_info(hash);

  /* count number of inputs */
  size_t num_inputs = 0;

  for (int i = 0; i < info->query.nparams(); i++) {
    const OSL::OSLQuery::Parameter *param = info->query.getparam(i);

    /* skip unsupported types */
    if (param->varlenarray || param->isstruct || param->type.arraylen > 1) {
      continue;
    }

    if (!param->isoutput) {
      num_inputs++;
    }
  }

  /* create node */
  OSLNode *node = OSLNode::create(graph, num_inputs);

  /* add new sockets from parameters */
  const set<void *> used_sockets;

  for (int i = 0; i < info->query.nparams(); i++) {
    const OSL::OSLQuery::Parameter *param = info->query.getparam(i);

    /* skip unsupported types */
    if (param->varlenarray || param->isstruct || param->type.arraylen > 1) {
      continue;
    }

    SocketType::Type socket_type;

    /* Read type and default value. */
    if (param->isclosure) {
      socket_type = SocketType::CLOSURE;
    }
    else if (param->type.vecsemantics != TypeDesc::NOSEMANTICS) {
      if (param->type.vecsemantics == TypeDesc::COLOR) {
        socket_type = SocketType::COLOR;
      }
      else if (param->type.vecsemantics == TypeDesc::POINT) {
        socket_type = SocketType::POINT;
      }
      else if (param->type.vecsemantics == TypeDesc::VECTOR) {
        socket_type = SocketType::VECTOR;
      }
      else if (param->type.vecsemantics == TypeDesc::NORMAL) {
        socket_type = SocketType::NORMAL;
      }
      else {
        continue;
      }

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
      else {
        continue;
      }
    }
    else {
      continue;
    }

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
          else if (metadata.name == "defaultgeomprop") {
            /* the following match up to MaterialX default geometry properties
             * that we use to help set socket flags to the corresponding
             * geometry link equivalents. */
            if (metadata.sdefault[0] == "Nobject") {
              socket_flags |= SocketType::LINK_TEXTURE_NORMAL;
            }
            else if (metadata.sdefault[0] == "Nworld") {
              socket_flags |= SocketType::LINK_NORMAL;
            }
            else if (metadata.sdefault[0] == "Pobject") {
              socket_flags |= SocketType::LINK_TEXTURE_GENERATED;
            }
            else if (metadata.sdefault[0] == "Pworld") {
              socket_flags |= SocketType::LINK_POSITION;
            }
            else if (metadata.sdefault[0] == "Tworld") {
              socket_flags |= SocketType::LINK_TANGENT;
            }
            else if (metadata.sdefault[0] == "UV0") {
              socket_flags |= SocketType::LINK_TEXTURE_UV;
            }
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
    OSLGlobals *og = sub_device->get_cpu_osl_memory();
    services_shared.insert(og->services);
  });

  for (OSLRenderServices *services : services_shared) {
    for (auto it = services->textures.begin(); it != services->textures.end(); ++it) {
      if (it->second.handle.get_manager() == image_manager) {
        const int slot = it->second.handle.svm_image_texture_id();
        image_slots.insert(slot);
      }
    }
  }
}

/* Graph Compiler */

OSLCompiler::OSLCompiler(OSL::ShadingSystem *ss, Scene *scene, Device *device)
    : scene(scene),
      services(static_cast<OSLRenderServices *>(ss->renderer())),
      ss(ss),
      device(device)
{
  current_type = SHADER_TYPE_SURFACE;
  current_shader = nullptr;
  background = false;
}

string OSLCompiler::id(ShaderNode *node)
{
  /* assign layer unique name based on pointer address + bump mode */
  std::stringstream stream;

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
  while ((i = sname.find(" ")) != string::npos) {
    sname.replace(i, 1, "");
  }

  /* if output exists with the same name, add "In" suffix */
  for (ShaderOutput *output : node->outputs) {
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
  while ((i = sname.find(" ")) != string::npos) {
    sname.replace(i, 1, "");
  }

  /* if input exists with the same name, add "Out" suffix */
  for (ShaderInput *input : node->inputs) {
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

  if (input->flags() & SocketType::SVM_INTERNAL) {
    return true;
  }

  if (node->special_type == SHADER_SPECIAL_TYPE_OUTPUT) {
    if (input->name() == "Surface" && current_type != SHADER_TYPE_SURFACE) {
      return true;
    }
    if (input->name() == "Volume" && current_type != SHADER_TYPE_VOLUME) {
      return true;
    }
    if (input->name() == "Displacement" && current_type != SHADER_TYPE_DISPLACEMENT) {
      return true;
    }
    if (input->name() == "Normal" && current_type != SHADER_TYPE_BUMP) {
      return true;
    }
  }
  else if (node->special_type == SHADER_SPECIAL_TYPE_BUMP) {
    if (input->name() == "Height") {
      return true;
    }
  }
  else if (current_type == SHADER_TYPE_DISPLACEMENT && input->link &&
           input->link->parent->special_type == SHADER_SPECIAL_TYPE_BUMP)
  {
    return true;
  }

  return false;
}

void OSLCompiler::add(ShaderNode *node, const char *name, bool isfilepath)
{
  /* load filepath */
  if (isfilepath) {
    name = scene->osl_manager->shader_load_filepath(name);

    if (name == nullptr) {
      return;
    }
  }

  /* pass in fixed parameter values */
  for (ShaderInput *input : node->inputs) {
    if (!input->link) {
      /* checks to untangle graphs */
      if (node_skip_input(node, input)) {
        continue;
      }
      if ((input->flags() & SocketType::LINK_OSL_INITIALIZER) && !(input->constant_folded_in)) {
        continue;
      }

      const string param_name = compatible_name(node, input);
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
  if (current_type == SHADER_TYPE_SURFACE) {
    ss->Shader(*current_group, "surface", name, id(node));
  }
  else if (current_type == SHADER_TYPE_VOLUME) {
    ss->Shader(*current_group, "surface", name, id(node));
  }
  else if (current_type == SHADER_TYPE_DISPLACEMENT) {
    ss->Shader(*current_group, "displacement", name, id(node));
  }
  else if (current_type == SHADER_TYPE_BUMP) {
    ss->Shader(*current_group, "displacement", name, id(node));
  }
  else {
    assert(0);
  }

  /* link inputs to other nodes */
  for (ShaderInput *input : node->inputs) {
    if (input->link) {
      if (node_skip_input(node, input)) {
        continue;
      }

      /* connect shaders */
      const string id_from = id(input->link->parent);
      const string id_to = id(node);
      const string param_from = compatible_name(input->link->parent, input->link);
      const string param_to = compatible_name(node, input);

      ss->ConnectShaders(*current_group, id_from, param_from, id_to, param_to);
    }
  }

  /* test if we shader contains specific closures */
  OSLShaderInfo *info = scene->osl_manager->shader_loaded_info(name);

  if (current_type == SHADER_TYPE_SURFACE) {
    if (info) {
      if (info->has_surface_emission && node->special_type == SHADER_SPECIAL_TYPE_OSL) {
        /* Will be used by Shader::estimate_emission. */
        OSLNode *oslnode = static_cast<OSLNode *>(node);
        oslnode->has_emission = true;
      }
      if (info->has_surface_transparent) {
        current_shader->has_surface_transparent = true;
      }
      if (info->has_surface_bssrdf) {
        current_shader->has_surface_bssrdf = true;
        current_shader->has_bssrdf_bump = true; /* can't detect yet */
      }
      current_shader->has_bump_from_surface = true; /* can't detect yet */
      current_shader->has_surface_raytrace = true;  /* can't detect yet */
    }

    if (node->has_spatial_varying()) {
      current_shader->has_surface_spatial_varying = true;
    }
  }
  else if (current_type == SHADER_TYPE_VOLUME) {
    if (node->has_spatial_varying()) {
      current_shader->has_volume_spatial_varying = true;
    }
    if (node->has_attribute_dependency()) {
      current_shader->has_volume_attribute_dependency = true;
    }
  }
}

static TypeDesc array_typedesc(const TypeDesc typedesc, const int arraylength)
{
  return TypeDesc((TypeDesc::BASETYPE)typedesc.basetype,
                  (TypeDesc::AGGREGATE)typedesc.aggregate,
                  (TypeDesc::VECSEMANTICS)typedesc.vecsemantics,
                  arraylength);
}

void OSLCompiler::parameter(ShaderNode *node, const char *name)
{
  const ustring uname = ustring(name);
  const SocketType &socket = *(node->type->find_input(uname));

  switch (socket.type) {
    case SocketType::BOOLEAN: {
      int value = node->get_bool(socket);
      ss->Parameter(*current_group, name, TypeInt, &value);
      break;
    }
    case SocketType::FLOAT: {
      float value = node->get_float(socket);
      ss->Parameter(*current_group, uname, TypeFloat, &value);
      break;
    }
    case SocketType::INT: {
      int value = node->get_int(socket);
      ss->Parameter(*current_group, uname, TypeInt, &value);
      break;
    }
    case SocketType::COLOR: {
      float3 value = node->get_float3(socket);
      ss->Parameter(*current_group, uname, TypeColor, &value);
      break;
    }
    case SocketType::VECTOR: {
      float3 value = node->get_float3(socket);
      ss->Parameter(*current_group, uname, TypeVector, &value);
      break;
    }
    case SocketType::POINT: {
      float3 value = node->get_float3(socket);
      ss->Parameter(*current_group, uname, TypePoint, &value);
      break;
    }
    case SocketType::NORMAL: {
      float3 value = node->get_float3(socket);
      ss->Parameter(*current_group, uname, TypeNormal, &value);
      break;
    }
    case SocketType::POINT2: {
      float2 value = node->get_float2(socket);
      ss->Parameter(*current_group,
                    uname,
                    TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC2, TypeDesc::POINT),
                    &value);
      break;
    }
    case SocketType::STRING: {
      ustring value = node->get_string(socket);
      ss->Parameter(*current_group, uname, TypeString, &value);
      break;
    }
    case SocketType::ENUM: {
      ustring value = node->get_string(socket);
      ss->Parameter(*current_group, uname, TypeString, &value);
      break;
    }
    case SocketType::TRANSFORM: {
      const Transform value = node->get_transform(socket);
      ProjectionTransform projection(value);
      projection = projection_transpose(projection);
      ss->Parameter(*current_group, uname, TypeMatrix, &projection);
      break;
    }
    case SocketType::BOOLEAN_ARRAY: {
      // OSL does not support booleans, so convert to int
      const array<bool> &value = node->get_bool_array(socket);
      array<int> intvalue(value.size());
      for (size_t i = 0; i < value.size(); i++) {
        intvalue[i] = value[i];
      }
      ss->Parameter(*current_group, uname, array_typedesc(TypeInt, value.size()), intvalue.data());
      break;
    }
    case SocketType::FLOAT_ARRAY: {
      const array<float> &value = node->get_float_array(socket);
      ss->Parameter(*current_group, uname, array_typedesc(TypeFloat, value.size()), value.data());
      break;
    }
    case SocketType::INT_ARRAY: {
      const array<int> &value = node->get_int_array(socket);
      ss->Parameter(*current_group, uname, array_typedesc(TypeInt, value.size()), value.data());
      break;
    }
    case SocketType::COLOR_ARRAY:
    case SocketType::VECTOR_ARRAY:
    case SocketType::POINT_ARRAY:
    case SocketType::NORMAL_ARRAY: {
      TypeDesc typedesc;

      switch (socket.type) {
        case SocketType::COLOR_ARRAY:
          typedesc = TypeColor;
          break;
        case SocketType::VECTOR_ARRAY:
          typedesc = TypeVector;
          break;
        case SocketType::POINT_ARRAY:
          typedesc = TypePoint;
          break;
        case SocketType::NORMAL_ARRAY:
          typedesc = TypeNormal;
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

      ss->Parameter(*current_group, uname, array_typedesc(typedesc, value.size()), fvalue.data());
      break;
    }
    case SocketType::POINT2_ARRAY: {
      const array<float2> &value = node->get_float2_array(socket);
      ss->Parameter(
          *current_group,
          uname,
          array_typedesc(TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC2, TypeDesc::POINT), value.size()),
          value.data());
      break;
    }
    case SocketType::STRING_ARRAY: {
      const array<ustring> &value = node->get_string_array(socket);
      ss->Parameter(*current_group, uname, array_typedesc(TypeString, value.size()), value.data());
      break;
    }
    case SocketType::TRANSFORM_ARRAY: {
      const array<Transform> &value = node->get_transform_array(socket);
      array<ProjectionTransform> fvalue(value.size());
      for (size_t i = 0; i < value.size(); i++) {
        fvalue[i] = projection_transpose(ProjectionTransform(value[i]));
      }
      ss->Parameter(
          *current_group, uname, array_typedesc(TypeMatrix, fvalue.size()), fvalue.data());
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

void OSLCompiler::parameter(const char *name, const float f)
{
  ss->Parameter(*current_group, name, TypeFloat, &f);
}

void OSLCompiler::parameter_color(const char *name, const float3 f)
{
  ss->Parameter(*current_group, name, TypeColor, &f);
}

void OSLCompiler::parameter_point(const char *name, const float3 f)
{
  ss->Parameter(*current_group, name, TypePoint, &f);
}

void OSLCompiler::parameter_normal(const char *name, const float3 f)
{
  ss->Parameter(*current_group, name, TypeNormal, &f);
}

void OSLCompiler::parameter_vector(const char *name, const float3 f)
{
  ss->Parameter(*current_group, name, TypeVector, &f);
}

void OSLCompiler::parameter(const char *name, const int f)
{
  ss->Parameter(*current_group, name, TypeInt, &f);
}

void OSLCompiler::parameter(const char *name, const char *s)
{
  ss->Parameter(*current_group, name, TypeString, (const void *)&s);
}

void OSLCompiler::parameter(const char *name, ustring s)
{
  const char *str = s.c_str();
  ss->Parameter(*current_group, name, TypeString, (const void *)&str);
}

void OSLCompiler::parameter(const char *name, const Transform &tfm)
{
  ProjectionTransform projection(tfm);
  projection = projection_transpose(projection);
  ss->Parameter(*current_group, name, TypeMatrix, (float *)&projection);
}

void OSLCompiler::parameter_array(const char *name, const float f[], int arraylen)
{
  TypeDesc type = TypeFloat;
  type.arraylen = arraylen;
  ss->Parameter(*current_group, name, type, f);
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

  TypeDesc type = TypeColor;
  type.arraylen = table.size();
  ss->Parameter(*current_group, name, type, table.data());
}

void OSLCompiler::parameter_attribute(const char *name, ustring s)
{
  if (Attribute::name_standard(s.c_str())) {
    parameter(name, (string("geom:") + s.c_str()).c_str());
  }
  else {
    parameter(name, s.c_str());
  }
}

void OSLCompiler::find_dependencies(ShaderNodeSet &dependencies, ShaderInput *input)
{
  ShaderNode *node = (input->link) ? input->link->parent : nullptr;

  if (node != nullptr && dependencies.find(node) == dependencies.end()) {
    for (ShaderInput *in : node->inputs) {
      if (!node_skip_input(node, in)) {
        find_dependencies(dependencies, in);
      }
    }

    dependencies.insert(node);
  }
}

void OSLCompiler::generate_nodes(const ShaderNodeSet &nodes)
{
  ShaderNodeSet done;
  bool nodes_done;

  do {
    nodes_done = true;

    for (ShaderNode *node : nodes) {
      if (done.find(node) == done.end()) {
        bool inputs_done = true;

        for (ShaderInput *input : node->inputs) {
          if (!node_skip_input(node, input)) {
            if (input->link && done.find(input->link->parent) == done.end()) {
              inputs_done = false;
            }
          }
        }

        if (inputs_done) {
          node->compile(*this);
          done.insert(node);

          if (current_type == SHADER_TYPE_SURFACE) {
            if (node->has_surface_transparent()) {
              current_shader->has_surface_transparent = true;
            }
            if (node->get_feature() & KERNEL_FEATURE_NODE_RAYTRACE) {
              current_shader->has_surface_raytrace = true;
            }
            if (node->has_spatial_varying()) {
              current_shader->has_surface_spatial_varying = true;
            }
            if (node->has_surface_bssrdf()) {
              current_shader->has_surface_bssrdf = true;
              if (node->has_bssrdf_bump()) {
                current_shader->has_bssrdf_bump = true;
              }
            }
            if (node->has_bump()) {
              current_shader->has_bump_from_surface = true;
            }
          }
          else if (current_type == SHADER_TYPE_VOLUME) {
            if (node->has_spatial_varying()) {
              current_shader->has_volume_spatial_varying = true;
            }
          }
        }
        else {
          nodes_done = false;
        }
      }
    }
  } while (!nodes_done);
}

OSL::ShaderGroupRef OSLCompiler::compile_type(Shader *shader, ShaderGraph *graph, ShaderType type)
{
  current_type = type;

  /* Use name hash to identify shader group to avoid issues with non-alphanumeric characters */
  std::stringstream name;
  name.imbue(std::locale("C"));
  name << "shader_" << shader->name.hash();

  current_group = ss->ShaderGroupBegin(name.str());

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
  else {
    assert(0);
  }

  ss->ShaderGroupEnd(*current_group);

  return std::move(current_group);
}

void OSLCompiler::compile(Shader *shader)
{
  if (shader->is_modified()) {
    ShaderGraph *graph = shader->graph.get();

    current_shader = shader;

    Shader::OSLCache cache;

    if (shader->reference_count()) {
      if (shader->has_surface) {
        cache.surface = compile_type(shader, graph, SHADER_TYPE_SURFACE);
        if (shader->has_bump_from_displacement) {
          cache.bump = compile_type(shader, graph, SHADER_TYPE_BUMP);
        }
      }
      if (shader->has_volume) {
        cache.volume = compile_type(shader, graph, SHADER_TYPE_VOLUME);
      }
      if (shader->has_displacement) {
        cache.displacement = compile_type(shader, graph, SHADER_TYPE_DISPLACEMENT);
      }
    }

    shader->osl_cache[device] = std::move(cache);

    /* Estimate emission for MIS. */
    shader->estimate_emission();
  }
}

void OSLCompiler::parameter_texture(const char *name, ustring filename, ustring colorspace)
{
  /* Textured loaded through the OpenImageIO texture cache. For this
   * case we need to do runtime color space conversion. */
  OSLTextureHandle handle(OSLTextureHandle::OIIO);
  handle.processor = ColorSpaceManager::get_processor(colorspace);
  services->textures.insert(OSLUStringHash(filename), handle);
  parameter(name, filename);
}

void OSLCompiler::parameter_texture(const char *name, const ImageHandle &handle)
{
  /* Texture loaded through SVM image texture system. We generate a unique
   * name, which ends up being used in OSLRenderServices::get_texture_handle
   * to get handle again. Note that this name must be unique between multiple
   * render sessions as the render services are shared. */
  const ustring filename(string_printf("@svm%d", texture_shared_unique_id++).c_str());
  services->textures.insert(
      OSLUStringHash(filename),
      OSLTextureHandle(OSLTextureHandle::SVM, handle.get_svm_image_texture_ids()));
  parameter(name, filename);
}

void OSLCompiler::parameter_texture_ies(const char *name, const int svm_image_texture_id)
{
  /* IES light textures stored in SVM. */
  const ustring filename(string_printf("@svm%d", texture_shared_unique_id++).c_str());
  services->textures.insert(OSLUStringHash(filename),
                            OSLTextureHandle(OSLTextureHandle::IES, svm_image_texture_id));
  parameter(name, filename);
}

#else

OSLManager::OSLManager(Device * /*device*/) {}
OSLManager::~OSLManager() {}

void OSLManager::free_memory() {}
void OSLManager::reset(Scene * /*scene*/) {}

void OSLManager::device_update_pre(Device * /*device*/, Scene * /*scene*/) {}
void OSLManager::device_update_post(Device * /*device*/,
                                    Scene * /*scene*/,
                                    Progress & /*progress*/,
                                    const bool /*reload_kernels*/)
{
}
void OSLManager::device_free(Device * /*device*/, DeviceScene * /*dscene*/, Scene * /*scene*/) {}

void OSLManager::tag_update() {}
bool OSLManager::need_update() const
{
  return false;
}

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

void OSLCompiler::parameter_texture(const char * /*name*/,
                                    ustring /*filename*/,
                                    ustring /*colorspace*/)
{
}

void OSLCompiler::parameter_texture(const char * /*name*/, const ImageHandle & /*handle*/) {}

void OSLCompiler::parameter_texture_ies(const char * /*name*/, int /*svm_image_texture_id*/) {}

#endif /* WITH_OSL */

CCL_NAMESPACE_END

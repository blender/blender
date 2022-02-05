/*
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
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "BLI_string_utils.h"

#include "GPU_capabilities.h"
#include "GPU_debug.h"
#include "GPU_matrix.h"
#include "GPU_platform.h"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_shader_create_info.hh"
#include "gpu_shader_create_info_private.hh"
#include "gpu_shader_dependency_private.h"
#include "gpu_shader_private.hh"

#include <string>

extern "C" char datatoc_gpu_shader_colorspace_lib_glsl[];

namespace blender::gpu {

std::string Shader::defines_declare(const shader::ShaderCreateInfo &info) const
{
  std::string defines;
  for (const auto &def : info.defines_) {
    defines += "#define ";
    defines += def[0];
    defines += " ";
    defines += def[1];
    defines += "\n";
  }
  return defines;
}

}  // namespace blender::gpu

using namespace blender;
using namespace blender::gpu;

static bool gpu_shader_srgb_uniform_dirty_get();

/* -------------------------------------------------------------------- */
/** \name Creation / Destruction
 * \{ */

Shader::Shader(const char *sh_name)
{
  BLI_strncpy(this->name, sh_name, sizeof(this->name));
}

Shader::~Shader()
{
  delete interface;
}

static void standard_defines(Vector<const char *> &sources)
{
  BLI_assert(sources.size() == 0);
  /* Version needs to be first. Exact values will be added by implementation. */
  sources.append("version");
  /* Define to identify code usage in shading language. */
  sources.append("#define GPU_SHADER\n");
  /* some useful defines to detect GPU type */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    sources.append("#define GPU_ATI\n");
  }
  else if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    sources.append("#define GPU_NVIDIA\n");
  }
  else if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    sources.append("#define GPU_INTEL\n");
  }
  /* some useful defines to detect OS type */
  if (GPU_type_matches(GPU_DEVICE_ANY, GPU_OS_WIN, GPU_DRIVER_ANY)) {
    sources.append("#define OS_WIN\n");
  }
  else if (GPU_type_matches(GPU_DEVICE_ANY, GPU_OS_MAC, GPU_DRIVER_ANY)) {
    sources.append("#define OS_MAC\n");
  }
  else if (GPU_type_matches(GPU_DEVICE_ANY, GPU_OS_UNIX, GPU_DRIVER_ANY)) {
    sources.append("#define OS_UNIX\n");
  }

  if (GPU_crappy_amd_driver()) {
    sources.append("#define GPU_DEPRECATED_AMD_DRIVER\n");
  }
}

GPUShader *GPU_shader_create_ex(const char *vertcode,
                                const char *fragcode,
                                const char *geomcode,
                                const char *computecode,
                                const char *libcode,
                                const char *defines,
                                const eGPUShaderTFBType tf_type,
                                const char **tf_names,
                                const int tf_count,
                                const char *shname)
{
  /* At least a vertex shader and a fragment shader are required, or only a compute shader. */
  BLI_assert(((fragcode != nullptr) && (vertcode != nullptr) && (computecode == nullptr)) ||
             ((fragcode == nullptr) && (vertcode == nullptr) && (geomcode == nullptr) &&
              (computecode != nullptr)));

  Shader *shader = GPUBackend::get()->shader_alloc(shname);

  if (vertcode) {
    Vector<const char *> sources;
    standard_defines(sources);
    sources.append("#define GPU_VERTEX_SHADER\n");
    sources.append("#define IN_OUT out\n");
    if (geomcode) {
      sources.append("#define USE_GEOMETRY_SHADER\n");
    }
    if (defines) {
      sources.append(defines);
    }
    sources.append(vertcode);

    shader->vertex_shader_from_glsl(sources);
  }

  if (fragcode) {
    Vector<const char *> sources;
    standard_defines(sources);
    sources.append("#define GPU_FRAGMENT_SHADER\n");
    sources.append("#define IN_OUT in\n");
    if (geomcode) {
      sources.append("#define USE_GEOMETRY_SHADER\n");
    }
    if (defines) {
      sources.append(defines);
    }
    if (libcode) {
      sources.append(libcode);
    }
    sources.append(fragcode);

    shader->fragment_shader_from_glsl(sources);
  }

  if (geomcode) {
    Vector<const char *> sources;
    standard_defines(sources);
    sources.append("#define GPU_GEOMETRY_SHADER\n");
    if (defines) {
      sources.append(defines);
    }
    sources.append(geomcode);

    shader->geometry_shader_from_glsl(sources);
  }

  if (computecode) {
    Vector<const char *> sources;
    standard_defines(sources);
    sources.append("#define GPU_COMPUTE_SHADER\n");
    if (defines) {
      sources.append(defines);
    }
    if (libcode) {
      sources.append(libcode);
    }
    sources.append(computecode);

    shader->compute_shader_from_glsl(sources);
  }

  if (tf_names != nullptr && tf_count > 0) {
    BLI_assert(tf_type != GPU_SHADER_TFB_NONE);
    shader->transform_feedback_names_set(Span<const char *>(tf_names, tf_count), tf_type);
  }

  if (!shader->finalize()) {
    delete shader;
    return nullptr;
  };

  return wrap(shader);
}

void GPU_shader_free(GPUShader *shader)
{
  delete unwrap(shader);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation utils
 * \{ */

GPUShader *GPU_shader_create(const char *vertcode,
                             const char *fragcode,
                             const char *geomcode,
                             const char *libcode,
                             const char *defines,
                             const char *shname)
{
  return GPU_shader_create_ex(vertcode,
                              fragcode,
                              geomcode,
                              nullptr,
                              libcode,
                              defines,
                              GPU_SHADER_TFB_NONE,
                              nullptr,
                              0,
                              shname);
}

GPUShader *GPU_shader_create_compute(const char *computecode,
                                     const char *libcode,
                                     const char *defines,
                                     const char *shname)
{
  return GPU_shader_create_ex(nullptr,
                              nullptr,
                              nullptr,
                              computecode,
                              libcode,
                              defines,
                              GPU_SHADER_TFB_NONE,
                              nullptr,
                              0,
                              shname);
}

GPUShader *GPU_shader_create_from_info_name(const char *info_name)
{
  using namespace blender::gpu::shader;
  const GPUShaderCreateInfo *_info = gpu_shader_create_info_get(info_name);
  const ShaderCreateInfo &info = *reinterpret_cast<const ShaderCreateInfo *>(_info);
  if (!info.do_static_compilation_) {
    printf("Warning: Trying to compile \"%s\" which was not marked for static compilation.\n",
           info.name_.c_str());
  }
  return GPU_shader_create_from_info(_info);
}

GPUShader *GPU_shader_create_from_info(const GPUShaderCreateInfo *_info)
{
  using namespace blender::gpu::shader;
  const ShaderCreateInfo &info = *reinterpret_cast<const ShaderCreateInfo *>(_info);

  const_cast<ShaderCreateInfo &>(info).finalize();

  GPU_debug_group_begin(GPU_DEBUG_SHADER_COMPILATION_GROUP);

  /* At least a vertex shader and a fragment shader are required, or only a compute shader. */
  if (info.compute_source_.is_empty()) {
    if (info.vertex_source_.is_empty()) {
      printf("Missing vertex shader in %s.\n", info.name_.c_str());
    }
    if (info.fragment_source_.is_empty()) {
      printf("Missing fragment shader in %s.\n", info.name_.c_str());
    }
    BLI_assert(!info.vertex_source_.is_empty() && !info.fragment_source_.is_empty());
  }
  else {
    if (!info.vertex_source_.is_empty()) {
      printf("Compute shader has vertex_source_ shader attached in %s.\n", info.name_.c_str());
    }
    if (!info.geometry_source_.is_empty()) {
      printf("Compute shader has geometry_source_ shader attached in %s.\n", info.name_.c_str());
    }
    if (!info.fragment_source_.is_empty()) {
      printf("Compute shader has fragment_source_ shader attached in %s.\n", info.name_.c_str());
    }
    BLI_assert(info.vertex_source_.is_empty() && info.geometry_source_.is_empty() &&
               info.fragment_source_.is_empty());
  }

  Shader *shader = GPUBackend::get()->shader_alloc(info.name_.c_str());

  std::string defines = shader->defines_declare(info);
  std::string resources = shader->resources_declare(info);
  char *shader_shared_utils = nullptr;

  defines += "#define USE_GPU_SHADER_CREATE_INFO\n";

  Vector<char *> typedefs;
  for (auto filename : info.typedef_sources_) {
    typedefs.append(gpu_shader_dependency_get_source(filename.c_str()));
  }
  if (!typedefs.is_empty()) {
    shader_shared_utils = gpu_shader_dependency_get_source("gpu_shader_shared_utils.h");
  }

  if (!info.vertex_source_.is_empty()) {
    uint32_t builtins = 0;
    std::string interface = shader->vertex_interface_declare(info);
    char *code = gpu_shader_dependency_get_resolved_source(info.vertex_source_.c_str(), &builtins);

    Vector<const char *> sources;
    standard_defines(sources);
    sources.append("#define GPU_VERTEX_SHADER\n");
    if (!info.geometry_source_.is_empty()) {
      sources.append("#define USE_GEOMETRY_SHADER\n");
    }
    sources.append(defines.c_str());
    if (!typedefs.is_empty()) {
      sources.append(shader_shared_utils);
    }
    for (auto *types : typedefs) {
      sources.append(types);
    }
    sources.append(resources.c_str());
    sources.append(interface.c_str());
    sources.append(code);

    shader->vertex_shader_from_glsl(sources);

    free(code);
  }

  if (!info.fragment_source_.is_empty()) {
    uint32_t builtins = 0;
    std::string interface = shader->fragment_interface_declare(info);
    char *code = gpu_shader_dependency_get_resolved_source(info.fragment_source_.c_str(),
                                                           &builtins);

    Vector<const char *> sources;
    standard_defines(sources);
    sources.append("#define GPU_FRAGMENT_SHADER\n");
    if (!info.geometry_source_.is_empty()) {
      sources.append("#define USE_GEOMETRY_SHADER\n");
    }
    sources.append(defines.c_str());
    if (!typedefs.is_empty()) {
      sources.append(shader_shared_utils);
    }
    for (auto *types : typedefs) {
      sources.append(types);
    }
    sources.append(resources.c_str());
    sources.append(interface.c_str());
    sources.append(code);

    shader->fragment_shader_from_glsl(sources);

    free(code);
  }

  if (!info.geometry_source_.is_empty()) {
    uint32_t builtins = 0;
    std::string interface = shader->geometry_interface_declare(info);
    std::string layout = shader->geometry_layout_declare(info);
    char *code = gpu_shader_dependency_get_resolved_source(info.geometry_source_.c_str(),
                                                           &builtins);

    Vector<const char *> sources;
    standard_defines(sources);
    sources.append("#define GPU_GEOMETRY_SHADER\n");
    sources.append(defines.c_str());
    if (!typedefs.is_empty()) {
      sources.append(shader_shared_utils);
    }
    for (auto *types : typedefs) {
      sources.append(types);
    }
    sources.append(resources.c_str());
    sources.append(layout.c_str());
    sources.append(interface.c_str());
    sources.append(code);

    shader->geometry_shader_from_glsl(sources);

    free(code);
  }

  if (!info.compute_source_.is_empty()) {
    uint32_t builtins = 0;
    char *code = gpu_shader_dependency_get_resolved_source(info.compute_source_.c_str(),
                                                           &builtins);
    std::string layout = shader->compute_layout_declare(info);

    Vector<const char *> sources;
    standard_defines(sources);
    sources.append("#define GPU_COMPUTE_SHADER\n");
    sources.append(defines.c_str());
    if (!typedefs.is_empty()) {
      sources.append(shader_shared_utils);
    }
    for (auto *types : typedefs) {
      sources.append(types);
    }
    sources.append(resources.c_str());
    sources.append(layout.c_str());
    sources.append(code);

    shader->compute_shader_from_glsl(sources);

    free(code);
  }

  for (auto *types : typedefs) {
    free(types);
  }

  if (shader_shared_utils) {
    free(shader_shared_utils);
  }

  if (!shader->finalize(&info)) {
    delete shader;
    GPU_debug_group_end();
    return nullptr;
  }

  GPU_debug_group_end();
  return wrap(shader);
}

GPUShader *GPU_shader_create_from_python(const char *vertcode,
                                         const char *fragcode,
                                         const char *geomcode,
                                         const char *libcode,
                                         const char *defines,
                                         const char *name)
{
  char *libcodecat = nullptr;

  if (libcode == nullptr) {
    libcode = datatoc_gpu_shader_colorspace_lib_glsl;
  }
  else {
    libcode = libcodecat = BLI_strdupcat(libcode, datatoc_gpu_shader_colorspace_lib_glsl);
  }

  /* Use pyGPUShader as default name for shader. */
  const char *shname = name != nullptr ? name : "pyGPUShader";

  GPUShader *sh = GPU_shader_create_ex(vertcode,
                                       fragcode,
                                       geomcode,
                                       nullptr,
                                       libcode,
                                       defines,
                                       GPU_SHADER_TFB_NONE,
                                       nullptr,
                                       0,
                                       shname);

  MEM_SAFE_FREE(libcodecat);
  return sh;
}

static const char *string_join_array_maybe_alloc(const char **str_arr, bool *r_is_alloc)
{
  bool is_alloc = false;
  if (str_arr == nullptr) {
    *r_is_alloc = false;
    return nullptr;
  }
  /* Skip empty strings (avoid alloc if we can). */
  while (str_arr[0] && str_arr[0][0] == '\0') {
    str_arr++;
  }
  int i;
  for (i = 0; str_arr[i]; i++) {
    if (i != 0 && str_arr[i][0] != '\0') {
      is_alloc = true;
    }
  }
  *r_is_alloc = is_alloc;
  if (is_alloc) {
    return BLI_string_join_arrayN(str_arr, i);
  }

  return str_arr[0];
}

struct GPUShader *GPU_shader_create_from_arrays_impl(
    const struct GPU_ShaderCreateFromArray_Params *params, const char *func, int line)
{
  struct {
    const char *str;
    bool is_alloc;
  } str_dst[4] = {{nullptr}};
  const char **str_src[4] = {params->vert, params->frag, params->geom, params->defs};

  for (int i = 0; i < ARRAY_SIZE(str_src); i++) {
    str_dst[i].str = string_join_array_maybe_alloc(str_src[i], &str_dst[i].is_alloc);
  }

  char name[64];
  BLI_snprintf(name, sizeof(name), "%s_%d", func, line);

  GPUShader *sh = GPU_shader_create(
      str_dst[0].str, str_dst[1].str, str_dst[2].str, nullptr, str_dst[3].str, name);

  for (auto &i : str_dst) {
    if (i.is_alloc) {
      MEM_freeN((void *)i.str);
    }
  }
  return sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

void GPU_shader_bind(GPUShader *gpu_shader)
{
  Shader *shader = unwrap(gpu_shader);

  Context *ctx = Context::get();

  if (ctx->shader != shader) {
    ctx->shader = shader;
    shader->bind();
    GPU_matrix_bind(gpu_shader);
    GPU_shader_set_srgb_uniform(gpu_shader);
  }
  else {
    if (gpu_shader_srgb_uniform_dirty_get()) {
      GPU_shader_set_srgb_uniform(gpu_shader);
    }
    if (GPU_matrix_dirty_get()) {
      GPU_matrix_bind(gpu_shader);
    }
  }
}

void GPU_shader_unbind()
{
#ifndef NDEBUG
  Context *ctx = Context::get();
  if (ctx->shader) {
    ctx->shader->unbind();
  }
  ctx->shader = nullptr;
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader name
 * \{ */

const char *GPU_shader_get_name(GPUShader *shader)
{
  return unwrap(shader)->name_get();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform feedback
 *
 * TODO(fclem): Should be replaced by compute shaders.
 * \{ */

bool GPU_shader_transform_feedback_enable(GPUShader *shader, GPUVertBuf *vertbuf)
{
  return unwrap(shader)->transform_feedback_enable(vertbuf);
}

void GPU_shader_transform_feedback_disable(GPUShader *shader)
{
  unwrap(shader)->transform_feedback_disable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms / Resource location
 * \{ */

int GPU_shader_get_uniform(GPUShader *shader, const char *name)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *uniform = interface->uniform_get(name);
  return uniform ? uniform->location : -1;
}

int GPU_shader_get_builtin_uniform(GPUShader *shader, int builtin)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  return interface->uniform_builtin((GPUUniformBuiltin)builtin);
}

int GPU_shader_get_builtin_block(GPUShader *shader, int builtin)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  return interface->ubo_builtin((GPUUniformBlockBuiltin)builtin);
}

int GPU_shader_get_ssbo(GPUShader *shader, const char *name)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *ssbo = interface->ssbo_get(name);
  return ssbo ? ssbo->location : -1;
}

int GPU_shader_get_uniform_block(GPUShader *shader, const char *name)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *ubo = interface->ubo_get(name);
  return ubo ? ubo->location : -1;
}

int GPU_shader_get_uniform_block_binding(GPUShader *shader, const char *name)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *ubo = interface->ubo_get(name);
  return ubo ? ubo->binding : -1;
}

int GPU_shader_get_texture_binding(GPUShader *shader, const char *name)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *tex = interface->uniform_get(name);
  return tex ? tex->binding : -1;
}

int GPU_shader_get_attribute(GPUShader *shader, const char *name)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *attr = interface->attr_get(name);
  return attr ? attr->location : -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Getters
 * \{ */

int GPU_shader_get_program(GPUShader *shader)
{
  return unwrap(shader)->program_handle_get();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms setters
 * \{ */

void GPU_shader_uniform_vector(
    GPUShader *shader, int loc, int len, int arraysize, const float *value)
{
  unwrap(shader)->uniform_float(loc, len, arraysize, value);
}

void GPU_shader_uniform_vector_int(
    GPUShader *shader, int loc, int len, int arraysize, const int *value)
{
  unwrap(shader)->uniform_int(loc, len, arraysize, value);
}

void GPU_shader_uniform_int(GPUShader *shader, int location, int value)
{
  GPU_shader_uniform_vector_int(shader, location, 1, 1, &value);
}

void GPU_shader_uniform_float(GPUShader *shader, int location, float value)
{
  GPU_shader_uniform_vector(shader, location, 1, 1, &value);
}

void GPU_shader_uniform_1i(GPUShader *sh, const char *name, int value)
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_int(sh, loc, value);
}

void GPU_shader_uniform_1b(GPUShader *sh, const char *name, bool value)
{
  GPU_shader_uniform_1i(sh, name, value ? 1 : 0);
}

void GPU_shader_uniform_2f(GPUShader *sh, const char *name, float x, float y)
{
  const float data[2] = {x, y};
  GPU_shader_uniform_2fv(sh, name, data);
}

void GPU_shader_uniform_3f(GPUShader *sh, const char *name, float x, float y, float z)
{
  const float data[3] = {x, y, z};
  GPU_shader_uniform_3fv(sh, name, data);
}

void GPU_shader_uniform_4f(GPUShader *sh, const char *name, float x, float y, float z, float w)
{
  const float data[4] = {x, y, z, w};
  GPU_shader_uniform_4fv(sh, name, data);
}

void GPU_shader_uniform_1f(GPUShader *sh, const char *name, float value)
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float(sh, loc, value);
}

void GPU_shader_uniform_2fv(GPUShader *sh, const char *name, const float data[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_vector(sh, loc, 2, 1, data);
}

void GPU_shader_uniform_3fv(GPUShader *sh, const char *name, const float data[3])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_vector(sh, loc, 3, 1, data);
}

void GPU_shader_uniform_4fv(GPUShader *sh, const char *name, const float data[4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_vector(sh, loc, 4, 1, data);
}

void GPU_shader_uniform_mat4(GPUShader *sh, const char *name, const float data[4][4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_vector(sh, loc, 16, 1, (const float *)data);
}

void GPU_shader_uniform_2fv_array(GPUShader *sh, const char *name, int len, const float (*val)[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_vector(sh, loc, 2, len, (const float *)val);
}

void GPU_shader_uniform_4fv_array(GPUShader *sh, const char *name, int len, const float (*val)[4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_vector(sh, loc, 4, len, (const float *)val);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name sRGB Rendering Workaround
 *
 * The viewport overlay frame-buffer is sRGB and will expect shaders to output display referred
 * Linear colors. But other frame-buffers (i.e: the area frame-buffers) are not sRGB and require
 * the shader output color to be in sRGB space
 * (assumed display encoded color-space as the time of writing).
 * For this reason we have a uniform to switch the transform on and off depending on the current
 * frame-buffer color-space.
 * \{ */

static int g_shader_builtin_srgb_transform = 0;
static bool g_shader_builtin_srgb_is_dirty = false;

static bool gpu_shader_srgb_uniform_dirty_get()
{
  return g_shader_builtin_srgb_is_dirty;
}

void GPU_shader_set_srgb_uniform(GPUShader *shader)
{
  int32_t loc = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_SRGB_TRANSFORM);
  if (loc != -1) {
    GPU_shader_uniform_vector_int(shader, loc, 1, 1, &g_shader_builtin_srgb_transform);
  }
  g_shader_builtin_srgb_is_dirty = false;
}

void GPU_shader_set_framebuffer_srgb_target(int use_srgb_to_linear)
{
  if (g_shader_builtin_srgb_transform != use_srgb_to_linear) {
    g_shader_builtin_srgb_transform = use_srgb_to_linear;
    g_shader_builtin_srgb_is_dirty = true;
  }
}

/** \} */

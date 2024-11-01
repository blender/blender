/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "GPU_capabilities.hh"
#include "GPU_debug.hh"
#include "GPU_matrix.hh"
#include "GPU_platform.hh"

#include "glsl_preprocess/glsl_preprocess.hh"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_shader_create_info.hh"
#include "gpu_shader_create_info_private.hh"
#include "gpu_shader_dependency_private.hh"
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

/* -------------------------------------------------------------------- */
/** \name Creation / Destruction
 * \{ */

Shader::Shader(const char *sh_name)
{
  STRNCPY(this->name, sh_name);
}

Shader::~Shader()
{
  delete interface;
}

static void standard_defines(Vector<StringRefNull> &sources)
{
  BLI_assert(sources.is_empty());
  /* Version and specialization constants needs to be first.
   * Exact values will be added by implementation. */
  sources.append("version");
  sources.append("/* specialization_constants */");
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
  else if (GPU_type_matches(GPU_DEVICE_APPLE, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    sources.append("#define GPU_APPLE\n");
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
  /* API Definition */
  eGPUBackendType backend = GPU_backend_get_type();
  switch (backend) {
    case GPU_BACKEND_OPENGL:
      sources.append("#define GPU_OPENGL\n");
      break;
    case GPU_BACKEND_METAL:
      sources.append("#define GPU_METAL\n");
      break;
    case GPU_BACKEND_VULKAN:
      sources.append("#define GPU_VULKAN\n");
      break;
    default:
      BLI_assert_msg(false, "Invalid GPU Backend Type");
      break;
  }

  if (GPU_crappy_amd_driver()) {
    sources.append("#define GPU_DEPRECATED_AMD_DRIVER\n");
  }
}

GPUShader *GPU_shader_create_ex(const std::optional<StringRefNull> vertcode,
                                const std::optional<StringRefNull> fragcode,
                                const std::optional<StringRefNull> geomcode,
                                const std::optional<StringRefNull> computecode,
                                const std::optional<StringRefNull> libcode,
                                const std::optional<StringRefNull> defines,
                                const eGPUShaderTFBType tf_type,
                                const char **tf_names,
                                const int tf_count,
                                const StringRefNull shname)
{
  /* At least a vertex shader and a fragment shader are required, or only a compute shader. */
  BLI_assert((fragcode.has_value() && vertcode.has_value() && !computecode.has_value()) ||
             (!fragcode.has_value() && !vertcode.has_value() && !geomcode.has_value() &&
              computecode.has_value()));

  Shader *shader = GPUBackend::get()->shader_alloc(shname.c_str());

  if (vertcode) {
    Vector<StringRefNull> sources;
    standard_defines(sources);
    sources.append("#define GPU_VERTEX_SHADER\n");
    sources.append("#define IN_OUT out\n");
    if (geomcode) {
      sources.append("#define USE_GEOMETRY_SHADER\n");
    }
    if (defines) {
      sources.append(*defines);
    }
    sources.append(*vertcode);

    shader->vertex_shader_from_glsl(sources);
  }

  if (fragcode) {
    Vector<StringRefNull> sources;
    standard_defines(sources);
    sources.append("#define GPU_FRAGMENT_SHADER\n");
    sources.append("#define IN_OUT in\n");
    if (geomcode) {
      sources.append("#define USE_GEOMETRY_SHADER\n");
    }
    if (defines) {
      sources.append(*defines);
    }
    if (libcode) {
      sources.append(*libcode);
    }
    sources.append(*fragcode);

    shader->fragment_shader_from_glsl(sources);
  }

  if (geomcode) {
    Vector<StringRefNull> sources;
    standard_defines(sources);
    sources.append("#define GPU_GEOMETRY_SHADER\n");
    if (defines) {
      sources.append(*defines);
    }
    sources.append(*geomcode);

    shader->geometry_shader_from_glsl(sources);
  }

  if (computecode) {
    Vector<StringRefNull> sources;
    standard_defines(sources);
    sources.append("#define GPU_COMPUTE_SHADER\n");
    if (defines) {
      sources.append(*defines);
    }
    if (libcode) {
      sources.append(*libcode);
    }
    sources.append(*computecode);

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

GPUShader *GPU_shader_create(const std::optional<StringRefNull> vertcode,
                             const std::optional<StringRefNull> fragcode,
                             const std::optional<StringRefNull> geomcode,
                             const std::optional<StringRefNull> libcode,
                             const std::optional<StringRefNull> defines,
                             const StringRefNull shname)
{
  return GPU_shader_create_ex(vertcode,
                              fragcode,
                              geomcode,
                              std::nullopt,
                              libcode,
                              defines,
                              GPU_SHADER_TFB_NONE,
                              nullptr,
                              0,
                              shname);
}

GPUShader *GPU_shader_create_compute(const std::optional<StringRefNull> computecode,
                                     const std::optional<StringRefNull> libcode,
                                     const std::optional<StringRefNull> defines,
                                     const StringRefNull shname)
{
  return GPU_shader_create_ex(std::nullopt,
                              std::nullopt,
                              std::nullopt,
                              computecode,
                              libcode,
                              defines,
                              GPU_SHADER_TFB_NONE,
                              nullptr,
                              0,
                              shname);
}

const GPUShaderCreateInfo *GPU_shader_create_info_get(const char *info_name)
{
  return gpu_shader_create_info_get(info_name);
}

void GPU_shader_create_info_get_unfinalized_copy(const char *info_name,
                                                 GPUShaderCreateInfo &r_info)
{
  gpu_shader_create_info_get_unfinalized_copy(info_name, r_info);
}

bool GPU_shader_create_info_check_error(const GPUShaderCreateInfo *_info, char r_error[128])
{
  using namespace blender::gpu::shader;
  const ShaderCreateInfo &info = *reinterpret_cast<const ShaderCreateInfo *>(_info);
  std::string error = info.check_error();
  if (error.length() == 0) {
    return true;
  }

  BLI_strncpy(r_error, error.c_str(), 128);
  return false;
}

GPUShader *GPU_shader_create_from_info_name(const char *info_name)
{
  using namespace blender::gpu::shader;
  const GPUShaderCreateInfo *_info = gpu_shader_create_info_get(info_name);
  const ShaderCreateInfo &info = *reinterpret_cast<const ShaderCreateInfo *>(_info);
  if (!info.do_static_compilation_) {
    std::cerr << "Warning: Trying to compile \"" << info.name_
              << "\" which was not marked for static compilation.\n";
  }
  return GPU_shader_create_from_info(_info);
}

GPUShader *GPU_shader_create_from_info(const GPUShaderCreateInfo *_info)
{
  using namespace blender::gpu::shader;
  const ShaderCreateInfo &info = *reinterpret_cast<const ShaderCreateInfo *>(_info);
  return wrap(Context::get()->compiler->compile(info, false));
}

static std::string preprocess_source(StringRefNull original)
{
  gpu::shader::Preprocessor processor;
  return processor.process(original);
};

GPUShader *GPU_shader_create_from_info_python(const GPUShaderCreateInfo *_info)
{
  using namespace blender::gpu::shader;
  ShaderCreateInfo &info = *const_cast<ShaderCreateInfo *>(
      reinterpret_cast<const ShaderCreateInfo *>(_info));

  std::string vertex_source_original = info.vertex_source_generated;
  std::string fragment_source_original = info.fragment_source_generated;
  std::string geometry_source_original = info.geometry_source_generated;
  std::string compute_source_original = info.compute_source_generated;

  info.vertex_source_generated = preprocess_source(info.vertex_source_generated);
  info.fragment_source_generated = preprocess_source(info.fragment_source_generated);
  info.geometry_source_generated = preprocess_source(info.geometry_source_generated);
  info.compute_source_generated = preprocess_source(info.compute_source_generated);

  GPUShader *result = wrap(Context::get()->compiler->compile(info, false));

  info.vertex_source_generated = vertex_source_original;
  info.fragment_source_generated = fragment_source_original;
  info.geometry_source_generated = geometry_source_original;
  info.compute_source_generated = compute_source_original;

  return result;
}

GPUShader *GPU_shader_create_from_python(std::optional<StringRefNull> vertcode,
                                         std::optional<StringRefNull> fragcode,
                                         std::optional<StringRefNull> geomcode,
                                         std::optional<StringRefNull> libcode,
                                         const std::optional<StringRefNull> defines,
                                         const std::optional<StringRefNull> name)
{
  std::string libcodecat;

  if (!libcode) {
    libcode = datatoc_gpu_shader_colorspace_lib_glsl;
  }
  else {
    libcodecat = *libcode + datatoc_gpu_shader_colorspace_lib_glsl;
    libcode = libcodecat;
  }

  std::string vertex_source_processed;
  std::string fragment_source_processed;
  std::string geometry_source_processed;
  std::string library_source_processed;

  if (vertcode.has_value()) {
    vertex_source_processed = preprocess_source(*vertcode);
    vertcode = vertex_source_processed;
  }
  if (fragcode.has_value()) {
    fragment_source_processed = preprocess_source(*fragcode);
    fragcode = fragment_source_processed;
  }
  if (geomcode.has_value()) {
    geometry_source_processed = preprocess_source(*geomcode);
    geomcode = geometry_source_processed;
  }
  if (libcode.has_value()) {
    library_source_processed = preprocess_source(*libcode);
    libcode = library_source_processed;
  }

  /* Use pyGPUShader as default name for shader. */
  blender::StringRefNull shname = name.value_or("pyGPUShader");

  GPUShader *sh = GPU_shader_create_ex(vertcode,
                                       fragcode,
                                       geomcode,
                                       std::nullopt,
                                       libcode,
                                       defines,
                                       GPU_SHADER_TFB_NONE,
                                       nullptr,
                                       0,
                                       shname);

  return sh;
}

BatchHandle GPU_shader_batch_create_from_infos(Span<const GPUShaderCreateInfo *> infos)
{
  using namespace blender::gpu::shader;
  Span<const ShaderCreateInfo *> &infos_ = reinterpret_cast<Span<const ShaderCreateInfo *> &>(
      infos);
  return Context::get()->compiler->batch_compile(infos_);
}

bool GPU_shader_batch_is_ready(BatchHandle handle)
{
  return Context::get()->compiler->batch_is_ready(handle);
}

Vector<GPUShader *> GPU_shader_batch_finalize(BatchHandle &handle)
{
  Vector<Shader *> result = Context::get()->compiler->batch_finalize(handle);
  return reinterpret_cast<Vector<GPUShader *> &>(result);
}

void GPU_shader_compile_static()
{
  printf("Compiling all static GPU shaders. This process takes a while.\n");
  gpu_shader_create_info_compile("");
}

void GPU_shader_cache_dir_clear_old()
{
  GPUBackend::get()->shader_cache_dir_clear_old();
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
    Shader::set_srgb_uniform(gpu_shader);
    shader->constants.is_dirty = false;
  }
  else {
    if (shader->constants.is_dirty) {
      shader->bind();
      shader->constants.is_dirty = false;
    }
    if (Shader::srgb_uniform_dirty_get()) {
      Shader::set_srgb_uniform(gpu_shader);
    }
    if (GPU_matrix_dirty_get()) {
      GPU_matrix_bind(gpu_shader);
    }
  }
#if GPU_SHADER_PRINTF_ENABLE
  if (ctx->printf_buf) {
    GPU_storagebuf_bind(ctx->printf_buf, GPU_SHADER_PRINTF_SLOT);
  }
#endif
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

GPUShader *GPU_shader_get_bound()
{
  Context *ctx = Context::get();
  if (ctx) {
    return wrap(ctx->shader);
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader name
 * \{ */

const char *GPU_shader_get_name(GPUShader *shader)
{
  return unwrap(shader)->name_get().c_str();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader cache warming
 * \{ */

void GPU_shader_set_parent(GPUShader *shader, GPUShader *parent)
{
  BLI_assert(shader != nullptr);
  BLI_assert(shader != parent);
  if (shader != parent) {
    Shader *shd_child = unwrap(shader);
    Shader *shd_parent = unwrap(parent);
    shd_child->parent_set(shd_parent);
  }
}

void GPU_shader_warm_cache(GPUShader *shader, int limit)
{
  unwrap(shader)->warm_cache(limit);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform feedback
 *
 * TODO(fclem): Should be replaced by compute shaders.
 * \{ */

bool GPU_shader_transform_feedback_enable(GPUShader *shader, blender::gpu::VertBuf *vertbuf)
{
  return unwrap(shader)->transform_feedback_enable(vertbuf);
}

void GPU_shader_transform_feedback_disable(GPUShader *shader)
{
  unwrap(shader)->transform_feedback_disable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Assign specialization constants.
 * \{ */

void Shader::specialization_constants_init(const shader::ShaderCreateInfo &info)
{
  using namespace shader;
  for (const SpecializationConstant &sc : info.specialization_constants_) {
    constants.types.append(sc.type);
    constants.values.append(sc.value);
  }
  constants.is_dirty = true;
}

void GPU_shader_constant_int_ex(GPUShader *sh, int location, int value)
{
  Shader &shader = *unwrap(sh);
  BLI_assert(shader.constants.types[location] == gpu::shader::Type::INT);
  shader.constants.is_dirty |= assign_if_different(shader.constants.values[location].i, value);
}
void GPU_shader_constant_uint_ex(GPUShader *sh, int location, uint value)
{
  Shader &shader = *unwrap(sh);
  BLI_assert(shader.constants.types[location] == gpu::shader::Type::UINT);
  shader.constants.is_dirty |= assign_if_different(shader.constants.values[location].u, value);
}
void GPU_shader_constant_float_ex(GPUShader *sh, int location, float value)
{
  Shader &shader = *unwrap(sh);
  BLI_assert(shader.constants.types[location] == gpu::shader::Type::FLOAT);
  shader.constants.is_dirty |= assign_if_different(shader.constants.values[location].f, value);
}
void GPU_shader_constant_bool_ex(GPUShader *sh, int location, bool value)
{
  Shader &shader = *unwrap(sh);
  BLI_assert(shader.constants.types[location] == gpu::shader::Type::BOOL);
  shader.constants.is_dirty |= assign_if_different(shader.constants.values[location].u,
                                                   uint32_t(value));
}

void GPU_shader_constant_int(GPUShader *sh, const char *name, int value)
{
  GPU_shader_constant_int_ex(sh, unwrap(sh)->interface->constant_get(name)->location, value);
}
void GPU_shader_constant_uint(GPUShader *sh, const char *name, uint value)
{
  GPU_shader_constant_uint_ex(sh, unwrap(sh)->interface->constant_get(name)->location, value);
}
void GPU_shader_constant_float(GPUShader *sh, const char *name, float value)
{
  GPU_shader_constant_float_ex(sh, unwrap(sh)->interface->constant_get(name)->location, value);
}
void GPU_shader_constant_bool(GPUShader *sh, const char *name, bool value)
{
  GPU_shader_constant_bool_ex(sh, unwrap(sh)->interface->constant_get(name)->location, value);
}

SpecializationBatchHandle GPU_shader_batch_specializations(
    blender::Span<ShaderSpecialization> specializations)
{
  return Context::get()->compiler->precompile_specializations(specializations);
}

bool GPU_shader_batch_specializations_is_ready(SpecializationBatchHandle &handle)
{
  return Context::get()->compiler->specialization_batch_is_ready(handle);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms / Resource location
 * \{ */

int GPU_shader_get_uniform(GPUShader *shader, const char *name)
{
  const ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *uniform = interface->uniform_get(name);
  return uniform ? uniform->location : -1;
}

int GPU_shader_get_constant(GPUShader *shader, const char *name)
{
  const ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *constant = interface->constant_get(name);
  return constant ? constant->location : -1;
}

int GPU_shader_get_builtin_uniform(GPUShader *shader, int builtin)
{
  const ShaderInterface *interface = unwrap(shader)->interface;
  return interface->uniform_builtin((GPUUniformBuiltin)builtin);
}

int GPU_shader_get_builtin_block(GPUShader *shader, int builtin)
{
  const ShaderInterface *interface = unwrap(shader)->interface;
  return interface->ubo_builtin((GPUUniformBlockBuiltin)builtin);
}

int GPU_shader_get_ssbo_binding(GPUShader *shader, const char *name)
{
  const ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *ssbo = interface->ssbo_get(name);
  return ssbo ? ssbo->location : -1;
}

int GPU_shader_get_uniform_block(GPUShader *shader, const char *name)
{
  const ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *ubo = interface->ubo_get(name);
  return ubo ? ubo->location : -1;
}

int GPU_shader_get_ubo_binding(GPUShader *shader, const char *name)
{
  const ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *ubo = interface->ubo_get(name);
  return ubo ? ubo->binding : -1;
}

int GPU_shader_get_sampler_binding(GPUShader *shader, const char *name)
{
  const ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *tex = interface->uniform_get(name);
  return tex ? tex->binding : -1;
}

uint GPU_shader_get_attribute_len(const GPUShader *shader)
{
  const ShaderInterface *interface = unwrap(shader)->interface;
  return interface->attr_len_;
}

int GPU_shader_get_attribute(const GPUShader *shader, const char *name)
{
  const ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *attr = interface->attr_get(name);
  return attr ? attr->location : -1;
}

bool GPU_shader_get_attribute_info(const GPUShader *shader,
                                   int attr_location,
                                   char r_name[256],
                                   int *r_type)
{
  const ShaderInterface *interface = unwrap(shader)->interface;

  const ShaderInput *attr = interface->attr_get(attr_location);
  if (!attr) {
    return false;
  }

  BLI_strncpy(r_name, interface->input_name_get(attr), 256);
  *r_type = attr->location != -1 ? interface->attr_types_[attr->location] : -1;
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Getters
 * \{ */

int GPU_shader_get_program(GPUShader *shader)
{
  return unwrap(shader)->program_handle_get();
}

int GPU_shader_get_ssbo_vertex_fetch_num_verts_per_prim(GPUShader *shader)
{
  return unwrap(shader)->get_ssbo_vertex_fetch_output_num_verts();
}

bool GPU_shader_uses_ssbo_vertex_fetch(GPUShader *shader)
{
  return unwrap(shader)->get_uses_ssbo_vertex_fetch();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms setters
 * \{ */

void GPU_shader_uniform_float_ex(
    GPUShader *shader, int loc, int len, int array_size, const float *value)
{
  unwrap(shader)->uniform_float(loc, len, array_size, value);
}

void GPU_shader_uniform_int_ex(
    GPUShader *shader, int loc, int len, int array_size, const int *value)
{
  unwrap(shader)->uniform_int(loc, len, array_size, value);
}

void GPU_shader_uniform_1i(GPUShader *sh, const char *name, int value)
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_int_ex(sh, loc, 1, 1, &value);
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
  GPU_shader_uniform_float_ex(sh, loc, 1, 1, &value);
}

void GPU_shader_uniform_2fv(GPUShader *sh, const char *name, const float data[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 2, 1, data);
}

void GPU_shader_uniform_3fv(GPUShader *sh, const char *name, const float data[3])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 3, 1, data);
}

void GPU_shader_uniform_4fv(GPUShader *sh, const char *name, const float data[4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 4, 1, data);
}

void GPU_shader_uniform_2iv(GPUShader *sh, const char *name, const int data[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_int_ex(sh, loc, 2, 1, data);
}

void GPU_shader_uniform_mat4(GPUShader *sh, const char *name, const float data[4][4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 16, 1, (const float *)data);
}

void GPU_shader_uniform_mat3_as_mat4(GPUShader *sh, const char *name, const float data[3][3])
{
  float matrix[4][4];
  copy_m4_m3(matrix, data);
  GPU_shader_uniform_mat4(sh, name, matrix);
}

void GPU_shader_uniform_1f_array(GPUShader *sh, const char *name, int len, const float *val)
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 1, len, val);
}

void GPU_shader_uniform_2fv_array(GPUShader *sh, const char *name, int len, const float (*val)[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 2, len, (const float *)val);
}

void GPU_shader_uniform_4fv_array(GPUShader *sh, const char *name, int len, const float (*val)[4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 4, len, (const float *)val);
}

/** \} */

namespace blender::gpu {

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

bool Shader::srgb_uniform_dirty_get()
{
  return g_shader_builtin_srgb_is_dirty;
}

void Shader::set_srgb_uniform(GPUShader *shader)
{
  int32_t loc = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_SRGB_TRANSFORM);
  if (loc != -1) {
    GPU_shader_uniform_int_ex(shader, loc, 1, 1, &g_shader_builtin_srgb_transform);
  }
  g_shader_builtin_srgb_is_dirty = false;
}

void Shader::set_framebuffer_srgb_target(int use_srgb_to_linear)
{
  if (g_shader_builtin_srgb_transform != use_srgb_to_linear) {
    g_shader_builtin_srgb_transform = use_srgb_to_linear;
    g_shader_builtin_srgb_is_dirty = true;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShaderCompiler
 * \{ */

Shader *ShaderCompiler::compile(const shader::ShaderCreateInfo &info, bool is_batch_compilation)
{
  using namespace blender::gpu::shader;
  const_cast<ShaderCreateInfo &>(info).finalize();

  GPU_debug_group_begin(GPU_DEBUG_SHADER_COMPILATION_GROUP);

  const std::string error = info.check_error();
  if (!error.empty()) {
    std::cerr << error << "\n";
    BLI_assert(false);
  }

  Shader *shader = GPUBackend::get()->shader_alloc(info.name_.c_str());
  shader->init(info, is_batch_compilation);
  shader->specialization_constants_init(info);

  std::string defines = shader->defines_declare(info);
  std::string resources = shader->resources_declare(info);

  if (info.legacy_resource_location_ == false) {
    defines += "#define USE_GPU_SHADER_CREATE_INFO\n";
  }

  Vector<StringRefNull> typedefs;
  if (!info.typedef_sources_.is_empty() || !info.typedef_source_generated.empty()) {
    typedefs.append(gpu_shader_dependency_get_source("GPU_shader_shared_utils.hh").c_str());
  }
  if (!info.typedef_source_generated.empty()) {
    typedefs.append(info.typedef_source_generated);
  }
  for (auto filename : info.typedef_sources_) {
    typedefs.append(gpu_shader_dependency_get_source(filename));
  }

  if (!info.vertex_source_.is_empty()) {
    auto code = gpu_shader_dependency_get_resolved_source(info.vertex_source_);
    std::string interface = shader->vertex_interface_declare(info);

    Vector<StringRefNull> sources;
    standard_defines(sources);
    sources.append("#define GPU_VERTEX_SHADER\n");
    if (!info.geometry_source_.is_empty()) {
      sources.append("#define USE_GEOMETRY_SHADER\n");
    }
    sources.append(defines);
    sources.extend(typedefs);
    sources.append(resources);
    sources.append(interface);
    sources.extend(code);
    sources.extend(info.dependencies_generated);
    sources.append(info.vertex_source_generated);

    shader->vertex_shader_from_glsl(sources);
  }

  if (!info.fragment_source_.is_empty()) {
    auto code = gpu_shader_dependency_get_resolved_source(info.fragment_source_);
    std::string interface = shader->fragment_interface_declare(info);

    Vector<StringRefNull> sources;
    standard_defines(sources);
    sources.append("#define GPU_FRAGMENT_SHADER\n");
    if (!info.geometry_source_.is_empty()) {
      sources.append("#define USE_GEOMETRY_SHADER\n");
    }
    sources.append(defines);
    sources.extend(typedefs);
    sources.append(resources);
    sources.append(interface);
    sources.extend(code);
    sources.extend(info.dependencies_generated);
    sources.append(info.fragment_source_generated);

    shader->fragment_shader_from_glsl(sources);
  }

  if (!info.geometry_source_.is_empty()) {
    auto code = gpu_shader_dependency_get_resolved_source(info.geometry_source_);
    std::string layout = shader->geometry_layout_declare(info);
    std::string interface = shader->geometry_interface_declare(info);

    Vector<StringRefNull> sources;
    standard_defines(sources);
    sources.append("#define GPU_GEOMETRY_SHADER\n");
    sources.append(defines);
    sources.extend(typedefs);
    sources.append(resources);
    sources.append(layout);
    sources.append(interface);
    sources.append(info.geometry_source_generated);
    sources.extend(code);

    shader->geometry_shader_from_glsl(sources);
  }

  if (!info.compute_source_.is_empty()) {
    auto code = gpu_shader_dependency_get_resolved_source(info.compute_source_);
    std::string layout = shader->compute_layout_declare(info);

    Vector<StringRefNull> sources;
    standard_defines(sources);
    sources.append("#define GPU_COMPUTE_SHADER\n");
    sources.append(defines);
    sources.extend(typedefs);
    sources.append(resources);
    sources.append(layout);
    sources.extend(code);
    sources.extend(info.dependencies_generated);
    sources.append(info.compute_source_generated);

    shader->compute_shader_from_glsl(sources);
  }

  if (info.tf_type_ != GPU_SHADER_TFB_NONE && info.tf_names_.size() > 0) {
    shader->transform_feedback_names_set(info.tf_names_.as_span(), info.tf_type_);
  }

  if (!shader->finalize(&info)) {
    delete shader;
    GPU_debug_group_end();
    return nullptr;
  }

  GPU_debug_group_end();
  return shader;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShaderCompilerGeneric
 * \{ */

ShaderCompilerGeneric::~ShaderCompilerGeneric()
{
  /* Ensure all the requested batches have been retrieved. */
  BLI_assert(batches.is_empty());
}

BatchHandle ShaderCompilerGeneric::batch_compile(Span<const shader::ShaderCreateInfo *> &infos)
{
  BatchHandle handle = next_batch_handle++;
  batches.add(handle, {{}, infos, true});
  Batch &batch = batches.lookup(handle);
  batch.shaders.reserve(infos.size());
  for (const shader::ShaderCreateInfo *info : infos) {
    batch.shaders.append(compile(*info, true));
  }
  return handle;
}

bool ShaderCompilerGeneric::batch_is_ready(BatchHandle handle)
{
  bool is_ready = batches.lookup(handle).is_ready;
  return is_ready;
}

Vector<Shader *> ShaderCompilerGeneric::batch_finalize(BatchHandle &handle)
{
  Vector<Shader *> shaders = batches.pop(handle).shaders;
  handle = 0;
  return shaders;
}

/** \} */

}  // namespace blender::gpu

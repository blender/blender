/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_colorspace.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_string.h"

#include "GPU_capabilities.hh"
#include "GPU_debug.hh"
#include "GPU_matrix.hh"
#include "GPU_platform.hh"

#include "glsl_preprocess/glsl_preprocess.hh"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_profile_report.hh"
#include "gpu_shader_create_info.hh"
#include "gpu_shader_create_info_private.hh"
#include "gpu_shader_dependency_private.hh"
#include "gpu_shader_private.hh"

#include <string>

extern "C" char datatoc_gpu_shader_colorspace_lib_glsl[];

namespace blender::gpu {

std::string Shader::defines_declare(const shader::ShaderCreateInfo &info)
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
  BLI_assert_msg(Context::get() == nullptr || Context::get()->shader != this,
                 "Shader must be unbound from context before being freed");
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
  GPUBackendType backend = GPU_backend_get_type();
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
}

void GPU_shader_free(blender::gpu::Shader *shader)
{
  delete shader;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation utils
 * \{ */

const GPUShaderCreateInfo *GPU_shader_create_info_get(const char *info_name)
{
  return gpu_shader_create_info_get(info_name);
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

blender::gpu::Shader *GPU_shader_create_from_info_name(const char *info_name)
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

blender::gpu::Shader *GPU_shader_create_from_info(const GPUShaderCreateInfo *_info)
{
  using namespace blender::gpu::shader;
  const ShaderCreateInfo &info = *reinterpret_cast<const ShaderCreateInfo *>(_info);
  return GPUBackend::get()->get_compiler()->compile(info, false);
}

std::string GPU_shader_preprocess_source(StringRefNull original)
{
  if (original.is_empty()) {
    return original;
  }
  gpu::shader::Preprocessor processor;
  return processor.process(original);
};

blender::gpu::Shader *GPU_shader_create_from_info_python(const GPUShaderCreateInfo *_info)
{
  using namespace blender::gpu::shader;
  ShaderCreateInfo &info = *const_cast<ShaderCreateInfo *>(
      reinterpret_cast<const ShaderCreateInfo *>(_info));

  std::string vertex_source_original = info.vertex_source_generated;
  std::string fragment_source_original = info.fragment_source_generated;
  std::string geometry_source_original = info.geometry_source_generated;
  std::string compute_source_original = info.compute_source_generated;

  info.vertex_source_generated = GPU_shader_preprocess_source(info.vertex_source_generated);
  info.fragment_source_generated = GPU_shader_preprocess_source(info.fragment_source_generated);
  info.geometry_source_generated = GPU_shader_preprocess_source(info.geometry_source_generated);
  info.compute_source_generated = GPU_shader_preprocess_source(info.compute_source_generated);

  blender::gpu::Shader *result = GPUBackend::get()->get_compiler()->compile(info, false);

  info.vertex_source_generated = vertex_source_original;
  info.fragment_source_generated = fragment_source_original;
  info.geometry_source_generated = geometry_source_original;
  info.compute_source_generated = compute_source_original;

  return result;
}

BatchHandle GPU_shader_batch_create_from_infos(Span<const GPUShaderCreateInfo *> infos,
                                               CompilationPriority priority)
{
  using namespace blender::gpu::shader;
  Span<const ShaderCreateInfo *> &infos_ = reinterpret_cast<Span<const ShaderCreateInfo *> &>(
      infos);
  return GPUBackend::get()->get_compiler()->batch_compile(infos_, priority);
}

bool GPU_shader_batch_is_ready(BatchHandle handle)
{
  return GPUBackend::get()->get_compiler()->batch_is_ready(handle);
}

Vector<blender::gpu::Shader *> GPU_shader_batch_finalize(BatchHandle &handle)
{
  Vector<Shader *> result = GPUBackend::get()->get_compiler()->batch_finalize(handle);
  return reinterpret_cast<Vector<blender::gpu::Shader *> &>(result);
}

void GPU_shader_batch_cancel(BatchHandle &handle)
{
  GPUBackend::get()->get_compiler()->batch_cancel(handle);
}

bool GPU_shader_batch_is_compiling()
{
  return GPUBackend::get()->get_compiler()->is_compiling();
}

void GPU_shader_batch_wait_for_all()
{
  GPUBackend::get()->get_compiler()->wait_for_all();
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

void GPU_shader_bind(blender::gpu::Shader *gpu_shader,
                     const shader::SpecializationConstants *constants_state)
{
  Shader *shader = gpu_shader;

  BLI_assert_msg(constants_state != nullptr || shader->constants->is_empty(),
                 "Shader requires specialization constants but none was passed");

  Context *ctx = Context::get();

  if (ctx->shader != shader) {
    ctx->shader = shader;
    shader->bind(constants_state);
    GPU_matrix_bind(gpu_shader);
    Shader::set_srgb_uniform(ctx, gpu_shader);
    /* Blender working color space do not change during the drawing of the frame.
     * So we can just set the uniform once. */
    Shader::set_scene_linear_to_xyz_uniform(gpu_shader);
  }
  else {
    if (constants_state) {
      shader->bind(constants_state);
    }
    if (ctx->shader_builtin_srgb_is_dirty) {
      Shader::set_srgb_uniform(ctx, gpu_shader);
    }
    if (GPU_matrix_dirty_get()) {
      GPU_matrix_bind(gpu_shader);
    }
  }
#if GPU_SHADER_PRINTF_ENABLE
  if (!ctx->printf_buf.is_empty()) {
    GPU_storagebuf_bind(ctx->printf_buf.last(), GPU_SHADER_PRINTF_SLOT);
  }
#endif
}

void GPU_shader_unbind()
{
  Context *ctx = Context::get();
  if (ctx == nullptr) {
    return;
  }
#ifndef NDEBUG
  if (ctx->shader) {
    ctx->shader->unbind();
  }
#endif
  ctx->shader = nullptr;
}

blender::gpu::Shader *GPU_shader_get_bound()
{
  Context *ctx = Context::get();
  if (ctx) {
    return ctx->shader;
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader name
 * \{ */

const char *GPU_shader_get_name(blender::gpu::Shader *shader)
{
  return shader->name_get().c_str();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader cache warming
 * \{ */

void GPU_shader_set_parent(blender::gpu::Shader *shader, blender::gpu::Shader *parent)
{
  BLI_assert(shader != nullptr);
  BLI_assert(shader != parent);
  if (shader != parent) {
    Shader *shd_child = shader;
    Shader *shd_parent = parent;
    shd_child->parent_set(shd_parent);
  }
}

void GPU_shader_warm_cache(blender::gpu::Shader *shader, int limit)
{
  shader->warm_cache(limit);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Assign specialization constants.
 * \{ */

const shader::SpecializationConstants &GPU_shader_get_default_constant_state(
    blender::gpu::Shader *sh)
{
  return *sh->constants;
}

void Shader::specialization_constants_init(const shader::ShaderCreateInfo &info)
{
  using namespace shader;
  shader::SpecializationConstants constants_tmp;
  for (const SpecializationConstant &sc : info.specialization_constants_) {
    constants_tmp.types.append(sc.type);
    constants_tmp.values.append(sc.value);
  }
  constants = std::make_unique<const shader::SpecializationConstants>(std::move(constants_tmp));
}

SpecializationBatchHandle GPU_shader_batch_specializations(
    blender::Span<ShaderSpecialization> specializations, CompilationPriority priority)
{
  return GPUBackend::get()->get_compiler()->precompile_specializations(specializations, priority);
}

bool GPU_shader_batch_specializations_is_ready(SpecializationBatchHandle &handle)
{
  return GPUBackend::get()->get_compiler()->specialization_batch_is_ready(handle);
}

void GPU_shader_batch_specializations_cancel(SpecializationBatchHandle &handle)
{
  GPUBackend::get()->get_compiler()->batch_cancel(handle);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms / Resource location
 * \{ */

int GPU_shader_get_uniform(blender::gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *uniform = interface->uniform_get(name);
  return uniform ? uniform->location : -1;
}

int GPU_shader_get_constant(blender::gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *constant = interface->constant_get(name);
  return constant ? constant->location : -1;
}

int GPU_shader_get_builtin_uniform(blender::gpu::Shader *shader, int builtin)
{
  const ShaderInterface *interface = shader->interface;
  return interface->uniform_builtin((GPUUniformBuiltin)builtin);
}

int GPU_shader_get_ssbo_binding(blender::gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *ssbo = interface->ssbo_get(name);
  return ssbo ? ssbo->location : -1;
}

int GPU_shader_get_uniform_block(blender::gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *ubo = interface->ubo_get(name);
  return ubo ? ubo->location : -1;
}

int GPU_shader_get_ubo_binding(blender::gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *ubo = interface->ubo_get(name);
  return ubo ? ubo->binding : -1;
}

int GPU_shader_get_sampler_binding(blender::gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *tex = interface->uniform_get(name);
  return tex ? tex->binding : -1;
}

uint GPU_shader_get_attribute_len(const blender::gpu::Shader *shader)
{
  const ShaderInterface *interface = shader->interface;
  return interface->valid_bindings_get(interface->inputs_, interface->attr_len_);
}

uint GPU_shader_get_ssbo_input_len(const blender::gpu::Shader *shader)
{
  const ShaderInterface *interface = shader->interface;
  return interface->ssbo_len_;
}

int GPU_shader_get_attribute(const blender::gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *attr = interface->attr_get(name);
  return attr ? attr->location : -1;
}

bool GPU_shader_get_attribute_info(const blender::gpu::Shader *shader,
                                   int attr_location,
                                   char r_name[256],
                                   int *r_type)
{
  const ShaderInterface *interface = shader->interface;

  const ShaderInput *attr = interface->attr_get(attr_location);
  if (!attr) {
    return false;
  }

  BLI_strncpy(r_name, interface->input_name_get(attr), 256);
  *r_type = attr->location != -1 ? interface->attr_types_[attr->location] : -1;
  return true;
}

bool GPU_shader_get_ssbo_input_info(const blender::gpu::Shader *shader,
                                    int ssbo_location,
                                    char r_name[256])
{
  const ShaderInterface *interface = shader->interface;

  const ShaderInput *ssbo_input = interface->ssbo_get(ssbo_location);
  if (!ssbo_input) {
    return false;
  }

  BLI_strncpy(r_name, interface->input_name_get(ssbo_input), 256);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms setters
 * \{ */

void GPU_shader_uniform_float_ex(
    blender::gpu::Shader *shader, int loc, int len, int array_size, const float *value)
{
  shader->uniform_float(loc, len, array_size, value);
}

void GPU_shader_uniform_int_ex(
    blender::gpu::Shader *shader, int loc, int len, int array_size, const int *value)
{
  shader->uniform_int(loc, len, array_size, value);
}

void GPU_shader_uniform_1i(blender::gpu::Shader *sh, const char *name, int value)
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_int_ex(sh, loc, 1, 1, &value);
}

void GPU_shader_uniform_1b(blender::gpu::Shader *sh, const char *name, bool value)
{
  GPU_shader_uniform_1i(sh, name, value ? 1 : 0);
}

void GPU_shader_uniform_2f(blender::gpu::Shader *sh, const char *name, float x, float y)
{
  const float data[2] = {x, y};
  GPU_shader_uniform_2fv(sh, name, data);
}

void GPU_shader_uniform_3f(blender::gpu::Shader *sh, const char *name, float x, float y, float z)
{
  const float data[3] = {x, y, z};
  GPU_shader_uniform_3fv(sh, name, data);
}

void GPU_shader_uniform_4f(
    blender::gpu::Shader *sh, const char *name, float x, float y, float z, float w)
{
  const float data[4] = {x, y, z, w};
  GPU_shader_uniform_4fv(sh, name, data);
}

void GPU_shader_uniform_1f(blender::gpu::Shader *sh, const char *name, float value)
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 1, 1, &value);
}

void GPU_shader_uniform_2fv(blender::gpu::Shader *sh, const char *name, const float data[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 2, 1, data);
}

void GPU_shader_uniform_3fv(blender::gpu::Shader *sh, const char *name, const float data[3])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 3, 1, data);
}

void GPU_shader_uniform_4fv(blender::gpu::Shader *sh, const char *name, const float data[4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 4, 1, data);
}

void GPU_shader_uniform_2iv(blender::gpu::Shader *sh, const char *name, const int data[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_int_ex(sh, loc, 2, 1, data);
}

void GPU_shader_uniform_3iv(blender::gpu::Shader *sh, const char *name, const int data[3])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_int_ex(sh, loc, 3, 1, data);
}

void GPU_shader_uniform_mat4(blender::gpu::Shader *sh, const char *name, const float data[4][4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 16, 1, (const float *)data);
}

void GPU_shader_uniform_mat3_as_mat4(blender::gpu::Shader *sh,
                                     const char *name,
                                     const float data[3][3])
{
  float matrix[4][4];
  copy_m4_m3(matrix, data);
  GPU_shader_uniform_mat4(sh, name, matrix);
}

void GPU_shader_uniform_1f_array(blender::gpu::Shader *sh,
                                 const char *name,
                                 int len,
                                 const float *val)
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 1, len, val);
}

void GPU_shader_uniform_2fv_array(blender::gpu::Shader *sh,
                                  const char *name,
                                  int len,
                                  const float (*val)[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 2, len, (const float *)val);
}

void GPU_shader_uniform_4fv_array(blender::gpu::Shader *sh,
                                  const char *name,
                                  int len,
                                  const float (*val)[4])
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

void Shader::set_srgb_uniform(Context *ctx, blender::gpu::Shader *shader)
{
  int32_t loc = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_SRGB_TRANSFORM);
  if (loc != -1) {
    GPU_shader_uniform_int_ex(shader, loc, 1, 1, &ctx->shader_builtin_srgb_transform);
  }
  ctx->shader_builtin_srgb_is_dirty = false;
}

void Shader::set_scene_linear_to_xyz_uniform(blender::gpu::Shader *shader)
{
  int32_t loc = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_SCENE_LINEAR_XFORM);
  if (loc != -1) {
    GPU_shader_uniform_float_ex(
        shader, loc, 9, 1, blender::colorspace::scene_linear_to_rec709.ptr()[0]);
  }
}

void Shader::set_framebuffer_srgb_target(int use_srgb_to_linear)
{
  Context *ctx = Context::get();
  if (ctx->shader_builtin_srgb_transform != use_srgb_to_linear) {
    ctx->shader_builtin_srgb_transform = use_srgb_to_linear;
    ctx->shader_builtin_srgb_is_dirty = true;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShaderCompiler
 * \{ */

Shader *ShaderCompiler::compile(const shader::ShaderCreateInfo &orig_info,
                                bool is_batch_compilation)
{
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  using namespace blender::gpu::shader;
  const_cast<ShaderCreateInfo &>(orig_info).finalize();
  BLI_assert(orig_info.do_static_compilation_ || orig_info.is_generated_);

  TimePoint start_time;

  if (Context::get()) {
    /* Context can be null in Vulkan compilation threads. */
    GPU_debug_group_begin(GPU_DEBUG_SHADER_COMPILATION_GROUP);
    GPU_debug_group_begin(orig_info.name_.c_str());
  }
  else if (G.profile_gpu) {
    start_time = Clock::now();
  }

  const std::string error = orig_info.check_error();
  if (!error.empty()) {
    std::cerr << error << "\n";
    BLI_assert(false);
  }

  Shader *shader = GPUBackend::get()->shader_alloc(orig_info.name_.c_str());

  const shader::ShaderCreateInfo &info = shader->patch_create_info(orig_info);

  /* Needs to be called before init as GL uses the default specialization constants state to insert
   * default shader inside a map. */
  shader->specialization_constants_init(info);
  shader->init(info, is_batch_compilation);

  shader->fragment_output_bits = 0;
  for (const shader::ShaderCreateInfo::FragOut &frag_out : info.fragment_outputs_) {
    shader->fragment_output_bits |= 1u << frag_out.index;
  }

  std::string defines = shader->defines_declare(info);
  std::string resources = shader->resources_declare(info);

  defines += info.resource_guard_defines();

  defines += "#define USE_GPU_SHADER_CREATE_INFO\n";

  Vector<StringRefNull> typedefs;
  if (!info.typedef_sources_.is_empty() || !info.typedef_source_generated.empty()) {
    typedefs.append(gpu_shader_dependency_get_source("GPU_shader_shared_utils.hh").c_str());
  }
  if (!info.typedef_source_generated.empty()) {
    typedefs.append(info.typedef_source_generated);
  }
  for (auto filename : info.typedef_sources_) {
    typedefs.extend_non_duplicates(
        gpu_shader_dependency_get_resolved_source(filename, info.generated_sources, info.name_));
  }

  if (!info.vertex_source_.is_empty()) {
    Vector<StringRefNull> code = gpu_shader_dependency_get_resolved_source(
        info.vertex_source_, info.generated_sources, info.name_);
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
    sources.append(info.vertex_source_generated);

    if (info.vertex_entry_fn_ != "main") {
      sources.append("void main() { ");
      sources.append(info.vertex_entry_fn_);
      sources.append("(); }\n");
    }

    shader->vertex_shader_from_glsl(info, sources);
  }

  if (!info.fragment_source_.is_empty()) {
    Vector<StringRefNull> code = gpu_shader_dependency_get_resolved_source(
        info.fragment_source_, info.generated_sources, info.name_);
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
    sources.append(info.fragment_source_generated);

    if (info.fragment_entry_fn_ != "main") {
      sources.append("void main() { ");
      sources.append(info.fragment_entry_fn_);
      sources.append("(); }\n");
    }

    shader->fragment_shader_from_glsl(info, sources);
  }

  if (!info.geometry_source_.is_empty()) {
    Vector<StringRefNull> code = gpu_shader_dependency_get_resolved_source(
        info.geometry_source_, info.generated_sources, info.name_);
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

    if (info.geometry_entry_fn_ != "main") {
      sources.append("void main() { ");
      sources.append(info.geometry_entry_fn_);
      sources.append("(); }\n");
    }

    shader->geometry_shader_from_glsl(info, sources);
  }

  if (!info.compute_source_.is_empty()) {
    Vector<StringRefNull> code = gpu_shader_dependency_get_resolved_source(
        info.compute_source_, info.generated_sources, info.name_);
    std::string layout = shader->compute_layout_declare(info);

    Vector<StringRefNull> sources;
    standard_defines(sources);
    sources.append("#define GPU_COMPUTE_SHADER\n");
    sources.append(defines);
    sources.append(layout);
    sources.extend(typedefs);
    sources.append(resources);
    sources.extend(code);
    sources.append(info.compute_source_generated);

    if (info.compute_entry_fn_ != "main") {
      sources.append("void main() { ");
      sources.append(info.compute_entry_fn_);
      sources.append("(); }\n");
    }

    shader->compute_shader_from_glsl(info, sources);
  }

  if (!shader->finalize(&info)) {
    delete shader;
    shader = nullptr;
  }

  if (Context::get()) {
    /* Context can be null in Vulkan compilation threads. */
    GPU_debug_group_end();
    GPU_debug_group_end();
  }
  else if (G.profile_gpu) {
    TimePoint end_time = Clock::now();
    /* Note: Used by the vulkan backend. Use the same time_since_epoch as process_frame_timings. */
    ProfileReport::get().add_group_cpu(GPU_DEBUG_SHADER_COMPILATION_GROUP,
                                       start_time.time_since_epoch().count(),
                                       end_time.time_since_epoch().count());
    ProfileReport::get().add_group_cpu(info.name_.c_str(),
                                       start_time.time_since_epoch().count(),
                                       end_time.time_since_epoch().count());
  }

  return shader;
}

ShaderCompiler::ShaderCompiler(uint32_t threads_count,
                               GPUWorker::ContextType context_type,
                               bool support_specializations)
{
  support_specializations_ = support_specializations;

  if (!GPU_use_main_context_workaround()) {
    compilation_worker_ = std::make_unique<GPUWorker>(
        threads_count,
        context_type,
        mutex_,
        [this]() -> void * { return this->pop_work(); },
        [this](void *work) { this->do_work(work); });
  }
}

ShaderCompiler::~ShaderCompiler()
{
  compilation_worker_.reset();

  /* Ensure all the requested batches have been retrieved. */
  BLI_assert(batches_.is_empty());
}

Shader *ShaderCompiler::compile_shader(const shader::ShaderCreateInfo &info)
{
  return compile(info, false);
}

BatchHandle ShaderCompiler::batch_compile(Span<const shader::ShaderCreateInfo *> &infos,
                                          CompilationPriority priority)
{
  std::unique_lock lock(mutex_);

  Batch *batch = MEM_new<Batch>(__func__);
  batch->infos = infos;
  batch->shaders.reserve(infos.size());

  BatchHandle handle = next_batch_handle_++;
  batches_.add(handle, batch);

  if (compilation_worker_) {
    batch->shaders.resize(infos.size(), nullptr);
    batch->pending_compilations = infos.size();
    for (int i : infos.index_range()) {
      compilation_queue_.push({batch, i}, priority);
      compilation_worker_->wake_up();
    }
  }
  else {
    for (const shader::ShaderCreateInfo *info : infos) {
      batch->shaders.append(compile(*info, false));
    }
  }

  return handle;
}

void ShaderCompiler::batch_cancel(BatchHandle &handle)
{
  std::unique_lock lock(mutex_);

  Batch *batch = batches_.pop(handle);
  compilation_queue_.remove_batch(batch);

  /* If it was already being compiled, wait until it's ready so the calling thread can safely
   * delete the ShaderCreateInfos. */
  compilation_finished_notification_.wait(lock, [&]() { return batch->is_ready(); });
  batch->free_shaders();
  MEM_delete(batch);

  handle = 0;
}

bool ShaderCompiler::batch_is_ready(BatchHandle handle)
{
  std::lock_guard lock(mutex_);

  return batches_.lookup(handle)->is_ready();
}

Vector<Shader *> ShaderCompiler::batch_finalize(BatchHandle &handle)
{
  std::unique_lock lock(mutex_);
  /* TODO: Move to be first on the queue. */
  compilation_finished_notification_.wait(lock,
                                          [&]() { return batches_.lookup(handle)->is_ready(); });

  Batch *batch = batches_.pop(handle);
  Vector<Shader *> shaders = std::move(batch->shaders);
  MEM_delete(batch);
  handle = 0;

  return shaders;
}

SpecializationBatchHandle ShaderCompiler::precompile_specializations(
    Span<ShaderSpecialization> specializations, CompilationPriority priority)
{
  if (!compilation_worker_ || !support_specializations_) {
    return 0;
  }

  std::lock_guard lock(mutex_);

  Batch *batch = MEM_new<Batch>(__func__);
  batch->specializations = specializations;

  BatchHandle handle = next_batch_handle_++;
  batches_.add(handle, batch);

  batch->pending_compilations = specializations.size();
  for (int i : specializations.index_range()) {
    compilation_queue_.push({batch, i}, priority);
    compilation_worker_->wake_up();
  }

  return handle;
}

bool ShaderCompiler::specialization_batch_is_ready(SpecializationBatchHandle &handle)
{
  if (handle != 0 && batch_is_ready(handle)) {
    std::lock_guard lock(mutex_);

    Batch *batch = batches_.pop(handle);
    MEM_delete(batch);
    handle = 0;
  }

  return handle == 0;
}

void *ShaderCompiler::pop_work()
{
  /* NOTE: Already under mutex lock when GPUWorker calls this function. */

  if (compilation_queue_.is_empty()) {
    return nullptr;
  }

  ParallelWork work = compilation_queue_.pop();
  return MEM_new<ParallelWork>(__func__, work);
}

void ShaderCompiler::do_work(void *work_payload)
{
  ParallelWork *work = reinterpret_cast<ParallelWork *>(work_payload);
  Batch *batch = work->batch;
  int shader_index = work->shader_index;
  MEM_delete(work);

  /* Compile */
  if (!batch->is_specialization_batch()) {
    batch->shaders[shader_index] = compile_shader(*batch->infos[shader_index]);
  }
  else {
    specialize_shader(batch->specializations[shader_index]);
  }

  {
    std::unique_lock lock(mutex_);
    batch->pending_compilations--;
  }

  compilation_finished_notification_.notify_all();
}

bool ShaderCompiler::is_compiling_impl()
{
  /* The mutex should be locked before calling this function. */
  BLI_assert(!mutex_.try_lock());

  if (!compilation_queue_.is_empty()) {
    return true;
  }

  for (Batch *batch : batches_.values()) {
    if (!batch->is_ready()) {
      return true;
    }
  }

  return false;
}

bool ShaderCompiler::is_compiling()
{
  std::unique_lock lock(mutex_);
  return is_compiling_impl();
}

void ShaderCompiler::wait_for_all()
{
  std::unique_lock lock(mutex_);
  compilation_finished_notification_.wait(lock, [&]() { return !is_compiling_impl(); });
}

/** \} */

}  // namespace blender::gpu

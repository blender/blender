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

#include "CLG_log.h"

#include "GPU_capabilities.hh"
#include "GPU_debug.hh"
#include "GPU_matrix.hh"
#include "GPU_platform.hh"

#include "shader_tool/processor.hh"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_profile_report.hh"
#include "gpu_shader_create_info.hh"
#include "gpu_shader_create_info_private.hh"
#include "gpu_shader_dependency_private.hh"
#include "gpu_shader_metadata_private.hh"
#include "gpu_shader_private.hh"

#include <filesystem>
#include <string>

namespace blender {

extern "C" char datatoc_gpu_shader_colorspace_lib_glsl[];

static CLG_LogRef LOG = {"gpu.shader"};

namespace gpu {

void Shader::dump_source_to_disk(StringRef shader_name,
                                 StringRef shader_name_with_stage_name,
                                 StringRef extension,
                                 StringRef source)
{
  StringRefNull pattern = G.gpu_debug_shader_source_name;
  /* Support starting and/or ending with a wildcard. */
  if (pattern == "*") {
    /* If using a single wildcard, match everything. */
  }
  else if (pattern.startswith("*") && pattern.endswith("*")) {
    std::string sub_str = pattern.substr(1, pattern.size() - 2);
    if (shader_name.find(sub_str) == std::string::npos) {
      return;
    }
  }
  else if (pattern.startswith("*")) {
    std::string sub_str = pattern.substr(1);
    if (!shader_name.endswith(sub_str)) {
      return;
    }
  }
  else if (pattern.endswith("*")) {
    std::string sub_str = pattern.substr(0, pattern.size() - 1);
    if (!shader_name.startswith(sub_str)) {
      return;
    }
  }
  else if (shader_name != pattern) {
    return;
  }

  namespace fs = std::filesystem;
  fs::path shader_dir = fs::current_path() / "Shaders";
  fs::create_directories(shader_dir);
  fs::path file_path = shader_dir / (shader_name_with_stage_name + extension);

  std::ofstream output_source_file(file_path);
  if (output_source_file) {
    output_source_file << source;
    output_source_file.close();
    std::cout << "Shader Source Debug: Writing file: " << file_path << "\n";
  }
  else {
    std::cerr << "Shader Source Debug: Failed to open file: " << file_path << "\n";
  }
}

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

}  // namespace gpu

using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Creation / Destruction
 * \{ */

Shader::Shader(const char *sh_name)
{
  STRNCPY(this->name, sh_name);

  /* Escape the shader name to be able to use it inside an identifier. */
  for (char &c : name) {
    if (c == '\0') {
      break;
    }
    if (!std::isalnum(c)) {
      c = '_';
    }
  }
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

void GPU_shader_free(gpu::Shader *shader)
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

gpu::Shader *GPU_shader_create_from_info_name(const char *info_name)
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

gpu::Shader *GPU_shader_create_from_info(const GPUShaderCreateInfo *_info)
{
  using namespace blender::gpu::shader;
  const ShaderCreateInfo &info = *reinterpret_cast<const ShaderCreateInfo *>(_info);
  return GPUBackend::get()->get_compiler()->compile(info, false);
}

std::string GPU_shader_preprocess_source(StringRefNull original,
                                         gpu::shader::ShaderCreateInfo &info)
{
  if (original.is_empty()) {
    return original;
  }
  gpu::shader::SourceProcessor processor(original, "python_shader.glsl", shader::Language::GLSL);
  auto [processed_str, metadata] = processor.convert();

  for (auto builtin : metadata.builtins) {
    info.builtins(gpu::shader::convert_builtin_bit(builtin));
  }
  return processed_str;
};

gpu::Shader *GPU_shader_create_from_info_python(const GPUShaderCreateInfo *_info)
{
  using namespace blender::gpu::shader;
  ShaderCreateInfo info = *const_cast<ShaderCreateInfo *>(
      reinterpret_cast<const ShaderCreateInfo *>(_info));

  const bool is_compute = !info.compute_source_generated.empty();

  std::array<StringRefNull, 2> includes = {
      "draw_colormanagement_lib.glsl",
      "gpu_shader_python_typedef_lib.glsl",
  };

  if (!info.typedef_source_generated.empty()) {
    info.generated_sources.append(
        {"gpu_shader_python_typedef_lib.glsl", {}, "\n" + info.typedef_source_generated});
  }
  else {
    /* Add emtpy source to avoid warning and importing the placeholder file. */
    info.generated_sources.append({"gpu_shader_python_typedef_lib.glsl", {}, "\n"});
  }

  info.builtins_ |= BuiltinBits::NO_BUFFER_TYPE_LINTING;

  auto preprocess_source = [&](const std::string &input_src) {
    std::string processed_str;
    processed_str += "\n";
    processed_str += "#ifdef CREATE_INFO_RES_PASS_pyGPU_Shader\n";
    processed_str += "CREATE_INFO_RES_PASS_pyGPU_Shader\n";
    processed_str += "#endif\n";
    processed_str += GPU_shader_preprocess_source(input_src, info);
    return processed_str;
  };

  if (is_compute) {
    info.compute_source("gpu_shader_python_comp.glsl");
    info.generated_sources.append({"gpu_shader_python_comp.glsl",
                                   includes,
                                   preprocess_source(info.compute_source_generated)});
  }
  else {
    info.vertex_source("gpu_shader_python_vert.glsl");
    info.generated_sources.append({"gpu_shader_python_vert.glsl",
                                   includes,
                                   preprocess_source(info.vertex_source_generated)});

    info.fragment_source("gpu_shader_python_frag.glsl");
    info.generated_sources.append({"gpu_shader_python_frag.glsl",
                                   includes,
                                   preprocess_source(info.fragment_source_generated)});
  }

  gpu::Shader *result = GPUBackend::get()->get_compiler()->compile(info, false);
  return result;
}

AsyncCompilationHandle GPU_shader_async_compilation(const GPUShaderCreateInfo *info,
                                                    CompilationPriority priority)
{
  using namespace blender::gpu::shader;
  const ShaderCreateInfo *info_ = reinterpret_cast<const ShaderCreateInfo *>(info);
  return GPUBackend::get()->get_compiler()->async_compilation(info_, priority);
}

bool GPU_shader_async_compilation_is_ready(AsyncCompilationHandle handle)
{
  return GPUBackend::get()->get_compiler()->async_compilation_is_ready(handle);
}

gpu::Shader *GPU_shader_async_compilation_finalize(AsyncCompilationHandle &handle)
{
  Shader *result = GPUBackend::get()->get_compiler()->async_compilation_finalize(handle);
  return reinterpret_cast<gpu::Shader *>(result);
}

void GPU_shader_async_compilation_cancel(AsyncCompilationHandle &handle)
{
  GPUBackend::get()->get_compiler()->asyc_compilation_cancel(handle);
}

bool GPU_shader_compiler_has_pending_work()
{
  return GPUBackend::get()->get_compiler()->is_compiling();
}

void GPU_shader_compiler_wait_for_all()
{
  GPUBackend::get()->get_compiler()->wait_for_all();
}

void GPU_shader_compiler_pause()
{
  GPUBackend::get()->get_compiler()->pause_all();
}

void GPU_shader_compiler_resume()
{
  GPUBackend::get()->get_compiler()->continue_all();
}

void GPU_shader_compile_static()
{
  printf("Compiling all static GPU shaders. This process takes a while.\n");
  gpu_shader_create_info_compile_all("");
}

void GPU_shader_cache_dir_clear_old()
{
  GPUBackend::get()->shader_cache_dir_clear_old();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

void GPU_shader_bind(gpu::Shader *gpu_shader,
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

gpu::Shader *GPU_shader_get_bound()
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

const char *GPU_shader_get_name(gpu::Shader *shader)
{
  return shader->name_get().c_str();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader cache warming
 * \{ */

void GPU_shader_set_parent(gpu::Shader *shader, gpu::Shader *parent)
{
  BLI_assert(shader != nullptr);
  BLI_assert(shader != parent);
  if (shader != parent) {
    Shader *shd_child = shader;
    Shader *shd_parent = parent;
    shd_child->parent_set(shd_parent);
  }
}

void GPU_shader_warm_cache(gpu::Shader *shader, int limit)
{
  shader->warm_cache(limit);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Assign specialization constants.
 * \{ */

const shader::SpecializationConstants &GPU_shader_get_default_constant_state(gpu::Shader *sh)
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

AsyncSpecializationHandle GPU_shader_async_specialization(
    const ShaderSpecialization &specialization, CompilationPriority priority)
{
  return GPUBackend::get()->get_compiler()->async_specialization(specialization, priority);
}

bool GPU_shader_async_specialization_is_ready(AsyncSpecializationHandle &handle)
{
  return GPUBackend::get()->get_compiler()->async_specialization_is_ready(handle);
}

void GPU_shader_async_specialization_cancel(AsyncSpecializationHandle &handle)
{
  GPUBackend::get()->get_compiler()->asyc_compilation_cancel(handle);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms / Resource location
 * \{ */

int GPU_shader_get_uniform(gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *uniform = interface->uniform_get(name);
  return uniform ? uniform->location : -1;
}

int GPU_shader_get_constant(gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *constant = interface->constant_get(name);
  return constant ? constant->location : -1;
}

int GPU_shader_get_builtin_uniform(gpu::Shader *shader, int builtin)
{
  const ShaderInterface *interface = shader->interface;
  return interface->uniform_builtin(GPUUniformBuiltin(builtin));
}

int GPU_shader_get_ssbo_binding(gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *ssbo = interface->ssbo_get(name);
  return ssbo ? ssbo->location : -1;
}

int GPU_shader_get_uniform_block(gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *ubo = interface->ubo_get(name);
  return ubo ? ubo->location : -1;
}

int GPU_shader_get_ubo_binding(gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *ubo = interface->ubo_get(name);
  return ubo ? ubo->binding : -1;
}

int GPU_shader_get_sampler_binding(gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *tex = interface->uniform_get(name);
  return tex ? tex->binding : -1;
}

uint GPU_shader_get_attribute_len(const gpu::Shader *shader)
{
  const ShaderInterface *interface = shader->interface;
  return interface->valid_bindings_get(interface->inputs_, interface->attr_len_);
}

uint GPU_shader_get_ssbo_input_len(const gpu::Shader *shader)
{
  const ShaderInterface *interface = shader->interface;
  return interface->ssbo_len_;
}

int GPU_shader_get_attribute(const gpu::Shader *shader, const char *name)
{
  const ShaderInterface *interface = shader->interface;
  const ShaderInput *attr = interface->attr_get(name);
  return attr ? attr->location : -1;
}

bool GPU_shader_get_attribute_info(const gpu::Shader *shader,
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

bool GPU_shader_get_ssbo_input_info(const gpu::Shader *shader, int ssbo_location, char r_name[256])
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
    gpu::Shader *shader, int loc, int len, int array_size, const float *value)
{
  shader->uniform_float(loc, len, array_size, value);
}

void GPU_shader_uniform_int_ex(
    gpu::Shader *shader, int loc, int len, int array_size, const int *value)
{
  shader->uniform_int(loc, len, array_size, value);
}

void GPU_shader_uniform_1i(gpu::Shader *sh, const char *name, int value)
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_int_ex(sh, loc, 1, 1, &value);
}

void GPU_shader_uniform_1b(gpu::Shader *sh, const char *name, bool value)
{
  GPU_shader_uniform_1i(sh, name, value ? 1 : 0);
}

void GPU_shader_uniform_2f(gpu::Shader *sh, const char *name, float x, float y)
{
  const float data[2] = {x, y};
  GPU_shader_uniform_2fv(sh, name, data);
}

void GPU_shader_uniform_3f(gpu::Shader *sh, const char *name, float x, float y, float z)
{
  const float data[3] = {x, y, z};
  GPU_shader_uniform_3fv(sh, name, data);
}

void GPU_shader_uniform_4f(gpu::Shader *sh, const char *name, float x, float y, float z, float w)
{
  const float data[4] = {x, y, z, w};
  GPU_shader_uniform_4fv(sh, name, data);
}

void GPU_shader_uniform_1f(gpu::Shader *sh, const char *name, float value)
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 1, 1, &value);
}

void GPU_shader_uniform_2fv(gpu::Shader *sh, const char *name, const float data[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 2, 1, data);
}

void GPU_shader_uniform_3fv(gpu::Shader *sh, const char *name, const float data[3])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 3, 1, data);
}

void GPU_shader_uniform_4fv(gpu::Shader *sh, const char *name, const float data[4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 4, 1, data);
}

void GPU_shader_uniform_2iv(gpu::Shader *sh, const char *name, const int data[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_int_ex(sh, loc, 2, 1, data);
}

void GPU_shader_uniform_3iv(gpu::Shader *sh, const char *name, const int data[3])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_int_ex(sh, loc, 3, 1, data);
}

void GPU_shader_uniform_mat4(gpu::Shader *sh, const char *name, const float data[4][4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 16, 1, reinterpret_cast<const float *>(data));
}

void GPU_shader_uniform_mat3_as_mat4(gpu::Shader *sh, const char *name, const float data[3][3])
{
  float matrix[4][4];
  copy_m4_m3(matrix, data);
  GPU_shader_uniform_mat4(sh, name, matrix);
}

void GPU_shader_uniform_1f_array(gpu::Shader *sh, const char *name, int len, const float *val)
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 1, len, val);
}

void GPU_shader_uniform_2fv_array(gpu::Shader *sh,
                                  const char *name,
                                  int len,
                                  const float (*val)[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 2, len, reinterpret_cast<const float *>(val));
}

void GPU_shader_uniform_4fv_array(gpu::Shader *sh,
                                  const char *name,
                                  int len,
                                  const float (*val)[4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float_ex(sh, loc, 4, len, reinterpret_cast<const float *>(val));
}

/** \} */

namespace gpu {

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

void Shader::set_srgb_uniform(Context *ctx, gpu::Shader *shader)
{
  int32_t loc = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_SRGB_TRANSFORM);
  if (loc != -1) {
    GPU_shader_uniform_int_ex(shader, loc, 1, 1, &ctx->shader_builtin_srgb_transform);
  }
  ctx->shader_builtin_srgb_is_dirty = false;
}

void Shader::set_scene_linear_to_xyz_uniform(gpu::Shader *shader)
{
  int32_t loc = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_SCENE_LINEAR_XFORM);
  if (loc != -1) {
    GPU_shader_uniform_float_ex(shader, loc, 9, 1, colorspace::scene_linear_to_rec709.ptr()[0]);
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

Shader *ShaderCompiler::compile(const shader::ShaderCreateInfo &orig_info, bool is_codegen_only)
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

  CLOG_INFO(&LOG, "Compiling Shader \"%s\"", orig_info.name_.c_str());

  Shader *shader = GPUBackend::get()->shader_alloc(orig_info.name_.c_str());

  ShaderCreateInfo specialized_info = orig_info;

  if (!specialized_info.compilation_constants_.is_empty()) {
    auto predicate = [&](const ShaderCreateInfo::Resource &res) {
      return !res.conditions.evaluate(specialized_info.compilation_constants_);
    };
    specialized_info.pass_resources_.remove_if(predicate);
    specialized_info.batch_resources_.remove_if(predicate);
    specialized_info.geometry_resources_.remove_if(predicate);
  }

  /* We merged infos keeping duplicates because of possible different condition per definitions.
   * Deduplicate remaining ones to avoid errors. */
  auto cleanup_duplicates = [&](Vector<ShaderCreateInfo::Resource, 0> &resources) {
    Vector<ShaderCreateInfo::Resource, 0> tmp = resources;
    resources.clear();
    resources.extend_non_duplicates(tmp);
  };
  cleanup_duplicates(specialized_info.pass_resources_);
  cleanup_duplicates(specialized_info.batch_resources_);
  cleanup_duplicates(specialized_info.geometry_resources_);

  const std::string error = specialized_info.check_error();
  if (!error.empty()) {
    std::cerr << error << "\n";
    BLI_assert(false);
  }

  const shader::ShaderCreateInfo &info = shader->patch_create_info(specialized_info);

  /* Needs to be called before init as GL uses the default specialization constants state to insert
   * default shader inside a map. */
  shader->specialization_constants_init(info);
  shader->init(info, is_codegen_only);

  shader->fragment_output_bits = 0;
  for (const shader::ShaderCreateInfo::FragOut &frag_out : info.fragment_outputs_) {
    shader->fragment_output_bits |= 1u << frag_out.index;
  }

  std::string defines = shader->defines_declare(info);
  std::string resources = shader->resources_declare(info);

  defines += info.resource_guard_defines(info.compilation_constants_);

  if (!info.compute_entry_fn_.is_empty()) {
    defines += "#define ENTRY_POINT_" + info.compute_entry_fn_ + "\n";
  }
  if (!info.fragment_entry_fn_.is_empty()) {
    defines += "#define ENTRY_POINT_" + info.fragment_entry_fn_ + "\n";
  }
  if (!info.vertex_entry_fn_.is_empty()) {
    defines += "#define ENTRY_POINT_" + info.vertex_entry_fn_ + "\n";
  }

  /* Compilation constants declaration for static branches evaluation.
   * In the future, these can be compiled using function constants on metal to reduce compilation
   * time. */
  for (const auto &constant : info.compilation_constants_) {
    defines += "#define SRT_CONSTANT_" + constant.name + " " + std::to_string(constant.value.i) +
               "\n";
  }

  defines += "#define USE_GPU_SHADER_CREATE_INFO\n";

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
    sources.append(resources);
    sources.append(interface);
    sources.extend(code);

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
    sources.append(resources);
    sources.append(interface);
    sources.extend(code);

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
    sources.append(resources);
    sources.append(layout);
    sources.append(interface);
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
    sources.append(resources);
    sources.extend(code);

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

static ThreadQueueWorkPriority to_work_priority(CompilationPriority priority)
{
  switch (priority) {
    case CompilationPriority::Low:
      return BLI_THREAD_QUEUE_WORK_PRIORITY_LOW;
    case CompilationPriority::Medium:
      return BLI_THREAD_QUEUE_WORK_PRIORITY_NORMAL;
    case CompilationPriority::High:
      return BLI_THREAD_QUEUE_WORK_PRIORITY_HIGH;
  }
  BLI_assert_unreachable();
  return BLI_THREAD_QUEUE_WORK_PRIORITY_NORMAL;
}

ShaderCompiler::ShaderCompiler(uint32_t threads_count,
                               GPUWorker::ContextType context_type,
                               bool support_specializations)
{
  support_specializations_ = support_specializations;

  if (!GPU_use_main_context_workaround()) {
    compilation_worker_ = std::make_unique<GPUWorker>(
        threads_count, context_type, do_work_static_cb);
  }
}

ShaderCompiler::~ShaderCompiler()
{
  compilation_worker_.reset();

  /* Ensure all the requested compilations have been retrieved. */
  BLI_assert(async_compilations_.is_empty());
}

Shader *ShaderCompiler::compile_shader(const shader::ShaderCreateInfo &info)
{
  return compile(info, false);
}

AsyncCompilationHandle ShaderCompiler::async_compilation(const shader::ShaderCreateInfo *info,
                                                         CompilationPriority priority)
{
  std::unique_lock lock(mutex_);

  AsyncCompilation *compilation = MEM_new<AsyncCompilation>(__func__);
  compilation->info = info;

  AsyncCompilationHandle handle = next_handle_++;
  async_compilations_.add(handle, compilation);

  if (compilation_worker_) {
    compilation->work = std::make_unique<ParallelWork>(ParallelWork{this, compilation});
    compilation->work->id = compilation_worker_->push_work(compilation->work.get(),
                                                           to_work_priority(priority));
  }
  else {
    compilation->shader = compile(*info, false);
    compilation->is_ready = true;
  }

  return handle;
}

void ShaderCompiler::asyc_compilation_cancel(AsyncCompilationHandle &handle)
{
  {
    std::unique_lock lock(mutex_);

    AsyncCompilation *compilation = async_compilations_.pop(handle);
    if (compilation_worker_ && compilation_worker_->cancel_work(compilation->work->id)) {
      compilation->is_ready = true;
    }

    /* If it was already being compiled, wait until it's ready so the calling thread can safely
     * delete the ShaderCreateInfos. */
    compilation_finished_notification_.wait(lock, [&]() { return compilation->is_ready == true; });
    GPU_shader_free(compilation->shader);
    compilation->shader = nullptr;
    MEM_delete(compilation);

    handle = 0;
  }

  /* Count this as a finished compilation, since wait_for_all might be waiting. */
  compilation_finished_notification_.notify_all();
}

bool ShaderCompiler::async_compilation_is_ready(AsyncCompilationHandle handle)
{
  std::lock_guard lock(mutex_);

  return async_compilations_.lookup(handle)->is_ready;
}

Shader *ShaderCompiler::async_compilation_finalize(AsyncCompilationHandle &handle)
{
  std::unique_lock lock(mutex_);
  /* TODO: Move to be first on the queue. */
  compilation_finished_notification_.wait(
      lock, [&]() { return async_compilations_.lookup(handle)->is_ready == true; });

  AsyncCompilation *compilation = async_compilations_.pop(handle);
  Shader *shader = compilation->shader;
  MEM_delete(compilation);
  handle = 0;

  return shader;
}

AsyncSpecializationHandle ShaderCompiler::async_specialization(
    const ShaderSpecialization &specialization, CompilationPriority priority)
{
  if (!compilation_worker_ || !support_specializations_) {
    return 0;
  }

  std::lock_guard lock(mutex_);

  AsyncCompilation *compilation = MEM_new<AsyncCompilation>(__func__);
  compilation->specialization = std::make_unique<ShaderSpecialization>(specialization);

  AsyncCompilationHandle handle = next_handle_++;
  async_compilations_.add(handle, compilation);

  compilation->work = std::make_unique<ParallelWork>(ParallelWork{this, compilation});
  compilation->work->id = compilation_worker_->push_work(compilation->work.get(),
                                                         to_work_priority(priority));

  return handle;
}

bool ShaderCompiler::async_specialization_is_ready(AsyncSpecializationHandle &handle)
{
  if (handle == 0) {
    return true;
  }

  std::lock_guard lock(mutex_);
  if (async_compilations_.lookup(handle)->is_ready) {
    AsyncCompilation *compilation = async_compilations_.pop(handle);
    MEM_delete(compilation);
    handle = 0;
  }

  return handle == 0;
}

void ShaderCompiler::do_work_static_cb(void *payload)
{
  ParallelWork *work = reinterpret_cast<ParallelWork *>(payload);
  work->compiler->do_work(*work);
}

void ShaderCompiler::do_work(ParallelWork &work)
{
  AsyncCompilation *compilation = work.compilation;

  /* Compile */
  if (!compilation->is_specialization()) {
    compilation->shader = compile_shader(*compilation->info);
  }
  else {
    specialize_shader(*compilation->specialization);
  }

  {
    std::unique_lock lock(mutex_);
    compilation->is_ready = true;
  }

  compilation_finished_notification_.notify_all();

  /* Pause must happen after the work has finished and before more work is requested,
   * otherwise we can run into deadlocks due to notifications desync. */
  std::unique_lock lock(mutex_);
  pause_finished_notification_.wait(lock, [&]() { return !is_paused_; });
}

bool ShaderCompiler::is_compiling_impl()
{
  /* The mutex should be locked before calling this function. */
  BLI_assert(!mutex_.try_lock());

  if (compilation_worker_ == nullptr) {
    return false;
  }

  if (!compilation_worker_->is_empty()) {
    return true;
  }

  for (AsyncCompilation *compilation : async_compilations_.values()) {
    if (!compilation->is_ready) {
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
  /** NOTE: We can't rely on BLI_thread_queue_wait_finish, since that only waits until the queue is
   * empty, but the works might still being processed. */
  std::unique_lock lock(mutex_);
  BLI_assert(!is_paused_);
  compilation_finished_notification_.wait(lock, [&]() { return !is_compiling_impl(); });
}

void ShaderCompiler::pause_all()
{
  std::unique_lock lock(mutex_);
  BLI_assert(!is_paused_);
  is_paused_ = true;
}

void ShaderCompiler::continue_all()
{
  {
    std::unique_lock lock(mutex_);
    BLI_assert(is_paused_);
    is_paused_ = false;
  }
  pause_finished_notification_.notify_all();
}

/** \} */

}  // namespace gpu
}  // namespace blender

/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_appdir.hh"

#include "BLI_fileops.hh"
#include "BLI_hash.hh"
#include "BLI_path_utils.hh"
#include "BLI_time.h"
#ifdef _WIN32
#  include "BLI_winstuff.h"
#endif

#include "vk_shader.hh"
#include "vk_shader_compiler.hh"

namespace blender::gpu {
VKShaderCompiler::VKShaderCompiler()
{
  task_pool_ = BLI_task_pool_create(nullptr, TASK_PRIORITY_LOW);
}

VKShaderCompiler::~VKShaderCompiler()
{
  BLI_task_pool_work_and_wait(task_pool_);
  BLI_task_pool_free(task_pool_);
  task_pool_ = nullptr;
}

/* -------------------------------------------------------------------- */
/** \name SPIR-V disk cache
 * \{ */

struct SPIRVSidecar {
  /** Size of the SPIRV binary. */
  uint64_t spirv_size;
};

static std::optional<std::string> cache_dir_get()
{
  static std::optional<std::string> result;
  if (!result.has_value()) {
    static char tmp_dir_buffer[FILE_MAX];
    /* Shader builder doesn't return the correct appdir*/
    if (!BKE_appdir_folder_caches(tmp_dir_buffer, sizeof(tmp_dir_buffer))) {
      return std::nullopt;
    }

    std::string cache_dir = std::string(tmp_dir_buffer) + "vk-spirv-cache" + SEP_STR;
    BLI_dir_create_recursive(cache_dir.c_str());
    result = cache_dir;
  }

  return result;
}

static bool read_spirv_from_disk(VKShaderModule &shader_module)
{
  if (G.debug & G_DEBUG_GPU_RENDERDOC) {
    /* RenderDoc uses spirv shaders including debug information. */
    return false;
  }
  std::optional<std::string> cache_dir = cache_dir_get();
  if (!cache_dir.has_value()) {
    return false;
  }
  shader_module.build_sources_hash();
  std::string spirv_path = (*cache_dir) + SEP_STR + shader_module.sources_hash + ".spv";
  std::string sidecar_path = (*cache_dir) + SEP_STR + shader_module.sources_hash + ".sidecar.bin";

  if (!BLI_exists(spirv_path.c_str()) || !BLI_exists(sidecar_path.c_str())) {
    return false;
  }

  BLI_file_touch(spirv_path.c_str());
  BLI_file_touch(sidecar_path.c_str());

  /* Read sidecar*/
  fstream sidecar_file(sidecar_path, std::ios::binary | std::ios::in | std::ios::ate);
  std::streamsize sidecar_size_on_disk = sidecar_file.tellg();
  SPIRVSidecar sidecar = {};
  if (sidecar_size_on_disk != sizeof(sidecar)) {
    return false;
  }
  sidecar_file.seekg(0, std::ios::beg);
  sidecar_file.read(reinterpret_cast<char *>(&sidecar), sizeof(sidecar));

  /* Read spirv binary */
  fstream spirv_file(spirv_path, std::ios::binary | std::ios::in | std::ios::ate);
  std::streamsize size = spirv_file.tellg();
  if (size != sidecar.spirv_size) {
    return false;
  }
  spirv_file.seekg(0, std::ios::beg);
  shader_module.spirv_binary.resize(size / 4);
  spirv_file.read(reinterpret_cast<char *>(shader_module.spirv_binary.data()), size);
  return true;
}

static void write_spirv_to_disk(VKShaderModule &shader_module)
{
  if (G.debug & G_DEBUG_GPU_RENDERDOC) {
    return;
  }
  std::optional<std::string> cache_dir = cache_dir_get();
  if (!cache_dir.has_value()) {
    return;
  }

  /* Write the spirv binary */
  std::string spirv_path = (*cache_dir) + SEP_STR + shader_module.sources_hash + ".spv";
  size_t size = (shader_module.compilation_result.end() -
                 shader_module.compilation_result.begin()) *
                sizeof(uint32_t);
  fstream spirv_file(spirv_path, std::ios::binary | std::ios::out);
  spirv_file.write(reinterpret_cast<const char *>(shader_module.compilation_result.begin()), size);

  /* Write the sidecar */
  SPIRVSidecar sidecar = {size};
  std::string sidecar_path = (*cache_dir) + SEP_STR + shader_module.sources_hash + ".sidecar.bin";
  fstream sidecar_file(sidecar_path, std::ios::binary | std::ios::out);
  sidecar_file.write(reinterpret_cast<const char *>(&sidecar), sizeof(SPIRVSidecar));
}

void VKShaderCompiler::cache_dir_clear_old()
{
  std::optional<std::string> cache_dir = cache_dir_get();
  if (!cache_dir.has_value()) {
    return;
  }

  direntry *entries = nullptr;
  uint32_t dir_len = BLI_filelist_dir_contents(cache_dir->c_str(), &entries);
  for (int i : blender::IndexRange(dir_len)) {
    direntry entry = entries[i];
    if (S_ISDIR(entry.s.st_mode)) {
      continue;
    }
    const time_t ts_now = time(nullptr);
    const time_t delete_threshold = 60 /*seconds*/ * 60 /*minutes*/ * 24 /*hours*/ * 30 /*days*/;
    if (entry.s.st_mtime + delete_threshold < ts_now) {
      BLI_delete(entry.path, false, false);
    }
  }
  BLI_filelist_free(entries, dir_len);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compilation
 * \{ */

BatchHandle VKShaderCompiler::batch_compile(Span<const shader::ShaderCreateInfo *> &infos)
{
  std::scoped_lock lock(mutex_);
  BatchHandle handle = next_batch_handle_++;
  VKBatch &batch = batches_.lookup_or_add_default(handle);
  batch.shaders.reserve(infos.size());
  for (const shader::ShaderCreateInfo *info : infos) {
    Shader *shader = compile(*info, true);
    batch.shaders.append(shader);
  }
  for (Shader *shader : batch.shaders) {
    BLI_task_pool_push(task_pool_, run, shader, false, nullptr);
  }
  return handle;
}

static const std::string to_stage_name(shaderc_shader_kind stage)
{
  switch (stage) {
    case shaderc_vertex_shader:
      return std::string("vertex");
    case shaderc_geometry_shader:
      return std::string("geometry");
    case shaderc_fragment_shader:
      return std::string("fragment");
    case shaderc_compute_shader:
      return std::string("compute");

    default:
      BLI_assert_msg(false, "Do not know how to convert shaderc_shader_kind to stage name.");
      break;
  }
  return std::string("unknown stage");
}

static bool compile_ex(shaderc::Compiler &compiler,
                       VKShader &shader,
                       shaderc_shader_kind stage,
                       VKShaderModule &shader_module)
{
  if (read_spirv_from_disk(shader_module)) {
    return true;
  }

  shaderc::CompileOptions options;
  options.SetOptimizationLevel(shaderc_optimization_level_performance);
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
  if (G.debug & G_DEBUG_GPU_RENDERDOC) {
    options.SetOptimizationLevel(shaderc_optimization_level_zero);
    options.SetGenerateDebugInfo();
  }

  /* WORKAROUND: Qualcomm driver can crash when handling optimized SPIR-V. */
  if (GPU_type_matches(GPU_DEVICE_QUALCOMM, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    options.SetOptimizationLevel(shaderc_optimization_level_zero);
  }

  std::string full_name = std::string(shader.name_get()) + "_" + to_stage_name(stage);
  shader_module.compilation_result = compiler.CompileGlslToSpv(
      shader_module.combined_sources, stage, full_name.c_str(), options);
  bool compilation_succeeded = shader_module.compilation_result.GetCompilationStatus() ==
                               shaderc_compilation_status_success;
  if (compilation_succeeded) {
    write_spirv_to_disk(shader_module);
  }
  return compilation_succeeded;
}

bool VKShaderCompiler::compile_module(VKShader &shader,
                                      shaderc_shader_kind stage,
                                      VKShaderModule &shader_module)
{
  shaderc::Compiler compiler;
  return compile_ex(compiler, shader, stage, shader_module);
}

void VKShaderCompiler::run(TaskPool *__restrict /*pool*/, void *task_data)
{
  VKShader &shader = *static_cast<VKShader *>(task_data);
  shaderc::Compiler compiler;

  bool has_not_succeeded = false;
  if (!shader.vertex_module.is_ready) {
    bool compilation_succeeded = compile_ex(
        compiler, shader, shaderc_vertex_shader, shader.vertex_module);
    has_not_succeeded |= !compilation_succeeded;
    shader.vertex_module.is_ready = true;
  }
  if (!shader.geometry_module.is_ready) {
    bool compilation_succeeded = compile_ex(
        compiler, shader, shaderc_geometry_shader, shader.geometry_module);
    has_not_succeeded |= !compilation_succeeded;
    shader.geometry_module.is_ready = true;
  }
  if (!shader.fragment_module.is_ready) {
    bool compilation_succeeded = compile_ex(
        compiler, shader, shaderc_fragment_shader, shader.fragment_module);
    has_not_succeeded |= !compilation_succeeded;
    shader.fragment_module.is_ready = true;
  }
  if (!shader.compute_module.is_ready) {
    bool compilation_succeeded = compile_ex(
        compiler, shader, shaderc_compute_shader, shader.compute_module);
    has_not_succeeded |= !compilation_succeeded;
    shader.compute_module.is_ready = true;
  }
  if (has_not_succeeded) {
    shader.compilation_failed = true;
  }
  shader.compilation_finished = true;
  shader.finalize_post();
}

bool VKShaderCompiler::batch_is_ready(BatchHandle handle)
{
  std::scoped_lock lock(mutex_);
  BLI_assert(batches_.contains(handle));
  VKBatch &batch = batches_.lookup(handle);
  for (Shader *shader_ : batch.shaders) {
    VKShader &shader = *unwrap(shader_);
    if (!shader.is_ready()) {
      return false;
    }
  }
  return true;
}

Vector<Shader *> VKShaderCompiler::batch_finalize(BatchHandle &handle)
{
  while (!batch_is_ready(handle)) {
    BLI_time_sleep_ms(1);
  }
  std::scoped_lock lock(mutex_);

  BLI_assert(batches_.contains(handle));
  VKBatch batch = batches_.pop(handle);
  handle = 0;
  return batch.shaders;
}

/** \} */

}  // namespace blender::gpu

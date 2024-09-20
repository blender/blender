/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_appdir.hh"
#include "BLI_fileops.hh"
#include "BLI_hash.hh"
#include "BLI_path_util.h"
#include "BLI_time.h"

#include "vk_shader_compiler.hh"

#include "vk_shader.hh"

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
  shaderc::CompileOptions options;
  options.SetOptimizationLevel(shaderc_optimization_level_performance);
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
  if (G.debug & G_DEBUG_GPU_RENDERDOC) {
    options.SetOptimizationLevel(shaderc_optimization_level_zero);
    options.SetGenerateDebugInfo();
  }

  std::string full_name = std::string(shader.name_get()) + "_" + to_stage_name(stage);
  shader_module.compilation_result = compiler.CompileGlslToSpv(
      shader_module.combined_sources, stage, full_name.c_str(), options);
  bool compilation_succeeded = shader_module.compilation_result.GetCompilationStatus() ==
                               shaderc_compilation_status_success;
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

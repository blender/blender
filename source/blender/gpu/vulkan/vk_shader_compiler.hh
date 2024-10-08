/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_shader_private.hh"

#include "BLI_map.hh"
#include "BLI_task.h"
#include "BLI_vector.hh"

#include "shaderc/shaderc.hpp"

namespace blender::gpu {
class VKShader;
class VKShaderModule;

/**
 * Vulkan shader compiler.
 *
 * Is used for both single threaded compilation by calling `VKShaderCompiler::compile_module` or
 * batch based compilation.
 */
class VKShaderCompiler : public ShaderCompiler {
 private:
  std::mutex mutex_;
  BatchHandle next_batch_handle_ = 1;

  struct VKBatch {
    Vector<Shader *> shaders;
  };
  Map<BatchHandle, VKBatch> batches_;

  TaskPool *task_pool_ = nullptr;

 public:
  VKShaderCompiler();
  virtual ~VKShaderCompiler();
  BatchHandle batch_compile(Span<const shader::ShaderCreateInfo *> &infos) override;
  bool batch_is_ready(BatchHandle handle) override;
  Vector<Shader *> batch_finalize(BatchHandle &handle) override;

  static bool compile_module(VKShader &shader,
                             shaderc_shader_kind stage,
                             VKShaderModule &shader_module);

  static void cache_dir_clear_old();

 private:
  static void run(TaskPool *__restrict pool, void *task_data);
};
}  // namespace blender::gpu

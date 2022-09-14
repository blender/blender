/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

/* OSL Shader Engine
 *
 * Holds all variables to execute and use OSL shaders from the kernel. These
 * are initialized externally by OSLShaderManager before rendering starts.
 *
 * Before/after a thread starts rendering, thread_init/thread_free must be
 * called, which will store any per thread OSL state in thread local storage.
 * This means no thread state must be passed along in the kernel itself.
 */

#include "kernel/osl/types.h"

CCL_NAMESPACE_BEGIN

class OSLShader {
 public:
  /* eval */
  static void eval_surface(const KernelGlobalsCPU *kg,
                           const void *state,
                           ShaderData *sd,
                           uint32_t path_flag);
  static void eval_background(const KernelGlobalsCPU *kg,
                              const void *state,
                              ShaderData *sd,
                              uint32_t path_flag);
  static void eval_volume(const KernelGlobalsCPU *kg,
                          const void *state,
                          ShaderData *sd,
                          uint32_t path_flag);
  static void eval_displacement(const KernelGlobalsCPU *kg, const void *state, ShaderData *sd);
};

CCL_NAMESPACE_END

/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __OSL_SHADER_H__
#define __OSL_SHADER_H__

#ifdef WITH_OSL

/* OSL Shader Engine
 *
 * Holds all variables to execute and use OSL shaders from the kernel. These
 * are initialized externally by OSLShaderManager before rendering starts.
 *
 * Before/after a thread starts rendering, thread_init/thread_free must be
 * called, which will store any per thread OSL state in thread local storage.
 * This means no thread state must be passed along in the kernel itself.
 */

#  include "kernel/types.h"

CCL_NAMESPACE_BEGIN

class Scene;

struct ShaderClosure;
struct ShaderData;
struct IntegratorStateCPU;
struct differential3;
struct KernelGlobalsCPU;

struct OSLGlobals;
struct OSLShadingSystem;

class OSLShader {
 public:
  /* init */
  static void register_closures(OSLShadingSystem *ss);

  /* per thread data */
  static void thread_init(KernelGlobalsCPU *kg, OSLGlobals *osl_globals);
  static void thread_free(KernelGlobalsCPU *kg);

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

  /* attributes */
  static int find_attribute(const KernelGlobalsCPU *kg,
                            const ShaderData *sd,
                            uint id,
                            AttributeDescriptor *desc);
};

CCL_NAMESPACE_END

#endif

#endif /* __OSL_SHADER_H__ */

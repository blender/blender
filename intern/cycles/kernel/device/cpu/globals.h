/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

/* Constant Globals */

#pragma once

#include "kernel/tables.h"
#include "kernel/types.h"
#include "kernel/util/profiling.h"

CCL_NAMESPACE_BEGIN

/* On the CPU, we pass along the struct KernelGlobals to nearly everywhere in
 * the kernel, to access constant data. These are all stored as "textures", but
 * these are really just standard arrays. We can't use actually globals because
 * multiple renders may be running inside the same process. */

#ifdef __OSL__
struct OSLGlobals;
struct OSLThreadData;
struct OSLShadingSystem;
#endif

typedef struct KernelGlobalsCPU {
#define KERNEL_TEX(type, name) texture<type> name;
#include "kernel/textures.h"

  KernelData __data;

#ifdef __OSL__
  /* On the CPU, we also have the OSL globals here. Most data structures are shared
   * with SVM, the difference is in the shaders and object/mesh attributes. */
  OSLGlobals *osl;
  OSLShadingSystem *osl_ss;
  OSLThreadData *osl_tdata;
#endif

  /* **** Run-time data ****  */

  ProfilingState profiler;
} KernelGlobalsCPU;

typedef const KernelGlobalsCPU *ccl_restrict KernelGlobals;

/* Abstraction macros */
#define kernel_tex_fetch(tex, index) (kg->tex.fetch(index))
#define kernel_tex_array(tex) (kg->tex.data)
#define kernel_data (kg->__data)

CCL_NAMESPACE_END

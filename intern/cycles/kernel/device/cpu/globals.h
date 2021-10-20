/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Constant Globals */

#pragma once

#include "kernel/kernel_profiling.h"
#include "kernel/kernel_types.h"

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
#include "kernel/kernel_textures.h"

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

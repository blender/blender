/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if !defined(__KERNEL_GPU__)
#  include <OSL/oslversion.h>
#endif

#include "kernel/types.h"

#include "util/defines.h"
#include "util/types_float3.h"

CCL_NAMESPACE_BEGIN

#if defined(__KERNEL_GPU__)
/* Strings are represented by their hashes on the GPU. */
using DeviceString = size_t;
#elif defined(OPENIMAGEIO_USTRING_H)
#  if OSL_LIBRARY_VERSION_CODE >= 11400
using DeviceString = ustringhash;
#  else
using DeviceString = ustring;
#  endif
#else
using DeviceString = const char *;
#endif

struct ThreadKernelGlobalsCPU;
struct IntegratorStateCPU;
struct IntegratorShadowStateCPU;
struct OSLTraceData;

ccl_device_inline DeviceString make_string(const char *str, const size_t hash)
{
#if defined(__KERNEL_GPU__)
  (void)str;
  return hash;
#elif defined(OPENIMAGEIO_USTRING_H)
  (void)hash; /* Ignored in release builds. */
  const DeviceString result = ustring(str);
  kernel_assert(result.hash() == hash);
  return result;
#else
  (void)hash;
  return str;
#endif
}

/* Closure */

enum OSLClosureType {
  OSL_CLOSURE_MUL_ID = -1,
  OSL_CLOSURE_ADD_ID = -2,

  OSL_CLOSURE_NONE_ID = 0,

#define OSL_CLOSURE_STRUCT_BEGIN(Upper, lower) OSL_CLOSURE_##Upper##_ID,
#include "closures_template.h"
  OSL_CLOSURE_LAYER_ID,
};

struct OSLClosure {
  OSLClosureType id;
};

struct ccl_align(8) OSLClosureMul : public OSLClosure {
  packed_float3 weight;
  const ccl_private OSLClosure *closure;
};

struct ccl_align(8) OSLClosureAdd : public OSLClosure {
  const ccl_private OSLClosure *closureA;
  const ccl_private OSLClosure *closureB;
};

struct ccl_align(8) OSLClosureComponent : public OSLClosure {
  packed_float3 weight;
};

/* Globals */

/* This structure is essentially a copy of OSL::ShaderGlobals, but with some of the
 * opaque pointers replaced with the types that we use for them and additional members
 * at the end.
 * As long as the layout matches (which is must in any case, in order for the OptiX OSL
 * code to work), this works fine since OSL doesn't do pointer arithmetic etc. on the
 * ShaderGlobals pointer that we give it. */
struct ShaderGlobals {
  /* This part of ShaderGlobals is shared with OSL's own struct, so the layout must match! */
  packed_float3 P, dPdx, dPdy;
  packed_float3 dPdz;
  packed_float3 I, dIdx, dIdy;
  packed_float3 N;
  packed_float3 Ng;
  float u, dudx, dudy;
  float v, dvdx, dvdy;
  packed_float3 dPdu, dPdv;
  float time;
  float dtime;
  packed_float3 dPdtime;
  packed_float3 Ps, dPsdx, dPsdy;

  /* In OSL this is an opaque pointer named render-state. */
  ccl_private ShaderData *sd;

  /* In OSL this is an opaque pointer */
  ccl_private OSLTraceData *tracedata;

  /* In OSL this is an opaque pointer named `objdata`. */
#ifdef __KERNEL_GPU__
  ccl_private uint8_t *closure_pool;
#else
  const ThreadKernelGlobalsCPU *kg;
#endif

  void *context;
  void *shadingStateUniform;
  int thread_index;

  /* We use this to encode the path state on GPUs.
   * Zero means no state, positive means path_state, negative means shadow_path_state.
   * On CPU, we use pointers in the Cycles-specific section below. */
  int shade_index;

  void *renderer;
  ccl_private void *object2common;
  ccl_private void *shader2common;
  ccl_private OSLClosure *Ci;
  float surfacearea;
  int raytype;
  int flipHandedness;
  int backfacing;

  /* This part is Cycles-specific and ignored by OSL itself. */
#ifndef __KERNEL_GPU__
  const struct IntegratorStateCPU *path_state;
  const struct IntegratorShadowStateCPU *shadow_path_state;
#endif
};

struct OSLNoiseOptions {};

struct OSLTextureOptions {};

#define OSL_TEXTURE_HANDLE_TYPE_IES ((uintptr_t)0x2 << 30)
#define OSL_TEXTURE_HANDLE_TYPE_SVM ((uintptr_t)0x1 << 30)
#define OSL_TEXTURE_HANDLE_TYPE_AO_OR_BEVEL ((uintptr_t)0x3 << 30)

#define OSL_TEXTURE_HANDLE_TYPE(handle) \
  ((unsigned int)((uintptr_t)(handle) & ((uintptr_t)0x3 << 30)))
#define OSL_TEXTURE_HANDLE_SLOT(handle) \
  ((unsigned int)((uintptr_t)(handle) & ((uintptr_t)0x3FFFFFFF)))

CCL_NAMESPACE_END

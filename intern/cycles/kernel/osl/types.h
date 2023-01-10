/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

CCL_NAMESPACE_BEGIN

#if defined(__KERNEL_GPU__)
/* Strings are represented by their hashes on the GPU. */
typedef size_t DeviceString;
#elif defined(OPENIMAGEIO_USTRING_H)
typedef ustring DeviceString;
#else
typedef const char *DeviceString;
#endif

ccl_device_inline DeviceString make_string(const char *str, size_t hash)
{
#if defined(__KERNEL_GPU__)
  (void)str;
  return hash;
#elif defined(OPENIMAGEIO_USTRING_H)
  (void)hash;
  return ustring(str);
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
};

struct OSLClosure {
  OSLClosureType id;
};

struct ccl_align(8) OSLClosureMul : public OSLClosure
{
  packed_float3 weight;
  ccl_private const OSLClosure *closure;
};

struct ccl_align(8) OSLClosureAdd : public OSLClosure
{
  ccl_private const OSLClosure *closureA;
  ccl_private const OSLClosure *closureB;
};

struct ccl_align(8) OSLClosureComponent : public OSLClosure
{
  packed_float3 weight;
};

/* Globals */

struct ShaderGlobals {
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
  ccl_private void *renderstate;
  ccl_private void *tracedata;
  ccl_private void *objdata;
  void *context;
  void *renderer;
  ccl_private void *object2common;
  ccl_private void *shader2common;
  ccl_private OSLClosure *Ci;
  float surfacearea;
  int raytype;
  int flipHandedness;
  int backfacing;
};

struct OSLNoiseOptions {
};

struct OSLTextureOptions {
};

CCL_NAMESPACE_END

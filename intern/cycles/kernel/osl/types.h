/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if !defined(__KERNEL_GPU__)
#  include <OSL/oslversion.h>
#  include <OpenImageIO/oiioversion.h>
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

/* This is copied out of OSL's TextureOpt struct, and it differs between versions.
 * Currently only missingcolor, sblur and tblur are used in Cycles code. */

#ifndef OIIO_VERSION
#  error "OIIO_VERSION must be defined to select the matching OSL TextureOpt layout"
#endif

#if OIIO_VERSION >= 30000
struct OSLTextureOptions {
  int firstchannel = 0;
  int subimage = 0;
  DeviceString /* ustring */ subimagename;
  uint8_t /* Wrap */ swrap = 0;            /* Wrap::Default; */
  uint8_t /* Wrap */ twrap = 0;            /* Wrap::Default; */
  uint8_t /* Wrap */ rwrap = 0;            /* Wrap::Default; */
  uint8_t /* MipMode */ mipmode = 0;       /* MipMode::Default; */
  uint8_t /* InterpMode */ interpmode = 3; /* InterpMode::SmartBicubic; */
  bool conservative_filter = true;
  uint16_t anisotropic = 32;
  float sblur = 0, tblur = 0, rblur = 0;
  float swidth = 1, twidth = 1;
  float rwidth = 1;
  float fill = 0;
  const float *missingcolor = nullptr;
  float rnd = -1;
  int colortransformid = 0;
  int envlayout = 0; /* private */
};
#else
struct OSLTextureOptions {
  int firstchannel;
  int subimage;
  DeviceString /* ustring */ subimagename;
  int /* Wrap */ swrap;
  int /* Wrap */ twrap;
  int /* MipMode */ mipmode;
  int /* InterpMode */ interpmode;
  int anisotropic;
  bool conservative_filter;
  float sblur, tblur;
  float swidth, twidth;
  float fill;
  const float *missingcolor;
  float time;
  union {
    float bias;
    float rnd;
  };
  int samples;
  int /* Wrap */ rwrap;
  float rblur;
  float rwidth;
  int colortransformid;
};
#endif
#ifndef __KERNEL_GPU__
static_assert(sizeof(OSLTextureOptions) == sizeof(OSL::TextureOpt),
              "OSLTextureOptions size mismatch.");
static_assert(sizeof(OSLTextureOptions::sblur) == sizeof(OSL::TextureOpt::sblur),
              "OSLTextureOptions::sblur size mismatch.");
static_assert(sizeof(OSLTextureOptions::tblur) == sizeof(OSL::TextureOpt::tblur),
              "OSLTextureOptions::tblur size mismatch.");
static_assert(sizeof(OSLTextureOptions::missingcolor) == sizeof(OSL::TextureOpt::missingcolor),
              "OSLTextureOptions::missingcolor size mismatch.");
static_assert(offsetof(OSLTextureOptions, sblur) == offsetof(OSL::TextureOpt, sblur),
              "OSLTextureOptions::sblur offsetof mismatch.");
static_assert(offsetof(OSLTextureOptions, tblur) == offsetof(OSL::TextureOpt, tblur),
              "OSLTextureOptions::tblur offsetof mismatch.");
static_assert(offsetof(OSLTextureOptions, missingcolor) == offsetof(OSL::TextureOpt, missingcolor),
              "OSLTextureOptions::missingcolor offsetof mismatch.");
#endif

/* Note: starting from 1 instead of 0 so that the encoded handle is never a null pointer
 * the OSL will interpret as an invalid handle. */
enum class OSLTextureHandleType : unsigned int { IMAGE = 1, IES = 2, BEVEL = 3, AO = 4 };

#define OSL_TEXTURE_HANDLE_ENCODE(type, id) ((uintptr_t(type) << 32) | uintptr_t(uint(id)))
#define OSL_TEXTURE_HANDLE_TYPE(handle) OSLTextureHandleType(uintptr_t(handle) >> 32)
#define OSL_TEXTURE_HANDLE_ID(handle) int(uint(uintptr_t(handle) & uintptr_t(0xFFFFFFFF)))

CCL_NAMESPACE_END

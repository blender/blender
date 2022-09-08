/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#include <OSL/genclosure.h>

#include "kernel/device/cpu/compat.h"
#include "kernel/osl/closures.h"

// clang-format off
#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#include "kernel/types.h"

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_util.h"
#include "kernel/closure/bsdf_diffuse.h"
#include "kernel/closure/bsdf_principled_diffuse.h"
#include "kernel/closure/bssrdf.h"

#include "kernel/util/color.h"
// clang-format on

CCL_NAMESPACE_BEGIN

using namespace OSL;

static ustring u_burley("burley");
static ustring u_random_walk_fixed_radius("random_walk_fixed_radius");
static ustring u_random_walk("random_walk");

class CBSSRDFClosure : public CClosurePrimitive {
 public:
  Bssrdf params;
  float ior;
  ustring method;

  CBSSRDFClosure()
  {
    params.roughness = FLT_MAX;
    params.anisotropy = 1.0f;
    ior = 1.4f;
  }

  void setup(ShaderData *sd, uint32_t path_flag, float3 weight)
  {
    params.N = ensure_valid_reflection(sd->Ng, sd->I, params.N);

    if (method == u_burley) {
      alloc(sd, path_flag, weight, CLOSURE_BSSRDF_BURLEY_ID);
    }
    else if (method == u_random_walk_fixed_radius) {
      alloc(sd, path_flag, weight, CLOSURE_BSSRDF_RANDOM_WALK_FIXED_RADIUS_ID);
    }
    else if (method == u_random_walk) {
      alloc(sd, path_flag, weight, CLOSURE_BSSRDF_RANDOM_WALK_ID);
    }
  }

  void alloc(ShaderData *sd, uint32_t path_flag, float3 weight, ClosureType type)
  {
    Bssrdf *bssrdf = bssrdf_alloc(sd, rgb_to_spectrum(weight));

    if (bssrdf) {
      /* disable in case of diffuse ancestor, can't see it well then and
       * adds considerably noise due to probabilities of continuing path
       * getting lower and lower */
      if (path_flag & PATH_RAY_DIFFUSE_ANCESTOR) {
        params.radius = zero_spectrum();
      }

      /* create one closure per color channel */
      bssrdf->radius = params.radius;
      bssrdf->albedo = params.albedo;
      bssrdf->N = params.N;
      bssrdf->roughness = params.roughness;
      bssrdf->anisotropy = clamp(params.anisotropy, 0.0f, 0.9f);
      sd->flag |= bssrdf_setup(sd, bssrdf, (ClosureType)type, clamp(ior, 1.01f, 3.8f));
    }
  }
};

ClosureParam *closure_bssrdf_params()
{
  static ClosureParam params[] = {
      CLOSURE_STRING_PARAM(CBSSRDFClosure, method),
      CLOSURE_FLOAT3_PARAM(CBSSRDFClosure, params.N),
      CLOSURE_FLOAT3_PARAM(CBSSRDFClosure, params.radius),
      CLOSURE_FLOAT3_PARAM(CBSSRDFClosure, params.albedo),
      CLOSURE_FLOAT_KEYPARAM(CBSSRDFClosure, params.roughness, "roughness"),
      CLOSURE_FLOAT_KEYPARAM(CBSSRDFClosure, ior, "ior"),
      CLOSURE_FLOAT_KEYPARAM(CBSSRDFClosure, params.anisotropy, "anisotropy"),
      CLOSURE_STRING_KEYPARAM(CBSSRDFClosure, label, "label"),
      CLOSURE_FINISH_PARAM(CBSSRDFClosure)};
  return params;
}

CCLOSURE_PREPARE(closure_bssrdf_prepare, CBSSRDFClosure)

CCL_NAMESPACE_END

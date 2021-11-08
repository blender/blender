/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <OSL/genclosure.h>

#include "kernel/device/cpu/compat.h"
#include "kernel/osl/closures.h"

// clang-format off
#include "kernel/types.h"

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_util.h"
#include "kernel/closure/bsdf_diffuse.h"
#include "kernel/closure/bsdf_principled_diffuse.h"
#include "kernel/closure/bssrdf.h"
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
    Bssrdf *bssrdf = bssrdf_alloc(sd, weight);

    if (bssrdf) {
      /* disable in case of diffuse ancestor, can't see it well then and
       * adds considerably noise due to probabilities of continuing path
       * getting lower and lower */
      if (path_flag & PATH_RAY_DIFFUSE_ANCESTOR) {
        params.radius = make_float3(0.0f, 0.0f, 0.0f);
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

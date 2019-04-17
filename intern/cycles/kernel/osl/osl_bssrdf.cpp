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

#include "kernel/kernel_compat_cpu.h"
#include "kernel/osl/osl_closures.h"

#include "kernel/kernel_types.h"
#include "kernel/kernel_montecarlo.h"

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_util.h"
#include "kernel/closure/bsdf_diffuse.h"
#include "kernel/closure/bsdf_principled_diffuse.h"
#include "kernel/closure/bssrdf.h"

CCL_NAMESPACE_BEGIN

using namespace OSL;

static ustring u_cubic("cubic");
static ustring u_gaussian("gaussian");
static ustring u_burley("burley");
static ustring u_principled("principled");
static ustring u_random_walk("random_walk");
static ustring u_principled_random_walk("principled_random_walk");

class CBSSRDFClosure : public CClosurePrimitive {
 public:
  Bssrdf params;
  ustring method;

  CBSSRDFClosure()
  {
    params.texture_blur = 0.0f;
    params.sharpness = 0.0f;
    params.roughness = 0.0f;
  }

  void setup(ShaderData *sd, int path_flag, float3 weight)
  {
    if (method == u_cubic) {
      alloc(sd, path_flag, weight, CLOSURE_BSSRDF_CUBIC_ID);
    }
    else if (method == u_gaussian) {
      alloc(sd, path_flag, weight, CLOSURE_BSSRDF_GAUSSIAN_ID);
    }
    else if (method == u_burley) {
      alloc(sd, path_flag, weight, CLOSURE_BSSRDF_BURLEY_ID);
    }
    else if (method == u_principled) {
      alloc(sd, path_flag, weight, CLOSURE_BSSRDF_PRINCIPLED_ID);
    }
    else if (method == u_random_walk) {
      alloc(sd, path_flag, weight, CLOSURE_BSSRDF_RANDOM_WALK_ID);
    }
    else if (method == u_principled_random_walk) {
      alloc(sd, path_flag, weight, CLOSURE_BSSRDF_PRINCIPLED_RANDOM_WALK_ID);
    }
  }

  void alloc(ShaderData *sd, int path_flag, float3 weight, ClosureType type)
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
      bssrdf->texture_blur = params.texture_blur;
      bssrdf->sharpness = params.sharpness;
      bssrdf->N = params.N;
      bssrdf->roughness = params.roughness;
      sd->flag |= bssrdf_setup(sd, bssrdf, (ClosureType)type);
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
      CLOSURE_FLOAT_KEYPARAM(CBSSRDFClosure, params.texture_blur, "texture_blur"),
      CLOSURE_FLOAT_KEYPARAM(CBSSRDFClosure, params.sharpness, "sharpness"),
      CLOSURE_FLOAT_KEYPARAM(CBSSRDFClosure, params.roughness, "roughness"),
      CLOSURE_STRING_KEYPARAM(CBSSRDFClosure, label, "label"),
      CLOSURE_FINISH_PARAM(CBSSRDFClosure)};
  return params;
}

CCLOSURE_PREPARE(closure_bssrdf_prepare, CBSSRDFClosure)

CCL_NAMESPACE_END

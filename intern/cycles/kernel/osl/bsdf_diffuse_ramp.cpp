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

#include <OpenImageIO/fmath.h>

#include <OSL/genclosure.h>

#include "kernel/kernel_compat_cpu.h"
#include "kernel/osl/osl_closures.h"

#include "kernel/kernel_types.h"
#include "kernel/kernel_montecarlo.h"
#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_diffuse_ramp.h"

CCL_NAMESPACE_BEGIN

using namespace OSL;

class DiffuseRampClosure : public CBSDFClosure {
 public:
  DiffuseRampBsdf params;
  Color3 colors[8];

  void setup(ShaderData *sd, int /* path_flag */, float3 weight)
  {
    DiffuseRampBsdf *bsdf = (DiffuseRampBsdf *)bsdf_alloc_osl(
        sd, sizeof(DiffuseRampBsdf), weight, &params);

    if (bsdf) {
      bsdf->colors = (float3 *)closure_alloc_extra(sd, sizeof(float3) * 8);

      if (bsdf->colors) {
        for (int i = 0; i < 8; i++)
          bsdf->colors[i] = TO_FLOAT3(colors[i]);

        sd->flag |= bsdf_diffuse_ramp_setup(bsdf);
      }
    }
  }
};

ClosureParam *closure_bsdf_diffuse_ramp_params()
{
  static ClosureParam params[] = {CLOSURE_FLOAT3_PARAM(DiffuseRampClosure, params.N),
                                  CLOSURE_COLOR_ARRAY_PARAM(DiffuseRampClosure, colors, 8),
                                  CLOSURE_STRING_KEYPARAM(DiffuseRampClosure, label, "label"),
                                  CLOSURE_FINISH_PARAM(DiffuseRampClosure)};
  return params;
}

CCLOSURE_PREPARE(closure_bsdf_diffuse_ramp_prepare, DiffuseRampClosure)

CCL_NAMESPACE_END

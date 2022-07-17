/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#include <OpenImageIO/fmath.h>

#include <OSL/genclosure.h>

#include "kernel/device/cpu/compat.h"
#include "kernel/osl/closures.h"

// clang-format off
#include "kernel/types.h"
#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_phong_ramp.h"
#include "kernel/closure/bsdf_util.h"
// clang-format on

CCL_NAMESPACE_BEGIN

using namespace OSL;

class PhongRampClosure : public CBSDFClosure {
 public:
  PhongRampBsdf params;
  Color3 colors[8];

  void setup(ShaderData *sd, uint32_t /* path_flag */, float3 weight)
  {
    params.N = ensure_valid_reflection(sd->Ng, sd->I, params.N);

    PhongRampBsdf *bsdf = (PhongRampBsdf *)bsdf_alloc_osl(
        sd, sizeof(PhongRampBsdf), weight, &params);

    if (bsdf) {
      bsdf->colors = (float3 *)closure_alloc_extra(sd, sizeof(float3) * 8);

      if (bsdf->colors) {
        for (int i = 0; i < 8; i++)
          bsdf->colors[i] = TO_FLOAT3(colors[i]);

        sd->flag |= bsdf_phong_ramp_setup(bsdf);
      }
    }
  }
};

ClosureParam *closure_bsdf_phong_ramp_params()
{
  static ClosureParam params[] = {CLOSURE_FLOAT3_PARAM(PhongRampClosure, params.N),
                                  CLOSURE_FLOAT_PARAM(PhongRampClosure, params.exponent),
                                  CLOSURE_COLOR_ARRAY_PARAM(PhongRampClosure, colors, 8),
                                  CLOSURE_STRING_KEYPARAM(PhongRampClosure, label, "label"),
                                  CLOSURE_FINISH_PARAM(PhongRampClosure)};
  return params;
}

CCLOSURE_PREPARE(closure_bsdf_phong_ramp_prepare, PhongRampClosure)

CCL_NAMESPACE_END

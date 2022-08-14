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
#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#include "kernel/types.h"
#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_diffuse_ramp.h"
#include "kernel/closure/bsdf_util.h"

#include "kernel/util/color.h"
// clang-format on

CCL_NAMESPACE_BEGIN

using namespace OSL;

class DiffuseRampClosure : public CBSDFClosure {
 public:
  DiffuseRampBsdf params;
  Color3 colors[8];

  void setup(ShaderData *sd, uint32_t /* path_flag */, float3 weight)
  {
    params.N = ensure_valid_reflection(sd->Ng, sd->I, params.N);

    DiffuseRampBsdf *bsdf = (DiffuseRampBsdf *)bsdf_alloc_osl(
        sd, sizeof(DiffuseRampBsdf), rgb_to_spectrum(weight), &params);

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

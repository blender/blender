/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#include <OpenImageIO/fmath.h>

#include <OSL/genclosure.h>

#include "kernel/osl/closures.h"

// clang-format off
#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#include "kernel/closure/alloc.h"
#include "kernel/closure/emissive.h"

#include "kernel/util/color.h"
// clang-format on

CCL_NAMESPACE_BEGIN

using namespace OSL;

/// Generic background closure
///
/// We only have a background closure for the shaders
/// to return a color in background shaders. No methods,
/// only the weight is taking into account
///
class GenericBackgroundClosure : public CClosurePrimitive {
 public:
  void setup(ShaderData *sd, uint32_t /* path_flag */, float3 weight)
  {
    background_setup(sd, rgb_to_spectrum(weight));
  }
};

/// Holdout closure
///
/// This will be used by the shader to mark the
/// amount of holdout for the current shading
/// point. No parameters, only the weight will be
/// used
///
class HoldoutClosure : CClosurePrimitive {
 public:
  void setup(ShaderData *sd, uint32_t /* path_flag */, float3 weight)
  {
    closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_HOLDOUT_ID, rgb_to_spectrum(weight));
    sd->flag |= SD_HOLDOUT;
  }
};

ClosureParam *closure_background_params()
{
  static ClosureParam params[] = {
      CLOSURE_STRING_KEYPARAM(GenericBackgroundClosure, label, "label"),
      CLOSURE_FINISH_PARAM(GenericBackgroundClosure)};
  return params;
}

CCLOSURE_PREPARE(closure_background_prepare, GenericBackgroundClosure)

ClosureParam *closure_holdout_params()
{
  static ClosureParam params[] = {CLOSURE_FINISH_PARAM(HoldoutClosure)};
  return params;
}

CCLOSURE_PREPARE(closure_holdout_prepare, HoldoutClosure)

CCL_NAMESPACE_END

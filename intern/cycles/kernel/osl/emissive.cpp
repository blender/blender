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

#include "kernel/types.h"
#include "kernel/closure/alloc.h"
#include "kernel/closure/emissive.h"

#include "kernel/util/color.h"
// clang-format on

CCL_NAMESPACE_BEGIN

using namespace OSL;

/// Variable cone emissive closure
///
/// This primitive emits in a cone having a configurable
/// penumbra area where the light decays to 0 reaching the
/// outer_angle limit. It can also behave as a lambertian emitter
/// if the provided angles are PI/2, which is the default
///
class GenericEmissiveClosure : public CClosurePrimitive {
 public:
  void setup(ShaderData *sd, uint32_t /* path_flag */, float3 weight)
  {
    emission_setup(sd, rgb_to_spectrum(weight));
  }
};

ClosureParam *closure_emission_params()
{
  static ClosureParam params[] = {CLOSURE_STRING_KEYPARAM(GenericEmissiveClosure, label, "label"),
                                  CLOSURE_FINISH_PARAM(GenericEmissiveClosure)};
  return params;
}

CCLOSURE_PREPARE(closure_emission_prepare, GenericEmissiveClosure)

CCL_NAMESPACE_END

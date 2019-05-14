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

#include "kernel/osl/osl_closures.h"

#include "kernel/kernel_compat_cpu.h"
#include "kernel/kernel_types.h"
#include "kernel/closure/alloc.h"
#include "kernel/closure/emissive.h"

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
  void setup(ShaderData *sd, int /* path_flag */, float3 weight)
  {
    emission_setup(sd, weight);
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

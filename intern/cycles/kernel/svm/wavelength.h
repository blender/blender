/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2013, Blender Foundation.
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

#pragma once

CCL_NAMESPACE_BEGIN

/* Wavelength to RGB */

ccl_device_noinline void svm_node_wavelength(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             ccl_private float *stack,
                                             uint wavelength,
                                             uint color_out)
{
  float lambda_nm = stack_load_float(stack, wavelength);
  float ii = (lambda_nm - 380.0f) * (1.0f / 5.0f);  // scaled 0..80
  int i = float_to_int(ii);
  float3 color;

  if (i < 0 || i >= 80) {
    color = make_float3(0.0f, 0.0f, 0.0f);
  }
  else {
    ii -= i;
    ccl_constant float *c = cie_colour_match[i];
    color = interp(make_float3(c[0], c[1], c[2]), make_float3(c[3], c[4], c[5]), ii);
  }

  color = xyz_to_rgb(kg, color);
  color *= 1.0f / 2.52f;  // Empirical scale from lg to make all comps <= 1

  /* Clamp to zero if values are smaller */
  color = max(color, make_float3(0.0f, 0.0f, 0.0f));

  stack_store_float3(stack, color_out, color);
}

CCL_NAMESPACE_END

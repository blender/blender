/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

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

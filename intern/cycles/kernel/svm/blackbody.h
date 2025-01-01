/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "kernel/globals.h"

#include "kernel/svm/math_util.h"
#include "kernel/svm/util.h"

#include "kernel/util/colorspace.h"

CCL_NAMESPACE_BEGIN

/* Blackbody Node */

ccl_device_noinline void svm_node_blackbody(KernelGlobals kg,
                                            ccl_private ShaderData *sd,
                                            ccl_private float *stack,
                                            const uint temperature_offset,
                                            const uint col_offset)
{
  /* Input */
  const float temperature = stack_load_float(stack, temperature_offset);

  float3 color_rgb = rec709_to_rgb(kg, svm_math_blackbody_color_rec709(temperature));
  color_rgb = max(color_rgb, zero_float3());

  stack_store_float3(stack, col_offset, color_rgb);
}

CCL_NAMESPACE_END

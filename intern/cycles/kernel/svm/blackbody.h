/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#pragma once

#include "kernel/svm/math_util.h"

CCL_NAMESPACE_BEGIN

/* Blackbody Node */

ccl_device_noinline void svm_node_blackbody(KernelGlobals kg,
                                            ccl_private ShaderData *sd,
                                            ccl_private float *stack,
                                            uint temperature_offset,
                                            uint col_offset)
{
  /* Input */
  float temperature = stack_load_float(stack, temperature_offset);

  float3 color_rgb = rec709_to_rgb(kg, svm_math_blackbody_color_rec709(temperature));

  stack_store_float3(stack, col_offset, color_rgb);
}

CCL_NAMESPACE_END

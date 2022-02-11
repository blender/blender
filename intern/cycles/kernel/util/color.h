/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "util/color.h"

CCL_NAMESPACE_BEGIN

ccl_device float3 xyz_to_rgb(KernelGlobals kg, float3 xyz)
{
  return make_float3(dot(float4_to_float3(kernel_data.film.xyz_to_r), xyz),
                     dot(float4_to_float3(kernel_data.film.xyz_to_g), xyz),
                     dot(float4_to_float3(kernel_data.film.xyz_to_b), xyz));
}

ccl_device float linear_rgb_to_gray(KernelGlobals kg, float3 c)
{
  return dot(c, float4_to_float3(kernel_data.film.rgb_to_y));
}

CCL_NAMESPACE_END

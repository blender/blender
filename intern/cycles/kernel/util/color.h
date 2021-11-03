/*
 * Copyright 2011-2018 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

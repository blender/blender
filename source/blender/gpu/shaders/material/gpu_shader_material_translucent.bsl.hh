/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"
#include "gpu_shader_math_vector_safe.bsl.hh"

[[node]]
void node_bsdf_translucent(float4 color, float3 N, float weight, Closure &result)
{
  color = max(color, float4(0.0f));
  N = safe_normalize(N);

  ClosureTranslucent translucent_data;
  translucent_data.color = color.rgb * weight;
  translucent_data.N = N;

  result = closure_eval(translucent_data);
}

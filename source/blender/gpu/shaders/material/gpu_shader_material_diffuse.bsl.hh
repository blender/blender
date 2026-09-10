/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"
#include "gpu_shader_math_vector_safe.bsl.hh"

[[node]]
void node_bsdf_diffuse(float4 color, float /*roughness*/, float3 N, float weight, Closure &result)
{
  ClosureDiffuse diffuse_data;
  diffuse_data.color = color.rgb * weight;
  diffuse_data.N = safe_normalize(N);

  result = closure_eval(diffuse_data);
}

/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void node_bsdf_ray_portal(
    float4 color, float3 /*position*/, float3 /*direction*/, float weight, Closure &result)
{
  ClosureTransparency transparency_data;
  transparency_data.transmittance = color.rgb * weight;
  transparency_data.holdout = 0.0f;

  result = closure_eval(transparency_data);
}

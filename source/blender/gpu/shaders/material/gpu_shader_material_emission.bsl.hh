/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void node_emission(float4 color, float strength, float weight, Closure &result)
{
  color = max(color, float4(0.0f));
  strength = max(strength, 0.0f);

  ClosureEmission emission_data;
  emission_data.emission = color.rgb * (strength * weight);

  result = closure_eval(emission_data);
}

/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void node_light_info(const float light_index, float4 &color, float &power, float3 &position)
{
  node_light_info_impl(int(light_index), color, power, position);
}

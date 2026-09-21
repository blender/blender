/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_common_hash.bsl.hh"
#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void node_point_info(ShadingData &sd, float3 &position, float &radius, float &random)
{
  position = sd.point_position;
  radius = sd.point_radius;
  random = wang_hash_noise(uint(sd.point_id));
}

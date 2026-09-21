/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void world_normals_get(const ShadingData &sd, float3 &N)
{
  N = sd.N;
}

[[node]]
void world_position_get(const ShadingData &sd, float3 &P)
{
  P = sd.P;
}

/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

[[node]]
void node_bevel(float /*radius*/, float3 N, float3 &result)
{
  result = N;
}

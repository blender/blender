/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

[[node]]
void node_vertex_color(float4 vertex_color, float4 &out_color, float &out_alpha)
{
  out_color = vertex_color;
  out_alpha = vertex_color.a;
}

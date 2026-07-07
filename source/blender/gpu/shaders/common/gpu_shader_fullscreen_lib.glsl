/* SPDX-FileCopyrightText: 2015-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/* Position vertices of a triangle to cover the whole screen. */
void fullscreen_vertex(int vertex_id, float4 &out_position)
{
  int v = vertex_id % 3;
  float x = -1.0f + float((v & 1) << 2);
  float y = -1.0f + float((v & 2) << 1);
  out_position = float4(x, y, 1.0f, 1.0f);
}

void fullscreen_vertex(int vertex_id, float4 &out_position, float2 &out_uv)
{
  fullscreen_vertex(vertex_id, out_position);
  out_uv = (out_position.xy + 1.0f) * 0.5f;
}

/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  float3 image_pos = float3(pos.x, pos.y, 0.0f);
  uv_image = uv;

  float4 position = drw_point_world_to_homogenous(image_pos);
  position.z = 0.0f;
  gl_Position = position;
}

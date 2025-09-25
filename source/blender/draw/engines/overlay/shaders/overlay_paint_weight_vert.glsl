/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_paint_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_paint_weight)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  float3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  /* Separate actual weight and alerts for independent interpolation */
  weight_interp = max(float2(weight, -weight), 0.0f);

  /* Saturate the weight to give a hint of the geometry behind the weights. */
#ifdef FAKE_SHADING
  float3 view_normal = normalize(drw_normal_object_to_view(nor));
  color_fac = abs(dot(view_normal, light_dir));
  color_fac = color_fac * 0.9f + 0.1f;

#else
  color_fac = 1.0f;
#endif

  view_clipping_distances(world_pos);
}

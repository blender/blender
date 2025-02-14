/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  vec3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  /* Separate actual weight and alerts for independent interpolation */
  weight_interp = max(vec2(weight, -weight), 0.0);

  /* Saturate the weight to give a hint of the geometry behind the weights. */
#ifdef FAKE_SHADING
  vec3 view_normal = normalize(drw_normal_object_to_view(nor));
  color_fac = abs(dot(view_normal, light_dir));
  color_fac = color_fac * 0.9 + 0.1;

#else
  color_fac = 1.0;
#endif

  view_clipping_distances(world_pos);
}

/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

vec3 weight_to_rgb(float t)
{
  if (t < 0.0) {
    /* Minimum color, gray */
    return vec3(0.25, 0.25, 0.25);
  }
  else if (t > 1.0) {
    /* Error color. */
    return vec3(1.0, 0.0, 1.0);
  }
  else {
    return texture(weightTex, t).rgb;
  }
}

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
  weightColor = vec4(weight_to_rgb(weight), 1.0);

  view_clipping_distances(world_pos);
}

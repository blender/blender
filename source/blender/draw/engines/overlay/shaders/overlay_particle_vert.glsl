/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

/* TODO(fclem): Share with C code. */
#define VCLASS_SCREENALIGNED (1 << 9)

#define VCLASS_EMPTY_AXES (1 << 11)

vec3 rotate(vec3 vec, vec4 quat)
{
  /* The quaternion representation here stores the w component in the first index. */
  return vec + 2.0 * cross(quat.yzw, cross(quat.yzw, vec) + quat.x * vec);
}

void main()
{
  /* Draw-size packed in alpha. */
  float draw_size = ucolor.a;

  vec3 world_pos = part_pos;

#ifdef USE_DOTS
  gl_Position = point_world_to_ndc(world_pos);
  /* World sized points. */
  gl_PointSize = sizePixel * draw_size * drw_view.winmat[1][1] * sizeViewport.y / gl_Position.w;
#else

  if ((vclass & VCLASS_SCREENALIGNED) != 0) {
    /* World sized, camera facing geometry. */
    world_pos += (ViewMatrixInverse[0].xyz * pos.x + ViewMatrixInverse[1].xyz * pos.y) * draw_size;
  }
  else {
    world_pos += rotate(pos, part_rot) * draw_size;
  }

  gl_Position = point_world_to_ndc(world_pos);
#endif

  /* Coloring */
  if ((vclass & VCLASS_EMPTY_AXES) != 0) {
    /* See VBO construction for explanation. */
    finalColor = vec4(clamp(pos * 10000.0, 0.0, 1.0), 1.0);
  }
  else if (part_val < 0.0) {
    finalColor = vec4(ucolor.rgb, 1.0);
  }
  else {
    finalColor = vec4(texture(weightTex, part_val).rgb, 1.0);
  }

  view_clipping_distances(world_pos);
}

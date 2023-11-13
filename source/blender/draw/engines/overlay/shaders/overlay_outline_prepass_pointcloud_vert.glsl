/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_pointcloud_lib.glsl)

uint outline_colorid_get(void)
{
  int flag = int(abs(ObjectInfo.w));
  bool is_active = (flag & DRW_BASE_ACTIVE) != 0;

  if (isTransform) {
    return 0u; /* colorTransform */
  }
  else if (is_active) {
    return 3u; /* colorActive */
  }
  else {
    return 1u; /* colorSelect */
  }

  return 0u;
}

/* Replace top 2 bits (of the 16bit output) by outlineId.
 * This leaves 16K different IDs to create outlines between objects.
 * SHIFT = (32 - (16 - 2)) */
#define SHIFT 18u

void main()
{
  vec3 world_pos = pointcloud_get_pos();

  gl_Position = point_world_to_ndc(world_pos);

  /* Small bias to always be on top of the geom. */
  gl_Position.z -= 1e-3;

  /* ID 0 is nothing (background) */
  interp.ob_id = uint(resource_handle + 1);

  /* Should be 2 bits only [0..3]. */
  uint outline_id = outline_colorid_get();

  /* Combine for 16bit uint target. */
  interp.ob_id = (outline_id << 14u) | ((interp.ob_id << SHIFT) >> SHIFT);

  view_clipping_distances(world_pos);
}

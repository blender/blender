/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_hair_lib.glsl)

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
  vec3 world_pos = point_object_to_world(pos);
 * SHIFT = (32 - (16 - 2)) */
#define SHIFT 18u

void main()
{
  bool is_persp = (drw_view.winmat[3][3] == 0.0);
  float time, thickness;
  vec3 center_wpos, tan, binor;

  hair_get_center_pos_tan_binor_time(is_persp,
                                     ModelMatrixInverse,
                                     drw_view.viewinv[3].xyz,
                                     drw_view.viewinv[2].xyz,
                                     center_wpos,
                                     tan,
                                     binor,
                                     time,
                                     thickness);
  vec3 world_pos;
  if (hairThicknessRes > 1) {
    /* Calculate the thickness, thick-time, worldpos taken into account the outline. */
    float outline_width = point_world_to_ndc(center_wpos).w * 1.25 * sizeViewportInv.y *
                          drw_view.wininv[1][1];
    thickness += outline_width;
    float thick_time = float(gl_VertexID % hairThicknessRes) / float(hairThicknessRes - 1);
    thick_time = thickness * (thick_time * 2.0 - 1.0);
    /* Take object scale into account.
     * NOTE: This only works fine with uniform scaling. */
    float scale = 1.0 / length(mat3(ModelMatrixInverse) * binor);
    world_pos = center_wpos + binor * thick_time * scale;
  }
  else {
    world_pos = center_wpos;
  }

  gl_Position = point_world_to_ndc(world_pos);

#ifdef USE_GEOM
  vert.pos = point_world_to_view(world_pos);
#endif

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

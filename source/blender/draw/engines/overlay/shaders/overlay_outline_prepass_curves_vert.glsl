/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_outline_info.hh"

VERTEX_SHADER_CREATE_INFO(overlay_outline_prepass_curves)

#include "draw_curves_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

uint outline_colorid_get()
{
  eObjectInfoFlag ob_flag = drw_object_infos().flag;
  bool is_active = flag_test(ob_flag, OBJECT_ACTIVE);

  if (is_transform) {
    return 0u; /* theme.colors.transform */
  }
  else if (is_active) {
    return 3u; /* theme.colors.active */
  }
  else {
    return 1u; /* theme.colors.object_select */
  }

  return 0u;
}

void main()
{
  bool is_persp = (drw_view().winmat[3][3] == 0.0f);
  float time, thickness;
  float3 center_wpos, tangent, binor;

  hair_get_center_pos_tan_binor_time(is_persp,
                                     drw_view().viewinv[3].xyz,
                                     drw_view().viewinv[2].xyz,
                                     center_wpos,
                                     tangent,
                                     binor,
                                     time,
                                     thickness);
  float3 world_pos;
  if (hairThicknessRes > 1) {
    /* Calculate the thickness, thick-time, world-position taken into account the outline. */
    float outline_width = drw_point_world_to_homogenous(center_wpos).w * 1.25f *
                          uniform_buf.size_viewport_inv.y * drw_view().wininv[1][1];
    thickness += outline_width;
    float thick_time = float(gl_VertexID % hairThicknessRes) / float(hairThicknessRes - 1);
    thick_time = thickness * (thick_time * 2.0f - 1.0f);
    /* Take object scale into account.
     * NOTE: This only works fine with uniform scaling. */
    float scale = 1.0f / length(to_float3x3(drw_modelinv()) * binor);
    world_pos = center_wpos + binor * thick_time * scale;
  }
  else {
    world_pos = center_wpos;
  }

  gl_Position = drw_point_world_to_homogenous(world_pos);

#ifdef USE_GEOM
  vert.pos = drw_point_world_to_view(world_pos);
#endif

  /* Small bias to always be on top of the geom. */
  gl_Position.z -= 1e-3f;

  /* ID 0 is nothing (background) */
  interp.ob_id = uint(drw_resource_id() + 1);

  /* Should be 2 bits only [0..3]. */
  uint outline_id = outline_colorid_get();

  /* Combine for 16bit uint target. */
  interp.ob_id = outline_id_pack(outline_id, interp.ob_id);

  view_clipping_distances(world_pos);
}

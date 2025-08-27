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
  const curves::Point ls_pt = curves::point_get(uint(gl_VertexID));
  const curves::Point ws_pt = curves::object_to_world(ls_pt, drw_modelmat());
#if 0 /* TODO(fclem) Unsure what it does. Verify if we can get rid of it. */
  /* Calculate the thickness, thick-time, world-position taken into account the outline. */
  float outline_width = drw_point_world_to_homogenous(ws_pt.P).w * 1.25f *
                        uniform_buf.size_viewport_inv.y * drw_view().wininv[1][1];
#endif
  float3 world_pos = curves::shape_point_get(ws_pt, drw_world_incident_vector(ws_pt.P)).P;

  gl_Position = drw_point_world_to_homogenous(world_pos);

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

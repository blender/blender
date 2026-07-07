/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_sculpt_curves_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_sculpt_curves_selection)

#include "draw_curves_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

float retrieve_selection(const curves::Point pt)
{
  if (is_point_domain) {
    return texelFetch(selection_tx, pt.point_id).r;
  }
  return texelFetch(selection_tx, pt.curve_id).r;
}

#if defined(GPU_NVIDIA) && defined(GPU_OPENGL)
/* WORKAROUND: Fix legacy driver compiler issue (see #148472). */
#  define const
#endif

void main()
{
  const curves::Point ls_pt = curves::point_get(uint(gl_VertexID));
  const curves::Point ws_pt = curves::object_to_world(ls_pt, drw_modelmat());
  float3 world_pos = curves::shape_point_get(ws_pt, drw_world_incident_vector(ws_pt.P)).P;

  gl_Position = drw_point_world_to_homogenous(world_pos);

  mask_weight = 1.0f - (selection_opacity - retrieve_selection(ws_pt) * selection_opacity);

  view_clipping_distances(world_pos);
}

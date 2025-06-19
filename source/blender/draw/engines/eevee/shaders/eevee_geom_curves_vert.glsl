/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_material_info.hh"

VERTEX_SHADER_CREATE_INFO(eevee_clip_plane)
VERTEX_SHADER_CREATE_INFO(eevee_geom_curves)

#include "draw_curves_lib.glsl" /* TODO rename to curve. */
#include "draw_model_lib.glsl"
#include "eevee_attributes_curves_lib.glsl"
#include "eevee_nodetree_vert_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_surf_lib.glsl"
#include "eevee_velocity_lib.glsl"

void main()
{
  DRW_VIEW_FROM_RESOURCE_ID;
#ifdef MAT_SHADOW
  shadow_viewport_layer_set(int(drw_view_id), int(render_view_buf[drw_view_id].viewport_index));
#endif

  init_interface();

  bool is_persp = (drw_view().winmat[3][3] == 0.0f);
  hair_get_pos_tan_binor_time(is_persp,
                              drw_modelinv(),
                              drw_view().viewinv[3].xyz,
                              drw_view().viewinv[2].xyz,
                              interp.P,
                              curve_interp.tangent,
                              curve_interp.binormal,
                              curve_interp.time,
                              curve_interp.thickness,
                              curve_interp.time_width);

  interp.N = cross(curve_interp.tangent, curve_interp.binormal);
  curve_interp_flat.strand_id = hair_get_strand_id();
  curve_interp.barycentric_coords = hair_get_barycentric();
#ifdef MAT_VELOCITY
  /* Due to the screen space nature of the vertex positioning, we compute only the motion of curve
   * strand, not its cylinder. Otherwise we would add the rotation velocity. */
  int vert_idx = hair_get_base_id();
  float3 prv, nxt;
  float3 pos = hair_get_point(vert_idx).position;
  velocity_local_pos_get(pos, vert_idx, prv, nxt);
  /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
   * ignores motion from animated displacement. Supporting animated displacement motion vectors
   * would require evaluating the node-tree multiple time with different node-tree UBOs evaluated
   * at different times, but also with different attributes (maybe we could assume static attribute
   * at least). */
  velocity_vertex(prv, pos, nxt, motion.prev, motion.next);
#endif

  init_globals();
  attrib_load();

  interp.P += nodetree_displacement();

#ifdef MAT_CLIP_PLANE
  clip_interp.clip_distance = dot(clip_plane.plane, float4(interp.P, 1.0f));
#endif

#ifdef MAT_SHADOW
  float3 vs_P = drw_point_world_to_view(interp.P);
  ShadowRenderView view = render_view_buf[drw_view_id];
  shadow_clip.position = shadow_position_vector_get(vs_P, view);
  shadow_clip.vector = shadow_clip_vector_get(vs_P, view.clip_distance_inv);
#endif

  gl_Position = reverse_z::transform(drw_point_world_to_homogenous(interp.P));
}

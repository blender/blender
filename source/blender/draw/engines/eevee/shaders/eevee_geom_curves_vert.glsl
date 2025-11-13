/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_geom_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_clip_plane)
VERTEX_SHADER_CREATE_INFO(eevee_geom_curves)

#include "draw_curves_lib.glsl"
#include "draw_model_lib.glsl"
#include "eevee_attributes_curves_lib.glsl"
#include "eevee_nodetree_vert_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_surf_lib.glsl"
#include "eevee_velocity_lib.glsl"

#if defined(GPU_NVIDIA) && defined(GPU_OPENGL)
/* WORKAROUND: Fix legacy driver compiler issue (see #148472). */
#  define const
#endif

void main()
{
  DRW_VIEW_FROM_RESOURCE_ID;
#ifdef MAT_SHADOW
  shadow_viewport_layer_set(int(drw_view_id), int(render_view_buf[drw_view_id].viewport_index));
#endif

  init_interface();

  const curves::Point ls_pt = curves::point_get(uint(gl_VertexID));
  const curves::Point ws_pt = curves::object_to_world(ls_pt, drw_modelmat());

  const float3 V = drw_world_incident_vector(ws_pt.P);

  const curves::ShapePoint pt = curves::shape_point_get(ws_pt, V);
  interp.P = pt.P;
  /* Correct normal is derived in fragment shader. */
  interp.N = pt.curve_N;
  curve_interp.binormal = pt.curve_B;
  curve_interp.tangent = pt.curve_T;
  /* Final radius is used for correct normal interpolation. */
  curve_interp.radius = ws_pt.radius;
  /* Scaled by radius for correct interpolation. */
  curve_interp.time_width = ws_pt.azimuthal_offset * ws_pt.radius;
  /* Note: Used for attribute loading. */
  curve_interp.point_id = float(ws_pt.point_id);
  curve_interp_flat.strand_id = ws_pt.curve_id;

#ifdef MAT_VELOCITY
  /* Due to the screen space nature of the vertex positioning, we compute only the motion of curve
   * strand, not its cylinder. Otherwise we would add the rotation velocity. */
  int vert_idx = ws_pt.point_id;
  float3 prv, nxt;
  float3 pos = ls_pt.P;
  velocity_local_pos_get(pos, vert_idx, prv, nxt);
  /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
   * ignores motion from animated displacement. Supporting animated displacement motion vectors
   * would require evaluating the node-tree multiple time with different node-tree UBOs evaluated
   * at different times, but also with different attributes (maybe we could assume static attribute
   * at least). */
  velocity_vertex(prv, pos, nxt, motion.prev, motion.next);
#endif

  init_globals();
  attrib_load(CurvesPoint(ws_pt.curve_id, ws_pt.point_id, ws_pt.curve_segment));

  interp.P += nodetree_displacement();

#ifdef MAT_SHADOW
  /* Since curves always face the view, camera and shadow orientation don't match.
   * Apply a bias to avoid self-shadow issues. */
  interp.P -= V * ws_pt.radius;
#endif

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

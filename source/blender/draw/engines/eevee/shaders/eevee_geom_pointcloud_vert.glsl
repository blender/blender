/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_geom_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_clip_plane)
VERTEX_SHADER_CREATE_INFO(eevee_geom_pointcloud)

#include "draw_model_lib.glsl"
#include "draw_pointcloud_lib.glsl"
#include "eevee_attributes_pointcloud_lib.glsl"
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

  pointcloud_interp_flat.id = pointcloud_get_point_id();
  pointcloud_get_pos_and_radius(pointcloud_interp.position, pointcloud_interp.radius);
  pointcloud_get_pos_and_nor(interp.P, interp.N);
#ifdef MAT_SHADOW
  /* Since point clouds always face the view, camera and shadow orientation don't match.
   * Apply a bias to avoid self-shadow issues. */
  interp.P -= drw_world_incident_vector(interp.P) * pointcloud_interp.radius;
#endif

#ifdef MAT_VELOCITY
  float3 lP = drw_point_world_to_object(pointcloud_interp.position);
  float3 prv, nxt;
  velocity_local_pos_get(lP, pointcloud_interp_flat.id, prv, nxt);
  /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
   * ignores motion from animated displacement. Supporting animated displacement motion vectors
   * would require evaluating the nodetree multiple time with different nodetree UBOs evaluated at
   * different times, but also with different attributes (maybe we could assume static attribute at
   * least). */
  velocity_vertex(prv, lP, nxt, motion.prev, motion.next);
#endif

  init_globals();
  attrib_load(PointCloudPoint(0));

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

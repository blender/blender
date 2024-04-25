/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_model_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_velocity_lib.glsl)

void main()
{
  DRW_VIEW_FROM_RESOURCE_ID;
#ifdef MAT_SHADOW
  shadow_viewport_layer_set(int(drw_view_id), int(render_view_buf[drw_view_id].viewport_index));
#endif

  init_interface();

  interp.P = drw_point_object_to_world(pos);
  interp.N = normalize(drw_normal_object_to_world(nor));
#ifdef MAT_VELOCITY
  vec3 prv, nxt;
  velocity_local_pos_get(pos, gl_VertexID, prv, nxt);
  /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
   * ignores motion from animated displacement. Supporting animated displacement motion vectors
   * would require evaluating the nodetree multiple time with different nodetree UBOs evaluated at
   * different times, but also with different attributes (maybe we could assume static attribute at
   * least). */
  velocity_vertex(prv, pos, nxt, motion.prev, motion.next);
#endif

  init_globals();
  attrib_load();

  interp.P += nodetree_displacement();

#ifdef MAT_CLIP_PLANE
  clip_interp.clip_distance = dot(clip_plane.plane, vec4(interp.P, 1.0));
#endif

#ifdef MAT_SHADOW
  shadow_clip.vector = shadow_clip_vector_get(drw_point_world_to_view(interp.P),
                                              render_view_buf[drw_view_id].clip_distance_inv);
#endif

  gl_Position = drw_point_world_to_homogenous(interp.P);
}

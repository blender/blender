/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_model_lib.glsl)
/* Grease pencil includes commmon_view_lib. */
// #pragma BLENDER_REQUIRE(common_gpencil_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_velocity_lib.glsl)

void main()
{
  DRW_VIEW_FROM_RESOURCE_ID;
#ifdef MAT_SHADOW
  shadow_viewport_layer_set(int(drw_view_id), int(render_view_buf[drw_view_id].viewport_index));
#endif

  init_interface();

  /* TODO(fclem): Expose through a node? */
  vec4 sspos;
  vec2 aspect;
  float strength;
  float hardness;
  vec2 thickness;

  gl_Position = gpencil_vertex(
      /* TODO */
      vec4(1024.0, 1024.0, 1.0 / 1024.0, 1.0 / 1024.0),
      interp.P,
      interp.N,
      g_color,
      strength,
      g_uvs,
      sspos,
      aspect,
      thickness,
      hardness);
#ifdef MAT_VELOCITY
  /* GPencil do not support deformation motion blur. */
  vec3 lP_curr = drw_point_world_to_object(interp.P);
  /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
   * ignores motion from animated displacement. Supporting animated displacement motion vectors
   * would require evaluating the node-tree multiple time with different node-tree UBOs evaluated
   * at different times, but also with different attributes (maybe we could assume static attribute
   * at least). */
  velocity_vertex(lP_curr, lP_curr, lP_curr, motion.prev, motion.next);
#endif

  init_globals();
  attrib_load();

  interp.P += nodetree_displacement();

#ifdef MAT_CLIP_PLANE
  clip_interp.clip_distance = dot(clip_plane.plane, vec4(interp.P, 1.0));
#endif

#ifdef MAT_SHADOW
  vec3 vs_P = drw_point_world_to_view(interp.P);
  ShadowRenderView view = render_view_buf[drw_view_id];
  shadow_clip.position = shadow_position_vector_get(vs_P, view);
  shadow_clip.vector = shadow_clip_vector_get(vs_P, view.clip_distance_inv);
#endif
}

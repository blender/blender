
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_camera_lib.glsl)

#ifdef VELOCITY_CAMERA

/**
 * Given a triple of position, compute the previous and next motion vectors.
 * Returns uv space motion vectors in pairs (motion_prev.xy, motion_next.xy)
 */
vec4 velocity_view(vec3 P_prev, vec3 P, vec3 P_next)
{
  vec2 prev_uv, curr_uv, next_uv;

  prev_uv = transform_point(ProjectionMatrix, transform_point(camera_prev.viewmat, P_prev)).xy;
  curr_uv = transform_point(ViewProjectionMatrix, P).xy;
  next_uv = transform_point(ProjectionMatrix, transform_point(camera_next.viewmat, P_next)).xy;

  vec4 motion;
  motion.xy = prev_uv - curr_uv;
  motion.zw = curr_uv - next_uv;
  /* Convert NDC velocity to UV velocity */
  motion *= 0.5;

  return motion;
}

/**
 * Given a triple of position, compute the previous and next motion vectors.
 * Returns uv space motion vectors in pairs (motion_prev.xy, motion_next.xy)
 * \a velocity_camera is the motion in film UV space after camera projection.
 * \a velocity_view is the motion in ShadingView UV space. It is different
 * from velocity_camera for multi-view rendering.
 */
void velocity_camera(vec3 P_prev, vec3 P, vec3 P_next, out vec4 vel_camera, out vec4 vel_view)
{
  vec2 prev_uv, curr_uv, next_uv;
  prev_uv = camera_uv_from_world(camera_prev, P_prev);
  curr_uv = camera_uv_from_world(camera_curr, P);
  next_uv = camera_uv_from_world(camera_next, P_next);

  vel_camera.xy = prev_uv - curr_uv;
  vel_camera.zw = curr_uv - next_uv;

  if (is_panoramic(camera_curr.type)) {
    /* This path is only used if using using panoramic projections. Since the views always have
     * the same 45° aperture angle, we can safely reuse the projection matrix. */
    prev_uv = transform_point(ProjectionMatrix, transform_point(camera_prev.viewmat, P_prev)).xy;
    curr_uv = transform_point(ViewProjectionMatrix, P).xy;
    next_uv = transform_point(ProjectionMatrix, transform_point(camera_next.viewmat, P_next)).xy;

    vel_view.xy = prev_uv - curr_uv;
    vel_view.zw = curr_uv - next_uv;
    /* Convert NDC velocity to UV velocity */
    vel_view *= 0.5;
  }
  else {
    vel_view = vel_camera;
  }
}

#endif

#ifdef MAT_VELOCITY

/**
 * Given a triple of position, compute the previous and next motion vectors.
 * Returns a tuple of world space motion deltas.
 */
void velocity_local_pos_get(vec3 lP, int vert_id, out vec3 lP_prev, out vec3 lP_next)
{
  VelocityIndex vel = velocity_indirection_buf[resource_id];
  lP_next = lP_prev = lP;
  if (vel.geo.do_deform) {
    if (vel.geo.ofs[STEP_PREVIOUS] != -1) {
      lP_prev = velocity_geo_prev_buf[vel.geo.ofs[STEP_PREVIOUS] + vert_id].xyz;
    }
    if (vel.geo.ofs[STEP_NEXT] != -1) {
      lP_next = velocity_geo_next_buf[vel.geo.ofs[STEP_NEXT] + vert_id].xyz;
    }
  }
}

/**
 * Given a triple of position, compute the previous and next motion vectors.
 * Returns a tuple of world space motion deltas.
 */
void velocity_vertex(
    vec3 lP_prev, vec3 lP, vec3 lP_next, out vec3 motion_prev, out vec3 motion_next)
{
  VelocityIndex vel = velocity_indirection_buf[resource_id];
  mat4 obmat_prev = velocity_obj_prev_buf[vel.obj.ofs[STEP_PREVIOUS]];
  mat4 obmat_next = velocity_obj_next_buf[vel.obj.ofs[STEP_NEXT]];
  vec3 P_prev = transform_point(obmat_prev, lP_prev);
  vec3 P_next = transform_point(obmat_next, lP_next);
  vec3 P = transform_point(ModelMatrix, lP);
  motion_prev = P_prev - P;
  motion_next = P_next - P;
}

#endif

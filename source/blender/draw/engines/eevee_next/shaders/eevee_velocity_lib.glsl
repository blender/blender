/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_camera_lib.glsl)

vec4 velocity_pack(vec4 data)
{
  return data * 0.01;
}

vec4 velocity_unpack(vec4 data)
{
  return data * 100.0;
}

#ifdef VELOCITY_CAMERA

/**
 * Given a triple of position, compute the previous and next motion vectors.
 * Returns uv space motion vectors in pairs (motion_prev.xy, motion_next.xy).
 */
vec4 velocity_surface(vec3 P_prv, vec3 P, vec3 P_nxt)
{
  /* NOTE: We don't use the drw_view.persmat to avoid adding the TAA jitter to the velocity. */
  vec2 prev_uv = project_point(camera_prev.persmat, P_prv).xy;
  vec2 curr_uv = project_point(camera_curr.persmat, P).xy;
  vec2 next_uv = project_point(camera_next.persmat, P_nxt).xy;
  /* Fix issue with perspective division. */
  if (any(isnan(prev_uv))) {
    prev_uv = curr_uv;
  }
  if (any(isnan(next_uv))) {
    next_uv = curr_uv;
  }
  /* NOTE: We output both vectors in the same direction so we can reuse the same vector
   * with RGRG swizzle in viewport. */
  vec4 motion = vec4(prev_uv - curr_uv, curr_uv - next_uv);
  /* Convert NDC velocity to UV velocity */
  motion *= 0.5;

  return motion;
}

/**
 * Given a view space view vector \a vV, compute the previous and next motion vectors for
 * background pixels.
 * Returns uv space motion vectors in pairs (motion_prev.xy, motion_next.xy).
 */
vec4 velocity_background(vec3 vV)
{
  /* Only transform direction to avoid losing precision. */
  vec3 V = transform_direction(camera_curr.viewinv, vV);
  /* NOTE: We don't use the drw_view.winmat to avoid adding the TAA jitter to the velocity. */
  vec2 prev_uv = project_point(camera_prev.winmat, V).xy;
  vec2 curr_uv = project_point(camera_curr.winmat, V).xy;
  vec2 next_uv = project_point(camera_next.winmat, V).xy;
  /* NOTE: We output both vectors in the same direction so we can reuse the same vector
   * with RGRG swizzle in viewport. */
  vec4 motion = vec4(prev_uv - curr_uv, curr_uv - next_uv);
  /* Convert NDC velocity to UV velocity */
  motion *= 0.5;

  return motion;
}

vec4 velocity_resolve(vec4 vector, vec2 uv, float depth)
{
  if (vector.x == VELOCITY_INVALID) {
    bool is_background = (depth == 1.0);
    if (is_background) {
      /* NOTE: Use viewCameraVec to avoid imprecision if camera is far from origin. */
      vec3 vV = viewCameraVec(get_view_space_from_depth(uv, 1.0));
      return velocity_background(vV);
    }
    else {
      /* Static geometry. No translation in world space. */
      vec3 P = get_world_space_from_depth(uv, depth);
      return velocity_surface(P, P, P);
    }
  }
  return velocity_unpack(vector);
}

/**
 * Load and resolve correct velocity as some pixels might still not have correct
 * motion data for performance reasons.
 * Returns motion vector in render UV space.
 */
vec4 velocity_resolve(sampler2D vector_tx, ivec2 texel, float depth)
{
  vec2 uv = (vec2(texel) + 0.5) / vec2(textureSize(vector_tx, 0).xy);
  vec4 vector = texelFetch(vector_tx, texel, 0);
  return velocity_resolve(vector, uv, depth);
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

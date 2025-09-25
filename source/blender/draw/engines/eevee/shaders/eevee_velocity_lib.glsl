/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_velocity_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_velocity_camera)

#include "draw_view_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"

float4 velocity_pack(float4 data)
{
  return data * 0.01f;
}

float4 velocity_unpack(float4 data)
{
  return data * 100.0f;
}

#ifdef VELOCITY_CAMERA

/**
 * Given a triple of position, compute the previous and next motion vectors.
 * Returns uv space motion vectors in pairs (motion_prev.xy, motion_next.xy).
 */
float4 velocity_surface(float3 P_prv, float3 P, float3 P_nxt)
{
  /* NOTE: We use CameraData matrices instead of drw_view().persmat to avoid adding the TAA jitter
   * to the velocity. */
  float2 prev_uv = project_point(camera_prev.persmat, P_prv).xy;
  float2 curr_uv = project_point(camera_curr.persmat, P).xy;
  float2 next_uv = project_point(camera_next.persmat, P_nxt).xy;
  /* Fix issue with perspective division. */
  if (any(isnan(prev_uv))) {
    prev_uv = curr_uv;
  }
  if (any(isnan(next_uv))) {
    next_uv = curr_uv;
  }
  /* NOTE: We output both vectors in the same direction so we can reuse the same vector
   * with RGRG swizzle in viewport. */
  float4 motion = float4(prev_uv - curr_uv, curr_uv - next_uv);
  /* Convert NDC velocity to UV velocity */
  motion *= 0.5f;

  return motion;
}

/**
 * Given a view space view vector \a vV, compute the previous and next motion vectors for
 * background pixels.
 * Returns uv space motion vectors in pairs (motion_prev.xy, motion_next.xy).
 */
float4 velocity_background(float3 vV)
{
  float3 V = transform_direction(camera_curr.viewinv, vV);
  /* NOTE: We use CameraData matrices instead of drw_view().winmat to avoid adding the TAA jitter
   * to the velocity. */
  float2 prev_uv =
      project_point(camera_prev.winmat, transform_direction(camera_prev.viewmat, V)).xy;
  float2 curr_uv =
      project_point(camera_curr.winmat, transform_direction(camera_curr.viewmat, V)).xy;
  float2 next_uv =
      project_point(camera_next.winmat, transform_direction(camera_next.viewmat, V)).xy;
  /* NOTE: We output both vectors in the same direction so we can reuse the same vector
   * with RGRG swizzle in viewport. */
  float4 motion = float4(prev_uv - curr_uv, curr_uv - next_uv);
  /* Convert NDC velocity to UV velocity */
  motion *= 0.5f;

  return motion;
}

float4 velocity_resolve(float4 vector, float2 uv, float depth)
{
  if (vector.x == VELOCITY_INVALID) {
    bool is_background = (depth == 1.0f);
    if (is_background) {
      /* NOTE: Use view vector to avoid imprecision if camera is far from origin. */
      float3 vV = -drw_view_incident_vector(drw_point_screen_to_view(float3(uv, 1.0f)));
      return velocity_background(vV);
    }
    else {
      /* Static geometry. No translation in world space. */
      float3 P = drw_point_screen_to_world(float3(uv, depth));
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
float4 velocity_resolve(sampler2D vector_tx, int2 texel, float depth)
{
  float2 uv = (float2(texel) + 0.5f) / float2(textureSize(vector_tx, 0).xy);
  float4 vector = texelFetch(vector_tx, texel, 0);
  return velocity_resolve(vector, uv, depth);
}

#endif

#ifdef MAT_VELOCITY

/**
 * Given a triple of position, compute the previous and next motion vectors.
 * Returns a tuple of local space motion deltas.
 */
void velocity_local_pos_get(float3 lP, int vert_id, out float3 lP_prev, out float3 lP_next)
{
  VelocityIndex vel = velocity_indirection_buf[drw_resource_id()];
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
 * WARNING: The returned motion_next is invalid when rendering the viewport.
 */
void velocity_vertex(
    float3 lP_prev, float3 lP, float3 lP_next, out float3 motion_prev, out float3 motion_next)
{
  VelocityIndex vel = velocity_indirection_buf[drw_resource_id()];
  float4x4 obmat_prev = velocity_obj_prev_buf[vel.obj.ofs[STEP_PREVIOUS]];
  float4x4 obmat_next = velocity_obj_next_buf[vel.obj.ofs[STEP_NEXT]];
  float3 P_prev = transform_point(obmat_prev, lP_prev);
  float3 P_next = transform_point(obmat_next, lP_next);
  float3 P = transform_point(drw_modelmat(), lP);
  motion_prev = P_prev - P;
  motion_next = P_next - P;
}

#endif

/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view_lib.glsl"
#include "eevee_camera_shared.hh"
#include "eevee_defines.hh"
#include "eevee_velocity_shared.hh"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"

namespace eevee::velocity {

float4 pack(float4 data)
{
  return data * 0.01f;
}

float4 unpack(float4 data)
{
  return data * 100.0f;
}

struct VertexCopy {
  [[storage(0, read)]] const float (&in_buf)[];
  [[storage(1, write)]] float4 (&out_buf)[];
  [[push_constant]] const int start_offset;
  [[push_constant]] const int vertex_stride;
  [[push_constant]] const int vertex_count;
};

/* Buffer copy using compute shader.
 * Allows to pad data for 16byte alignment regardless of input layout. */
[[compute, local_size(VERTEX_COPY_GROUP_SIZE)]]
void vertex_copy([[resource_table]] VertexCopy &srt,
                 [[global_invocation_id]] const uint3 global_id,
                 [[num_work_groups]] const uint3 groups_count)
{
  uint vert_start = uint(srt.start_offset);
  uint vert_count = uint(srt.vertex_count);
  uint vert_stride = uint(srt.vertex_stride);

  uint vertices_per_thread = divide_ceil(vert_count, uint(VERTEX_COPY_GROUP_SIZE)) /
                             groups_count.x;
  uint vertex_start = min(global_id.x * vertices_per_thread, vert_count);
  uint vertex_end = min(vertex_start + vertices_per_thread, vert_count);

  for (uint vertex_id = vertex_start; vertex_id < vertex_end; vertex_id++) {
    srt.out_buf[vert_start + vertex_id] = float4(
        srt.in_buf[vertex_id * vert_stride + 0],
        srt.in_buf[vertex_id * vert_stride + 1],
        srt.in_buf[vertex_id * vert_stride + 2],
        1.0f /* TODO(fclem): Remove padding or use it for some other data (radius?). */);
  }
}

}  // namespace eevee::velocity

namespace eevee {

struct CameraVelocity {
  [[uniform(VELOCITY_CAMERA_PREV_BUF)]] const CameraData &camera_prev;
  [[uniform(VELOCITY_CAMERA_CURR_BUF)]] const CameraData &camera_curr;
  [[uniform(VELOCITY_CAMERA_NEXT_BUF)]] const CameraData &camera_next;
  /**
   * Given a triple of position, compute the previous and next motion vectors.
   * Returns uv space motion vectors in pairs (motion_prev.xy, motion_next.xy).
   */
  float4 surface_velocity(float3 P_prv, float3 P, float3 P_nxt) const
  {
    /* NOTE: We use CameraData matrices instead of drw_view().persmat to avoid adding the TAA
     * jitter to the velocity. */
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
  float4 background_velocity(float3 vV) const
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

  float4 resolve(float4 vector, float2 uv, float depth) const
  {
    if (vector.x == VELOCITY_INVALID) {
      bool is_background = (depth == 1.0f);
      if (is_background) {
        /* NOTE: Use view vector to avoid imprecision if camera is far from origin. */
        float3 vV = -drw_view_incident_vector(drw_point_screen_to_view(float3(uv, 1.0f)));
        return background_velocity(vV);
      }
      /* Static geometry. No translation in world space. */
      float3 P = drw_point_screen_to_world(float3(uv, depth));
      return surface_velocity(P, P, P);
    }
    return velocity::unpack(vector);
  }

  /**
   * Load and resolve correct velocity as some pixels might still not have correct
   * motion data for performance reasons.
   * Returns motion vector in render UV space.
   */
  float4 resolve(sampler2D vector_tx, int2 texel, float depth) const
  {
    float2 uv = (float2(texel) + 0.5f) / float2(textureSize(vector_tx, 0).xy);
    float4 vector = texelFetch(vector_tx, texel, 0);
    return resolve(vector, uv, depth);
  }
};

struct GeometryVelocity {
  [[legacy_info]] ShaderCreateInfo eevee_velocity_iface_info;

  [[resource_table]] srt_t<CameraVelocity> camera;

  [[storage(VELOCITY_OBJ_PREV_BUF_SLOT, read)]] const float4x4 (&velocity_obj_prev_buf)[];
  [[storage(VELOCITY_OBJ_NEXT_BUF_SLOT, read)]] const float4x4 (&velocity_obj_next_buf)[];
  [[storage(VELOCITY_GEO_PREV_BUF_SLOT, read)]] const float4 (&velocity_geo_prev_buf)[];
  [[storage(VELOCITY_GEO_NEXT_BUF_SLOT, read)]] const float4 (&velocity_geo_next_buf)[];
  [[storage(VELOCITY_INDIRECTION_BUF_SLOT,
            read)]] const VelocityIndex (&velocity_indirection_buf)[];

  /**
   * Given a triple of position, compute the previous and next motion vectors.
   * Returns a tuple of local space motion deltas.
   */
  void local_position_deltas(
      float3 lP, int vert_id, float3 &lP_prev, float3 &lP_next, uint resource_id) const
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
   * WARNING: The returned motion_next is invalid when rendering the viewport.
   */
  void vertex_velocity(float3 lP_prev,
                       float3 lP,
                       float3 lP_next,
                       float3 &motion_prev,
                       float3 &motion_next,
                       uint resource_id,
                       float4x4 model_mat) const
  {
    VelocityIndex vel = velocity_indirection_buf[resource_id];
    float4x4 obmat_prev = velocity_obj_prev_buf[vel.obj.ofs[STEP_PREVIOUS]];
    float4x4 obmat_next = velocity_obj_next_buf[vel.obj.ofs[STEP_NEXT]];
    float3 P_prev = transform_point(obmat_prev, lP_prev);
    float3 P_next = transform_point(obmat_next, lP_next);
    float3 P = transform_point(model_mat, lP);
    motion_prev = P_prev - P;
    motion_next = P_next - P;
  }
};

}  // namespace eevee

PipelineCompute eevee_vertex_copy(eevee::velocity::vertex_copy);

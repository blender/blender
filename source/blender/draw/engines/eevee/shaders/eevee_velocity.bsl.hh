/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view.bsl.hh"
#include "eevee_camera_lib.bsl.hh"
#include "eevee_camera_shared.hh"
#include "eevee_defines.hh"
#include "eevee_velocity_shared.hh"
#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_math_matrix_transform.bsl.hh"

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
  /* U is periodic. */
  float4 equirect_motion_wrap(float4 motion) const
  {
    if (camera_curr.type == CAMERA_PANO_EQUIRECT) {
      const float period = abs(M_TAU * camera_curr.equirect_scale_inv.x);
      motion.xz -= period * round(motion.xz / period);
    }
    return motion;
  }

  /**
   * Given a triple of position, compute the previous and next motion vectors.
   * Returns uv space motion vectors in pairs (motion_prev.xy, motion_next.xy).
   */
  float4 surface_velocity(float3 P_prv, float3 P, float3 P_nxt) const
  {
    float2 prev_uv, curr_uv, next_uv;
    const bool panoramic = is_panoramic(camera_curr.type);
    if (panoramic) {
      /* Project through the panoramic mapping so the motion vector lands in the same warped film
       * space the accumulation buffer is stored in. */
      prev_uv = eevee::camera::uv_from_view(camera_prev,
                                            transform_point(camera_prev.viewmat, P_prv));
      curr_uv = eevee::camera::uv_from_view(camera_curr, transform_point(camera_curr.viewmat, P));
      next_uv = eevee::camera::uv_from_view(camera_next,
                                            transform_point(camera_next.viewmat, P_nxt));
    }
    else {
      /* NOTE: We use CameraData matrices instead of drw_view().persmat to avoid adding the TAA
       * jitter to the velocity. */
      prev_uv = project_point(camera_prev.persmat, P_prv).xy;
      curr_uv = project_point(camera_curr.persmat, P).xy;
      next_uv = project_point(camera_next.persmat, P_nxt).xy;
    }
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
    if (!panoramic) {
      /* Convert NDC velocity to UV velocity. `uv_from_view` above already returns UV space
       * directly for the panoramic case. */
      motion *= 0.5f;
    }
    else {
      motion = equirect_motion_wrap(motion);
    }

    return motion;
  }

  /**
   * Face-local variant of `surface_velocity()`, for `RenderBuffers::vector_tx`. Called from the
   * depth pre-pass with the true \a P_prv/\a P_nxt, and from `resolve_local()`'s static-geometry
   * fallback with \a P_prv == \a P == \a P_nxt.
   */
  float4 surface_velocity_local(const ViewMatrices &view,
                                float3 P_prv,
                                float3 P,
                                float3 P_nxt) const
  {
    if (!is_panoramic(camera_curr.type)) {
      return surface_velocity(P_prv, P, P_nxt);
    }
    float4x4 face_mat = view.viewmat * camera_curr.viewinv;
    float4x4 viewmat_prev = face_mat * camera_prev.viewmat;
    float4x4 viewmat_next = face_mat * camera_next.viewmat;

    float2 curr_ndc = project_point(view.winmat, transform_point(view.viewmat, P)).xy;
    float2 prev_ndc = project_point(view.winmat, transform_point(viewmat_prev, P_prv)).xy;
    float2 next_ndc = project_point(view.winmat, transform_point(viewmat_next, P_nxt)).xy;

    const float max_valid_ndc = 5.0f;
    if (any(isnan(prev_ndc)) || any(greaterThan(abs(prev_ndc), float2(max_valid_ndc)))) {
      prev_ndc = curr_ndc;
    }
    if (any(isnan(next_ndc)) || any(greaterThan(abs(next_ndc), float2(max_valid_ndc)))) {
      next_ndc = curr_ndc;
    }
    return float4(prev_ndc - curr_ndc, curr_ndc - next_ndc) * 0.5f;
  }

  /* World-space view direction. Must use `view`, which is this cubeface for panoramic camera. */
  float3 background_world_direction(const ViewMatrices &view, float2 uv) const
  {
    /* Use view vector to avoid imprecision if camera is far from origin. */
    float3 vV = -view.view_incident_vector(view.point_screen_to_view(float3(uv, 1.0f)));
    return transform_direction(view.viewinv, vV);
  }

  /**
   * Resolves `RenderBuffers::vector_tx` into this cubeface's own local NDC space. Real motion is
   * a plain unpack, already written by `surface_velocity_local()` in the depth pre-pass.
   * Background/no-data pixels fall back to reprojecting a static point/direction.
   */
  float4 resolve_local(const ViewMatrices &view, float4 vector_local, float2 uv, float depth) const
  {
    if (vector_local.x != VELOCITY_INVALID) {
      return velocity::unpack(vector_local);
    }
    if (depth == 1.0f) {
      float2 curr_ndc = uv * 2.0f - 1.0f;
      float4x4 face_mat = view.viewmat * camera_curr.viewinv;
      float4x4 viewmat_prev = face_mat * camera_prev.viewmat;
      float4x4 viewmat_next = face_mat * camera_next.viewmat;
      float3 V = background_world_direction(view, uv);
      float2 prev_ndc = project_point(view.winmat, transform_direction(viewmat_prev, V)).xy;
      float2 next_ndc = project_point(view.winmat, transform_direction(viewmat_next, V)).xy;

      const float max_valid_ndc = 5.0f;
      if (any(isnan(prev_ndc)) || any(greaterThan(abs(prev_ndc), float2(max_valid_ndc)))) {
        prev_ndc = curr_ndc;
      }
      if (any(isnan(next_ndc)) || any(greaterThan(abs(next_ndc), float2(max_valid_ndc)))) {
        next_ndc = curr_ndc;
      }
      return float4(prev_ndc - curr_ndc, curr_ndc - next_ndc) * 0.5f;
    }

    float3 P = view.point_screen_to_world(float3(uv, depth));
    return surface_velocity_local(view, P, P, P);
  }

  /**
   * Load and resolve correct velocity as some pixels might still not have correct
   * motion data for performance reasons.
   * Returns motion vector in this cubeface's own local NDC space.
   */
  float4 resolve_local(const draw::View &views, sampler2D vector_tx, int2 texel, float depth) const
  {
    const ViewMatrices view = views.get(0);
    float2 uv = (float2(texel) + 0.5f) / float2(textureSize(vector_tx, 0).xy);
    float4 vector = texelFetch(vector_tx, texel, 0);
    return resolve_local(view, vector, uv, depth);
  }

  /** Converts local NDC motion (see `resolve_local()`) to panoramic UV. */
  float4 local_to_panoramic(const ViewMatrices &view,
                            float4 local_motion,
                            float2 uv,
                            float depth) const
  {
    if (!is_panoramic(camera_curr.type)) {
      return local_motion;
    }
    /* Reconstruct world position from depth and reproject through each camera directly.
     * This cubeface's frustum only reaches 90 degrees. */
    float3 P = view.point_screen_to_world(float3(uv, depth));

    float4 camera_only_motion = surface_velocity_local(view, P, P, P);
    float4 object_motion_delta = (local_motion - camera_only_motion) * 2.0f;
    float2 curr_ndc = uv * 2.0f - 1.0f;
    float3 P_prv = view.point_screen_to_world(
        float3((curr_ndc + object_motion_delta.xy) * 0.5f + 0.5f, depth));
    float3 P_nxt = view.point_screen_to_world(
        float3((curr_ndc - object_motion_delta.zw) * 0.5f + 0.5f, depth));

    float3 curr_dir = normalize(transform_point(camera_curr.viewmat, P));
    float3 prev_dir = normalize(transform_point(camera_prev.viewmat, P_prv));
    float3 next_dir = normalize(transform_point(camera_next.viewmat, P_nxt));
    float2 curr_uv = eevee::camera::uv_from_view(camera_curr, curr_dir);
    float2 prev_uv = eevee::camera::uv_from_view(camera_prev, prev_dir);
    float2 next_uv = eevee::camera::uv_from_view(camera_next, next_dir);

    if (!panoramic_validate_uvs(curr_dir, curr_uv, prev_dir, prev_uv, next_dir, next_uv)) {
      return float4(0.0f);
    }
    return equirect_motion_wrap(float4(prev_uv - curr_uv, curr_uv - next_uv));
  }

  /**
   * Whether \a uv, from `uv_from_view(cam, dir)`, actually maps back to \a dir.
   */
  bool panoramic_uv_is_valid(CameraData cam, float3 dir, float2 uv) const
  {
    if (cam.type != CAMERA_PANO_FISHEYE_LENS_POLYNOMIAL) {
      return true;
    }
    /* Fisheye polynomial fits a wiggly curve that can't be solved directly, so we can only
     * approximate it with Newton's method. In certain cases, the numbers are way off, so need
     * to check if uv actually maps back to dir. */
    return dot(eevee::camera::view_from_uv(cam, uv), dir) > 0.99999f;
  }

  /** Shared by `local_to_panoramic()` and `resolve()`'s background case to whether the 3
   * reprojected uvs are in range and `panoramic_uv_is_valid()` for their camera. */
  bool panoramic_validate_uvs(float3 curr_dir,
                              float2 curr_uv,
                              float3 prev_dir,
                              float2 prev_uv,
                              float3 next_dir,
                              float2 next_uv) const
  {
    const float max_valid_uv = 5.0f;
    if (any(isnan(curr_uv)) || any(greaterThan(abs(curr_uv), float2(max_valid_uv))) ||
        any(isnan(prev_uv)) || any(greaterThan(abs(prev_uv), float2(max_valid_uv))) ||
        any(isnan(next_uv)) || any(greaterThan(abs(next_uv), float2(max_valid_uv))) ||
        !panoramic_uv_is_valid(camera_curr, curr_dir, curr_uv) ||
        !panoramic_uv_is_valid(camera_prev, prev_dir, prev_uv) ||
        !panoramic_uv_is_valid(camera_next, next_dir, next_uv))
    {
      return false;
    }
    return true;
  }

  /** Panoramic UV variant of `resolve_local()`. */
  float4 resolve_panoramic(const draw::View &views,
                           sampler2D vector_tx,
                           int2 texel,
                           float depth) const
  {
    const ViewMatrices view = views.get(0);
    float2 uv = (float2(texel) + 0.5f) / float2(textureSize(vector_tx, 0).xy);
    float4 vector = texelFetch(vector_tx, texel, 0);
    if (vector.x == VELOCITY_INVALID && depth == 1.0f) {
      /* Resolve background directly in panoramic space. */
      float3 V = background_world_direction(view, uv);
      float3 prev_dir = transform_direction(camera_prev.viewmat, V);
      float3 curr_dir = transform_direction(camera_curr.viewmat, V);
      float3 next_dir = transform_direction(camera_next.viewmat, V);
      float2 prev_uv = eevee::camera::uv_from_view(camera_prev, prev_dir);
      float2 curr_uv = eevee::camera::uv_from_view(camera_curr, curr_dir);
      float2 next_uv = eevee::camera::uv_from_view(camera_next, next_dir);
      if (!panoramic_validate_uvs(curr_dir, curr_uv, prev_dir, prev_uv, next_dir, next_uv)) {
        return float4(0.0f);
      }
      return equirect_motion_wrap(float4(prev_uv - curr_uv, curr_uv - next_uv));
    }
    float4 local_motion = resolve_local(view, vector, uv, depth);
    return local_to_panoramic(view, local_motion, uv, depth);
  }
};

/* -------------------------------------------------------------------- */

/* Pass world space deltas to the fragment shader.
 * This is to make sure that the resulting motion vectors are valid even with displacement.
 * WARNING: The next value is invalid when rendering the viewport. */
struct VertOutVelocity {
  [[smooth]] float3 prev;
  [[smooth]] float3 next;
};

/* Surface Velocity
 *
 * Combined with the depth pre-pass shader.
 * Outputs the view motion vectors for animated objects. */
struct GeometryVelocity {
  [[resource_table]] CameraVelocity camera;

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

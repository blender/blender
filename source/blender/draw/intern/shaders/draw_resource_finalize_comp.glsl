/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Finish computation of a few draw resource after sync.
 */

#include "draw_view_infos.hh"

#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

COMPUTE_SHADER_CREATE_INFO(draw_resource_finalize)

void main()
{
  uint resource_id = gl_GlobalInvocationID.x;
  if (resource_id >= uint(resource_len)) {
    return;
  }

  float4x4 model_mat = matrix_buf[resource_id].model;
  ObjectInfos infos = infos_buf[resource_id];
  ObjectBounds bounds = bounds_buf[resource_id];

  if (drw_bounds_corners_are_valid(bounds)) {
    /* Convert corners to origin + sides in world space. */
    float3 p0 = bounds.bounding_corners[0].xyz;
    float3 p01 = bounds.bounding_corners[1].xyz - p0;
    float3 p02 = bounds.bounding_corners[2].xyz - p0;
    float3 p03 = bounds.bounding_corners[3].xyz - p0;
    /* Avoid flat box. */
    p01.x = max(p01.x, 1e-4f);
    p02.y = max(p02.y, 1e-4f);
    p03.z = max(p03.z, 1e-4f);
    float3 diagonal = p01 + p02 + p03;
    float3 center = p0 + diagonal * 0.5f;
    float min_axis = reduce_min(abs(diagonal));
    bounds.bounding_sphere.xyz = transform_point(model_mat, center);
    /* We have to apply scaling to the diagonal. */
    bounds.bounding_sphere.w = length(transform_direction(model_mat, diagonal)) * 0.5f;
    bounds._inner_sphere_radius = min_axis;
    bounds.bounding_corners[0].xyz = transform_point(model_mat, p0);
    bounds.bounding_corners[1].xyz = transform_direction(model_mat, p01);
    bounds.bounding_corners[2].xyz = transform_direction(model_mat, p02);
    bounds.bounding_corners[3].xyz = transform_direction(model_mat, p03);
    /* Always have correct handedness in the corners vectors. */
    if (flag_test(infos.flag, OBJECT_NEGATIVE_SCALE)) {
      bounds.bounding_corners[0].xyz += bounds.bounding_corners[1].xyz;
      bounds.bounding_corners[1].xyz = -bounds.bounding_corners[1].xyz;
    }

    /* TODO: Bypass test for very large objects (see #67319). */
    if (bounds.bounding_sphere.w > 1e12f) {
      bounds.bounding_sphere.w = -2.0f;
    }

    /* Bypass culling test for objects that are flattened on one or more axes (see #127774).
     * Fixing them is too much computation but might be worth doing if a use case for it.
     * Do not compute the real length to save some instructions. */
    float3 object_scale = float3(reduce_add(abs(model_mat[0].xyz)),
                                 reduce_add(abs(model_mat[1].xyz)),
                                 reduce_add(abs(model_mat[2].xyz)));
    if (any(lessThan(abs(object_scale), float3(1e-10f)))) {
      bounds.bounding_sphere.w = -2.0f;
    }

    /* Update bounds. */
    bounds_buf[resource_id] = bounds;
  }

  float3 loc = infos.orco_add;  /* Box center. */
  float3 size = infos.orco_mul; /* Box half-extent. */
  float3 orco_mul = safe_rcp(size * 2.0f);
  float3 orco_add = (loc - size) * -orco_mul;
  infos_buf[resource_id].orco_add = orco_add;
  infos_buf[resource_id].orco_mul = orco_mul;
}

/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Finish computation of a few draw resource after sync.
 */

#pragma once

#include "draw_shader_shared.hh"
#include "gpu_shader_math_matrix_transform.bsl.hh"
#include "gpu_shader_math_vector_reduce.bsl.hh"
#include "gpu_shader_math_vector_safe.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

namespace draw {

struct ResourceFinalize {
  [[storage(0, read)]] ObjectMatrices (&matrix_buf)[];
  [[storage(1, read_write)]] ObjectBounds (&bounds_buf)[];
  [[storage(2, read_write)]] ObjectInfos (&infos_buf)[];

  [[push_constant]] int resource_len;
};

[[compute, local_size(DRW_VIEW_MAX)]] void resource_finalize_main(
    [[resource_table]] ResourceFinalize &srt, [[global_invocation_id]] const uint3 global_id)
{
  uint resource_id = global_id.x;
  if (resource_id >= uint(srt.resource_len)) {
    return;
  }

  float4x4 model_mat = srt.matrix_buf[resource_id].model;
  ObjectInfos infos = srt.infos_buf[resource_id];
  ObjectBounds bounds = srt.bounds_buf[resource_id];

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
    bounds.inner_sphere_radius_set(min_axis);
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
    srt.bounds_buf[resource_id] = bounds;
  }

  float3 loc = infos.orco_add;  /* Box center. */
  float3 size = infos.orco_mul; /* Box half-extent. */
  float3 orco_mul = safe_rcp(size * 2.0f);
  float3 orco_add = (loc - size) * -orco_mul;
  srt.infos_buf[resource_id].orco_add = orco_add;
  srt.infos_buf[resource_id].orco_mul = orco_mul;
}

PipelineCompute resource_finalize(resource_finalize_main);

}  // namespace draw

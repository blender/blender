/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Compute visibility of each resource bounds for a given view.
 */

/* TODO(fclem): This could be augmented by a 2 pass occlusion culling system. */

#pragma once

#include "draw_intersect.bsl.hh"

namespace draw {

struct Visibility {
  [[storage(0, read)]] ObjectBounds (&bounds_buf)[];
  [[storage(1, read_write)]] uint (&visibility_buf)[];

  [[push_constant]] int resource_len;
  [[push_constant]] int view_len;
  [[push_constant]] int visibility_word_per_draw;

  void mask_visibility_bit(const uint3 global_id,
                           const uint3 local_id,
                           const uint3 workgroup_id,
                           uint view_id)
  {
    if (view_len > 1) {
      uint index = global_id.x * uint(visibility_word_per_draw) + (view_id / 32u);
      visibility_buf[index] &= ~(1u << (view_id & 31u));
    }
    else {
      atomicAnd(visibility_buf[workgroup_id.x], ~(1u << local_id.x));
    }
  }
};

[[compute, local_size(DRW_VISIBILITY_GROUP_SIZE)]]
void visibility([[resource_table]] Visibility &srt,
                [[resource_table]] const ViewCulling &culling,
                [[global_invocation_id]] const uint3 global_id,
                [[local_invocation_id]] const uint3 local_id,
                [[work_group_id]] const uint3 workgroup_id)
{
  if (int(global_id.x) >= srt.resource_len) {
    return;
  }

  ObjectBounds bounds = srt.bounds_buf[global_id.x];

  if (drw_bounds_are_valid(bounds)) {
    IsectBox box = isect_box_setup(bounds.bounding_corners[0].xyz,
                                   bounds.bounding_corners[1].xyz,
                                   bounds.bounding_corners[2].xyz,
                                   bounds.bounding_corners[3].xyz);
    Sphere bounding_sphere = shape_sphere(bounds.bounding_sphere.xyz, bounds.bounding_sphere.w);
    Sphere inscribed_sphere = shape_sphere(bounds.bounding_sphere.xyz,
                                           bounds._inner_sphere_radius);

    for (uint view_id = 0u; view_id < uint(srt.view_len); view_id++) {
      if (culling.get(view_id).bound_sphere.w == -1.0f) {
        /* View disabled. */
        srt.mask_visibility_bit(global_id, local_id, workgroup_id, view_id);
      }
      else if (culling.intersect_view(inscribed_sphere, view_id) == true) {
        /* Visible. */
      }
      else if (culling.intersect_view(bounding_sphere, view_id) == false) {
        /* Not visible. */
        srt.mask_visibility_bit(global_id, local_id, workgroup_id, view_id);
      }
      else if (culling.intersect_view(box, view_id) == false) {
        /* Not visible. */
        srt.mask_visibility_bit(global_id, local_id, workgroup_id, view_id);
      }
    }
  }
  else {
    /* Culling is disabled, but we need to mask the bits for disabled views. */
    for (uint view_id = 0u; view_id < uint(srt.view_len); view_id++) {
      if (culling.get(view_id).bound_sphere.w == -1.0f) {
        /* View disabled. */
        srt.mask_visibility_bit(global_id, local_id, workgroup_id, view_id);
      }
    }
  }
}

PipelineCompute visibility_compute(visibility);

}  // namespace draw

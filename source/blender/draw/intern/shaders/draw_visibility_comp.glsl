/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute visibility of each resource bounds for a given view.
 */
/* TODO(fclem): This could be augmented by a 2 pass occlusion culling system. */

#include "draw_view_infos.hh"

#include "draw_intersect_lib.glsl"

COMPUTE_SHADER_CREATE_INFO(draw_visibility_compute)

void mask_visibility_bit(uint view_id)
{
  if (view_len > 1) {
    uint index = gl_GlobalInvocationID.x * uint(visibility_word_per_draw) + (view_id / 32u);
    visibility_buf[index] &= ~(1u << view_id);
  }
  else {
    atomicAnd(visibility_buf[gl_WorkGroupID.x], ~(1u << gl_LocalInvocationID.x));
  }
}

void main()
{
  if (int(gl_GlobalInvocationID.x) >= resource_len) {
    return;
  }

  ObjectBounds bounds = bounds_buf[gl_GlobalInvocationID.x];

  if (drw_bounds_are_valid(bounds)) {
    IsectBox box = isect_box_setup(bounds.bounding_corners[0].xyz,
                                   bounds.bounding_corners[1].xyz,
                                   bounds.bounding_corners[2].xyz,
                                   bounds.bounding_corners[3].xyz);
    Sphere bounding_sphere = shape_sphere(bounds.bounding_sphere.xyz, bounds.bounding_sphere.w);
    Sphere inscribed_sphere = shape_sphere(bounds.bounding_sphere.xyz,
                                           bounds._inner_sphere_radius);

    for (uint view_id = 0u; view_id < uint(view_len); view_id++) {
      if (drw_view_culling(view_id).bound_sphere.w == -1.0f) {
        /* View disabled. */
        mask_visibility_bit(view_id);
      }
      else if (intersect_view(inscribed_sphere, view_id) == true) {
        /* Visible. */
      }
      else if (intersect_view(bounding_sphere, view_id) == false) {
        /* Not visible. */
        mask_visibility_bit(view_id);
      }
      else if (intersect_view(box, view_id) == false) {
        /* Not visible. */
        mask_visibility_bit(view_id);
      }
    }
  }
  else {
    /* Culling is disabled, but we need to mask the bits for disabled views. */
    for (uint view_id = 0u; view_id < uint(view_len); view_id++) {
      if (drw_view_culling(view_id).bound_sphere.w == -1.0f) {
        /* View disabled. */
        mask_visibility_bit(view_id);
      }
    }
  }
}

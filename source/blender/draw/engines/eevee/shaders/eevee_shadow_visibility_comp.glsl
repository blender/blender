/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute visibility of each resource bounds for a given shadow view.
 * Checks for shadow linking properties and issue one draw call for each view.
 */
/* TODO(fclem): Could reject bounding boxes that are covering only invalid tiles. */

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_shadow_view_visibility)

#include "draw_intersect_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

bool shadow_linking_affects_caster(uint view_id, uint resource_id)
{
  ObjectInfos object_infos = drw_infos[resource_id];
  return bitmask64_test(render_view_buf[view_id].shadow_set_membership,
                        blocker_shadow_set_get(object_infos));
}

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

/* Returns true if visibility needs to be disabled. */
bool non_culling_tests(uint view_id, uint resource_id)
{
  if (shadow_linking_affects_caster(view_id, resource_id) == false) {
    /* Object doesn't cast shadow from this light. */
    return true;
  }
  else if (drw_view_culling().bound_sphere.w == -1.0f) {
    /* View disabled. */
    return true;
  }
  return false;
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

    for (drw_view_id = 0u; drw_view_id < uint(view_len); drw_view_id++) {
      if (non_culling_tests(drw_view_id, gl_GlobalInvocationID.x)) {
        mask_visibility_bit(drw_view_id);
      }
      else if (drw_view_culling().bound_sphere.w == -1.0f) {
        /* View disabled. */
        mask_visibility_bit(drw_view_id);
      }
      else if (intersect_view(inscribed_sphere) == true) {
        /* Visible. */
      }
      else if (intersect_view(bounding_sphere) == false) {
        /* Not visible. */
        mask_visibility_bit(drw_view_id);
      }
      else if (intersect_view(box) == false) {
        /* Not visible. */
        mask_visibility_bit(drw_view_id);
      }
    }
  }
  else {
    /* Culling is disabled, but we need to mask the bits for disabled views. */
    for (drw_view_id = 0u; drw_view_id < uint(view_len); drw_view_id++) {
      if (non_culling_tests(drw_view_id, gl_GlobalInvocationID.x)) {
        mask_visibility_bit(drw_view_id);
      }
    }
  }
}

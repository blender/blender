/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute visibility of each resource bounds for a given shadow view.
 * Checks for shadow linking properties and issue one draw call for each view.
 */
/* TODO(fclem): Could reject bounding boxes that are covering only invalid tiles. */

#pragma once

#include "infos/eevee_common_infos.hh"

COMPUTE_SHADER_CREATE_INFO(draw_view)
COMPUTE_SHADER_CREATE_INFO(draw_view_culling)
COMPUTE_SHADER_CREATE_INFO(draw_object_infos)

#include "draw_intersect_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

#include "eevee_defines.hh"
#include "eevee_shadow_shared.hh"

namespace eevee::shadow {

struct ViewVisibility {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo draw_view_culling;
  [[legacy_info]] ShaderCreateInfo draw_object_infos;

  [[storage(0, read)]] const ObjectBounds (&bounds_buf)[];
  [[storage(1, read_write)]] uint (&visibility_buf)[];
  [[storage(2, read)]] const ShadowRenderView (&render_view_buf)[SHADOW_VIEW_MAX];

  [[push_constant]] const int resource_len;
  [[push_constant]] const int view_len;
  [[push_constant]] const int visibility_word_per_draw;

  bool shadow_linking_affects_caster(uint view_id, uint resource_id)
  {
    const auto &infos_buf = buffer_get(draw_object_infos, drw_infos);
    ObjectInfos object_infos = infos_buf[resource_id];
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
    if (drw_view_culling(view_id).bound_sphere.w == -1.0f) {
      /* View disabled. */
      return true;
    }
    return false;
  }
};

[[compute, local_size(DRW_VISIBILITY_GROUP_SIZE)]]
void comp_main([[resource_table]] ViewVisibility &srt)
{
  if (int(gl_GlobalInvocationID.x) >= srt.resource_len) {
    return;
  }

  ObjectBounds bounds = srt.bounds_buf[gl_GlobalInvocationID.x];

  if (drw_bounds_are_valid(bounds)) {
    IsectBox box = isect_box_setup(bounds.bounding_corners[0].xyz,
                                   bounds.bounding_corners[1].xyz,
                                   bounds.bounding_corners[2].xyz,
                                   bounds.bounding_corners[3].xyz);
    Sphere bounding_sphere = shape_sphere(bounds.bounding_sphere.xyz, bounds.bounding_sphere.w);
    Sphere inscribed_sphere = shape_sphere(bounds.bounding_sphere.xyz,
                                           bounds._inner_sphere_radius);

    for (uint view_id = 0u; view_id < uint(srt.view_len); view_id++) {
      if (srt.non_culling_tests(view_id, gl_GlobalInvocationID.x)) {
        srt.mask_visibility_bit(view_id);
      }
      else if (drw_view_culling(view_id).bound_sphere.w == -1.0f) {
        /* View disabled. */
        srt.mask_visibility_bit(view_id);
      }
      else if (intersect_view(inscribed_sphere, view_id) == true) {
        /* Visible. */
      }
      else if (intersect_view(bounding_sphere, view_id) == false) {
        /* Not visible. */
        srt.mask_visibility_bit(view_id);
      }
      else if (intersect_view(box, view_id) == false) {
        /* Not visible. */
        srt.mask_visibility_bit(view_id);
      }
    }
  }
  else {
    /* Culling is disabled, but we need to mask the bits for disabled views. */
    for (uint view_id = 0u; view_id < uint(srt.view_len); view_id++) {
      if (srt.non_culling_tests(view_id, gl_GlobalInvocationID.x)) {
        srt.mask_visibility_bit(view_id);
      }
    }
  }
}

PipelineCompute view_visibility(eevee::shadow::comp_main);

}  // namespace eevee::shadow

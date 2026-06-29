/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute visibility of each resource bounds for a given shadow view.
 * Checks for shadow linking properties and issue one draw call for each view.
 */
/* TODO(fclem): Could reject bounding boxes that are covering only invalid tiles. */

#pragma once

#include "draw_view_infos.hh"

COMPUTE_SHADER_CREATE_INFO(draw_view_culling)

#include "draw_intersect_lib.glsl"
#include "draw_model.bsl.hh"
#include "gpu_shader_utildefines_lib.glsl"

#include "eevee_defines.hh"
#include "eevee_shadow_shared.hh"

namespace eevee::shadow {

struct ViewVisibility {
  [[legacy_info]] ShaderCreateInfo draw_view_culling;

  [[resource_table]] srt_t<draw::Infos> infos_;

  [[storage(0, read)]] const ObjectBounds (&bounds_buf)[];
  [[storage(1, read_write)]] uint (&visibility_buf)[];
  [[storage(2, read)]] const ShadowRenderView (&render_view_buf)[SHADOW_VIEW_MAX];

  [[push_constant]] const int resource_len;

  bool shadow_linking_affects_caster(uint view_id, uint resource_id)
  {
    [[resource_table]] draw::Infos infos = infos_;
    ObjectInfos object_infos = infos.get(resource_id);
    return bitmask64_test(render_view_buf[view_id].shadow_set_membership,
                          blocker_shadow_set_get(object_infos));
  }

  void mask_visibility_bit(uint view_id, uint3 global_id)
  {
    constexpr uint visibility_word_per_draw = uint(SHADOW_VIEW_MAX) / 32u;
    uint index = global_id.x * visibility_word_per_draw + (view_id / 32u);
    visibility_buf[index] &= ~(1u << (view_id & 31u));
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
void comp_main([[resource_table]] ViewVisibility &srt,
               [[global_invocation_id]] const uint3 global_id)
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

    for (uint view_id = 0u; view_id < uint(SHADOW_VIEW_MAX); view_id++) {
      if (srt.non_culling_tests(view_id, global_id.x)) {
        srt.mask_visibility_bit(view_id, global_id);
      }
      else if (drw_view_culling(view_id).bound_sphere.w == -1.0f) {
        /* View disabled. */
        srt.mask_visibility_bit(view_id, global_id);
      }
      else if (intersect_view(inscribed_sphere, view_id) == true) {
        /* Visible. */
      }
      else if (intersect_view(bounding_sphere, view_id) == false) {
        /* Not visible. */
        srt.mask_visibility_bit(view_id, global_id);
      }
      else if (intersect_view(box, view_id) == false) {
        /* Not visible. */
        srt.mask_visibility_bit(view_id, global_id);
      }
    }
  }
  else {
    /* Culling is disabled, but we need to mask the bits for disabled views. */
    for (uint view_id = 0u; view_id < uint(SHADOW_VIEW_MAX); view_id++) {
      if (srt.non_culling_tests(view_id, global_id.x)) {
        srt.mask_visibility_bit(view_id, global_id);
      }
    }
  }
}

PipelineCompute view_visibility(eevee::shadow::comp_main);

}  // namespace eevee::shadow

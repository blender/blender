/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * TODO(fclem): Merge with workbench_shadow.bsl.hh once draw_model_lib.glsl has been ported.
 */

#pragma once

#include "draw_view_infos.hh"
#include "gpu_index_load_infos.hh"

#include "draw_intersect_lib.glsl"
#include "workbench_shader_shared.hh"

namespace workbench::shadow::visibility {

struct Resources {
  [[compilation_constant]] const bool dynamic_pass_selection;

  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo draw_view_culling;
  [[storage(0, read)]] const ObjectBounds (&bounds_buf)[];

  [[push_constant]] const int resource_len;
  [[push_constant]] const int view_len;

  [[push_constant]] const bool force_fail_method;
  [[push_constant]] const float3 shadow_direction;

  [[uniform(2)]] const ExtrudedFrustum &extruded_frustum;

  [[storage(1, read_write), condition(!dynamic_pass_selection)]] uint (&visibility_buf)[];

  [[storage(1, read_write), condition(dynamic_pass_selection)]] uint (&pass_visibility_buf)[];
  [[storage(2, read_write), condition(dynamic_pass_selection)]] uint (&fail_visibility_buf)[];

  bool is_visible(IsectBox box)
  {
    for (int i_plane = 0; i_plane < extruded_frustum.planes_count; i_plane++) {
      float4 plane = extruded_frustum.planes[i_plane];
      bool separating_axis = true;
      for (int i_corner = 0; i_corner < 8; i_corner++) {
        float signed_distance = dot(box.corners[i_corner], plane.xyz) - plane.w;
        if (signed_distance <= 0) {
          separating_axis = false;
          break;
        }
      }
      if (separating_axis) {
        return false;
      }
    }
    return true;
  }

  bool intersects_near_plane(IsectBox box)
  {
    float4 near_plane = drw_view_culling().frustum_planes.planes[4];
    bool on_positive_side = false;
    bool on_negative_side = false;

    for (int i_corner = 0; i_corner < 8; i_corner++) {
      for (int i_displace = 0; i_displace < 2; i_displace++) {
        float3 corner = box.corners[i_corner] + (shadow_direction * 1e5f * i_displace);
        float signed_distance = dot(corner, -near_plane.xyz) - near_plane.w;
        if (signed_distance <= 0) {
          on_negative_side = true;
        }
        else {
          on_positive_side = true;
        }
        if (on_negative_side && on_positive_side) {
          return true;
        }
      }
    }

    return false;
  }

  void set_visibility(bool pass, bool fail, uint instance_id)
  {
    if (dynamic_pass_selection) [[static_branch]] {
      if (!pass) {
        atomicAnd(pass_visibility_buf[instance_id / 32u], ~(1u << (instance_id & 31u)));
      }
      if (!fail) {
        atomicAnd(fail_visibility_buf[instance_id / 32u], ~(1u << (instance_id & 31u)));
      }
    }
  }

  void set_visibility(bool visibility, uint instance_id)
  {
    if (dynamic_pass_selection == false) [[static_branch]] {
      if (!visibility) {
        atomicAnd(visibility_buf[instance_id / 32u], ~(1u << (instance_id & 31u)));
      }
    }
  }
};

[[compute, local_size(DRW_VISIBILITY_GROUP_SIZE)]]
void culling_main([[resource_table]] Resources &srt,
                  [[global_invocation_id]] const uint3 global_id)
{
  if (int(global_id.x) >= srt.resource_len) {
    return;
  }

  ObjectBounds bounds = srt.bounds_buf[global_id.x];
  if (!drw_bounds_are_valid(bounds)) {
    /* Invalid bounding box. */
    return;
  }
  IsectBox box = isect_box_setup(bounds.bounding_corners[0].xyz,
                                 bounds.bounding_corners[1].xyz,
                                 bounds.bounding_corners[2].xyz,
                                 bounds.bounding_corners[3].xyz);

  if (srt.dynamic_pass_selection) [[static_branch]] {
    if (srt.is_visible(box)) {
      bool use_fail_pass = srt.force_fail_method || srt.intersects_near_plane(box);
      srt.set_visibility(!use_fail_pass, use_fail_pass, global_id.x);
    }
    else {
      srt.set_visibility(false, false, global_id.x);
    }
  }
  else {
    srt.set_visibility(srt.is_visible(box), global_id.x);
  }
}

#ifndef GLSL_CPP_STUBS
PipelineCompute compute_dynamic_pass_type(culling_main, Resources{.dynamic_pass_selection = true});
PipelineCompute compute_static_pass_type(culling_main, Resources{.dynamic_pass_selection = false});
#endif

}  // namespace workbench::shadow::visibility

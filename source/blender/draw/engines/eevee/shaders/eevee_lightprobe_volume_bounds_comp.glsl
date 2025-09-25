/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Surface Capture: Output surface parameters to diverse storage.
 *
 * The resources expected to be defined are:
 * - capture_info_buf
 */

#include "infos/eevee_lightprobe_volume_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_lightprobe_volume_bounds)

#include "draw_intersect_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void main()
{
  uint index = gl_GlobalInvocationID.x;
  if (index >= uint(resource_len)) {
    return;
  }

  ObjectBounds bounds = bounds_buf[index];
  if (!drw_bounds_are_valid(bounds)) {
    return;
  }

  IsectBox box = isect_box_setup(bounds.bounding_corners[0].xyz,
                                 bounds.bounding_corners[1].xyz,
                                 bounds.bounding_corners[2].xyz,
                                 bounds.bounding_corners[3].xyz);

  float3 local_min = float3(FLT_MAX);
  float3 local_max = float3(-FLT_MAX);
  for (int i = 0; i < 8; i++) {
    local_min = min(local_min, box.corners[i].xyz);
    local_max = max(local_max, box.corners[i].xyz);
  }

  atomicMin(capture_info_buf.scene_bound_x_min, floatBitsToOrderedInt(local_min.x));
  atomicMax(capture_info_buf.scene_bound_x_max, floatBitsToOrderedInt(local_max.x));

  atomicMin(capture_info_buf.scene_bound_y_min, floatBitsToOrderedInt(local_min.y));
  atomicMax(capture_info_buf.scene_bound_y_max, floatBitsToOrderedInt(local_max.y));

  atomicMin(capture_info_buf.scene_bound_z_min, floatBitsToOrderedInt(local_min.z));
  atomicMax(capture_info_buf.scene_bound_z_max, floatBitsToOrderedInt(local_max.z));
}

/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * GPU generated indirection buffer. Updated on topology change.
 * One thread processes one curve.
 */

#include "draw_curves_infos.hh"

COMPUTE_SHADER_CREATE_INFO(draw_curves_topology)

#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_offset_indices_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void main()
{
  if (gl_GlobalInvocationID.x >= uint(curves_count)) {
    return;
  }
  uint curve_id = gl_GlobalInvocationID.x + uint(curves_start);

  bool is_curve_cyclic = false;
  if (use_cyclic) {
    is_curve_cyclic = gpu_attr_load_bool(curves_cyclic_buf, curve_id);
  }

  IndexRange points = offset_indices::load_range_from_buffer(evaluated_offsets_buf, curve_id);
  int index_start = points.start();
  int num_segment = points.size();

  if (use_cyclic) {
    index_start += int(curve_id);
    num_segment += 1;
  }

  constexpr int cyclic_endpoint_pivot = INT_MAX / 2;
  constexpr int end_of_curve = INT_MAX;

  int indirection_index_count = num_segment + (is_ribbon_topology ? 1 : -1);
  index_start += int(is_ribbon_topology ? curve_id : -curve_id);

  for (int i = 0; i < indirection_index_count; i++) {
    int value = int((i == 0) ? curve_id : -i);

    bool is_restart = false;
    bool is_cyclic_last_segment = false;

    if (use_cyclic) {
      if (is_ribbon_topology) {
        is_cyclic_last_segment = i == indirection_index_count - 2;
        if (is_curve_cyclic) {
          is_restart = i == indirection_index_count - 1;
        }
        else {
          is_restart = i >= indirection_index_count - 2;
        }
      }
      else {
        is_cyclic_last_segment = i == indirection_index_count - 1;
        if (is_curve_cyclic) {
          is_restart = false;
        }
        else {
          is_restart = i == indirection_index_count - 1;
        }
      }
    }
    else {
      if (is_ribbon_topology) {
        is_restart = i == indirection_index_count - 1;
      }
      else {
        is_restart = false;
      }
    }

    indirection_buf[index_start + i] = is_restart ? end_of_curve :
                                                    (is_cyclic_last_segment ?
                                                         value - cyclic_endpoint_pivot :
                                                         value);
  }
}

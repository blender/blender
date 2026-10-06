/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * GPU generated indirection buffer. Updated on topology change.
 * One thread processes one curve.
 */

#pragma once

#include "gpu_shader_attribute_load.bsl.hh"
#include "gpu_shader_offset_indices_lib.glsl"

namespace curves {

struct Topology {
  /* Offsets giving the start and end of the curve. */
  [[storage(0, read)]] int (&evaluated_offsets_buf)[];
  [[storage(1, read)]] uint (&curves_cyclic_buf)[]; /* Actually bool (1 byte). */
  [[storage(2, write)]] int (&indirection_buf)[];
  [[push_constant]] int curves_start;
  [[push_constant]] int curves_count;
  [[push_constant]] bool is_ribbon_topology;
  [[push_constant]] bool use_cyclic;
};

[[compute, local_size(CURVES_PER_THREADGROUP)]] void eval_topology(
    [[resource_table]] Topology &srt, [[global_invocation_id]] const uint3 global_id)
{
  if (global_id.x >= uint(srt.curves_count)) {
    return;
  }
  uint curve_id = global_id.x + uint(srt.curves_start);

  bool is_curve_cyclic = false;
  if (srt.use_cyclic) {
    /* Note: Quirk of the force_inline implementation. */
    bool value = gpu_attr_load_bool(srt.curves_cyclic_buf, curve_id);
    is_curve_cyclic = value;
  }

  IndexRange points = offset_indices::load_range_from_buffer(srt.evaluated_offsets_buf,
                                                             int(curve_id));
  int index_start = points.start();
  int num_segment = points.size();

  if (srt.use_cyclic) {
    index_start += int(curve_id);
    num_segment += 1;
  }

  constexpr int cyclic_endpoint_pivot = INT_MAX / 2;
  constexpr int end_of_curve = INT_MAX;

  int indirection_index_count = num_segment + (srt.is_ribbon_topology ? 1 : -1);
  index_start += int(srt.is_ribbon_topology ? curve_id : -curve_id);

  for (int i = 0; i < indirection_index_count; i++) {
    int value = (i == 0) ? int(curve_id) : -i;

    bool is_restart = false;
    bool is_cyclic_last_segment = false;

    if (srt.use_cyclic) {
      if (srt.is_ribbon_topology) {
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
      if (srt.is_ribbon_topology) {
        is_restart = i == indirection_index_count - 1;
      }
      else {
        is_restart = false;
      }
    }

    srt.indirection_buf[index_start + i] = is_restart ? end_of_curve :
                                                        (is_cyclic_last_segment ?
                                                             value - cyclic_endpoint_pivot :
                                                             value);
  }
}

}  // namespace curves

PipelineCompute draw_curves_topology(curves::eval_topology);

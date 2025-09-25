/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_velocity_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_vertex_copy)

#include "gpu_shader_math_base_lib.glsl"

void main()
{
  uint vertices_per_thread = divide_ceil(uint(vertex_count), uint(gl_WorkGroupSize.x)) /
                             gl_NumWorkGroups.x;
  uint vertex_start = min(gl_GlobalInvocationID.x * vertices_per_thread, uint(vertex_count));
  uint vertex_end = min(vertex_start + vertices_per_thread, uint(vertex_count));

  for (uint vertex_id = vertex_start; vertex_id < vertex_end; vertex_id++) {
    out_buf[start_offset + vertex_id] = float4(in_buf[vertex_id * vertex_stride + 0],
                                               in_buf[vertex_id * vertex_stride + 1],
                                               in_buf[vertex_id * vertex_stride + 2],
                                               1.0f);
  }
}

/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  uint vertex_id = gl_GlobalInvocationID.x;
  if (vertex_id >= vertex_count) {
    return;
  }
  out_buf[start_offset + vertex_id] = vec4(in_buf[vertex_id * vertex_stride + 0],
                                           in_buf[vertex_id * vertex_stride + 1],
                                           in_buf[vertex_id * vertex_stride + 2],
                                           1.0);
}

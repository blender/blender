/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"

#include "GEO_mesh_primitive_line.hh"

namespace blender::geometry {

Mesh *create_line_mesh(const float3 start, const float3 delta, const int count)
{
  if (count < 1) {
    return nullptr;
  }

  Mesh *mesh = BKE_mesh_new_nomain(count, count - 1, 0, 0);
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<int2> edges = mesh->edges_for_write();

  threading::parallel_invoke(
      1024 < count,
      [&]() {
        threading::parallel_for(positions.index_range(), 4096, [&](IndexRange range) {
          for (const int i : range) {
            positions[i] = start + delta * i;
          }
        });
      },
      [&]() {
        threading::parallel_for(edges.index_range(), 4096, [&](IndexRange range) {
          for (const int i : range) {
            edges[i][0] = i;
            edges[i][1] = i + 1;
          }
        });
      });

  return mesh;
}

}  // namespace blender::geometry

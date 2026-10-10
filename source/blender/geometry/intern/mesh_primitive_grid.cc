/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup geo
 */

#include "BKE_attribute.hh"
#include "BKE_mesh.hh"

#include "GEO_mesh_primitive_grid.hh"

namespace blender::geometry {

static constexpr int64_t grain_size = 4096;

static void fill_position_column(float3 *__restrict positions,
                                 const int verts_y,
                                 const float x_pos,
                                 const float y_shift,
                                 const float dy)
{
  for (int y = 0; y < verts_y; y++) {
    positions[y] = float3(x_pos, (y - y_shift) * dy, 0.0f);
  }
}

BLI_NOINLINE static void calculate_positions(const int verts_x,
                                             const int verts_y,
                                             const float size_x,
                                             const float size_y,
                                             MutableSpan<float3> positions)
{
  const int edges_x = verts_x - 1;
  const int edges_y = verts_y - 1;
  const float dx = edges_x == 0 ? 0.0f : size_x / edges_x;
  const float dy = edges_y == 0 ? 0.0f : size_y / edges_y;
  const float x_shift = edges_x / 2.0f;
  const float y_shift = edges_y / 2.0f;
  const int64_t x_grain = std::max<int64_t>(1, grain_size / std::max(verts_y, 1));

  threading::parallel_for(IndexRange(verts_x), x_grain, [&](const IndexRange x_range) {
    for (const int x : x_range) {
      fill_position_column(
          positions.data() + x * verts_y, verts_y, (x - x_shift) * dx, y_shift, dy);
    }
  });
}

static void fill_y_edge_strip(int *__restrict edge_ints, const int edges_y)
{
  for (int y = 0; y < edges_y; y++) {
    edge_ints[2 * y + 0] = y;
    edge_ints[2 * y + 1] = y + 1;
  }
}

static void add_int_offset(int *__restrict dst,
                           const int *__restrict src,
                           const int ints_num,
                           const int offset)
{
  for (int i = 0; i < ints_num; i++) {
    dst[i] = src[i] + offset;
  }
}

BLI_NOINLINE static void calculate_y_edges(const int verts_x,
                                           const int verts_y,
                                           MutableSpan<int2> edges)
{
  const int edges_y = verts_y - 1;
  if (edges_y < 1) {
    return;
  }

  int *edge_ints = reinterpret_cast<int *>(edges.data());
  fill_y_edge_strip(edge_ints, edges_y);

  if (verts_x <= 1) {
    return;
  }

  const int ints_num = edges_y * 2;
  const int64_t x_grain = std::max<int64_t>(1, grain_size / std::max(edges_y, 1));
  threading::parallel_for(IndexRange(1, verts_x - 1), x_grain, [&](const IndexRange x_range) {
    for (const int x : x_range) {
      add_int_offset(edge_ints + x * ints_num, edge_ints, ints_num, x * verts_y);
    }
  });
}

static void fill_x_edge_strip(int *__restrict edge_ints, const int edges_x, const int verts_y)
{
  for (int x = 0; x < edges_x; x++) {
    const int vert = x * verts_y;
    edge_ints[2 * x + 0] = vert;
    edge_ints[2 * x + 1] = vert + verts_y;
  }
}

BLI_NOINLINE static void calculate_x_edges(const int verts_x,
                                           const int verts_y,
                                           MutableSpan<int2> edges)
{
  const int edges_x = verts_x - 1;
  const int edges_y = verts_y - 1;
  if (edges_x < 1) {
    return;
  }

  int *edge_ints = reinterpret_cast<int *>(edges.data() + verts_x * edges_y);
  fill_x_edge_strip(edge_ints, edges_x, verts_y);

  if (verts_y <= 1) {
    return;
  }

  const int ints_num = edges_x * 2;
  const int64_t y_grain = std::max<int64_t>(1, grain_size / std::max(edges_x, 1));
  threading::parallel_for(IndexRange(1, verts_y - 1), y_grain, [&](const IndexRange y_range) {
    for (const int y : y_range) {
      add_int_offset(edge_ints + y * ints_num, edge_ints, ints_num, y);
    }
  });
}

static void fill_corner_column(int *__restrict corner_verts,
                               int *__restrict corner_edges,
                               const int edges_y,
                               const int edges_x,
                               const int x,
                               const int vert_column,
                               const int vert_column_next,
                               const int x_edges_start,
                               const int y_edges_col,
                               const int y_edges_col_next)
{
  for (int y = 0; y < edges_y; y++) {
    const int vert = vert_column + y;
    corner_verts[4 * y + 0] = vert;
    corner_verts[4 * y + 1] = vert_column_next + y;
    corner_verts[4 * y + 2] = vert_column_next + y + 1;
    corner_verts[4 * y + 3] = vert + 1;

    corner_edges[4 * y + 0] = x_edges_start + edges_x * y + x;
    corner_edges[4 * y + 1] = y_edges_col_next + y;
    corner_edges[4 * y + 2] = x_edges_start + edges_x * (y + 1) + x;
    corner_edges[4 * y + 3] = y_edges_col + y;
  }
}

BLI_NOINLINE static void calculate_corners(const int verts_x,
                                           const int verts_y,
                                           MutableSpan<int> corner_verts,
                                           MutableSpan<int> corner_edges)
{
  const int edges_x = verts_x - 1;
  const int edges_y = verts_y - 1;
  if (edges_x < 1 || edges_y < 1) {
    return;
  }

  const int x_edges_start = verts_x * edges_y;
  const int64_t x_grain = std::max<int64_t>(1, grain_size / std::max(edges_y, 1));

  threading::parallel_for(IndexRange(edges_x), x_grain, [&](const IndexRange x_range) {
    for (const int x : x_range) {
      const int face_offset = x * edges_y;
      fill_corner_column(corner_verts.data() + face_offset * 4,
                         corner_edges.data() + face_offset * 4,
                         edges_y,
                         edges_x,
                         x,
                         x * verts_y,
                         (x + 1) * verts_y,
                         x_edges_start,
                         edges_y * x,
                         edges_y * (x + 1));
    }
  });
}

static void fill_uv_column(float2 *__restrict uvs,
                           const int edges_y,
                           const float u,
                           const float u_next,
                           const float v_scale)
{
  for (int y = 0; y < edges_y; y++) {
    const float v = float(y) * v_scale;
    const float v_next = float(y + 1) * v_scale;
    uvs[4 * y + 0] = float2(u, v);
    uvs[4 * y + 1] = float2(u_next, v);
    uvs[4 * y + 2] = float2(u_next, v_next);
    uvs[4 * y + 3] = float2(u, v_next);
  }
}

BLI_NOINLINE static void calculate_uvs(const int verts_x,
                                       const int verts_y,
                                       const float size_x,
                                       const float size_y,
                                       MutableSpan<float2> uvs)
{
  const int edges_x = verts_x - 1;
  const int edges_y = verts_y - 1;
  const float u_scale = (size_x == 0.0f) ? 0.0f : 1.0f / float(edges_x);
  const float v_scale = (size_y == 0.0f) ? 0.0f : 1.0f / float(edges_y);
  const int64_t x_grain = std::max<int64_t>(1, grain_size / std::max(edges_y, 1));

  threading::parallel_for(IndexRange(edges_x), x_grain, [&](const IndexRange x_range) {
    for (const int x : x_range) {
      fill_uv_column(uvs.data() + x * edges_y * 4,
                     edges_y,
                     float(x) * u_scale,
                     float(x + 1) * u_scale,
                     v_scale);
    }
  });
}

Mesh *create_grid_mesh(const int verts_x,
                       const int verts_y,
                       const float size_x,
                       const float size_y,
                       const std::optional<StringRef> uv_map_id)
{
  BLI_assert(verts_x > 0 && verts_y > 0);
  const int edges_x = verts_x - 1;
  const int edges_y = verts_y - 1;
  const int verts_num = verts_x * verts_y;
  const int edges_num = edges_x * verts_y + edges_y * verts_x;
  const int faces_num = edges_x * edges_y;
  const int corners_num = faces_num * 4;

  Mesh *mesh = BKE_mesh_new_nomain(verts_num, edges_num, faces_num, corners_num);
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<int2> edges = mesh->edges_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();
  MutableSpan<int> corner_edges = mesh->corner_edges_for_write();
  bke::mesh_smooth_set(*mesh, false);

  bke::SpanAttributeWriter<float2> uv_attribute;
  if (uv_map_id && faces_num != 0) {
    uv_attribute = mesh->attributes_for_write().lookup_or_add_for_write_only_span<float2>(
        *uv_map_id, bke::AttrDomain::Corner);
  }

  const bool use_threading = verts_num > grain_size;
  const int64_t bytes_touched = positions.size_in_bytes() + edges.size_in_bytes() +
                                corner_verts.size_in_bytes() + corner_edges.size_in_bytes() +
                                (uv_attribute ? uv_attribute.span.size_in_bytes() : 0);

  threading::memory_bandwidth_bound_task(bytes_touched, [&]() {
    threading::parallel_invoke(
        use_threading,
        [&]() { offset_indices::fill_constant_group_size(4, 0, mesh->face_offsets_for_write()); },
        [&]() { calculate_positions(verts_x, verts_y, size_x, size_y, positions); },
        [&]() { calculate_y_edges(verts_x, verts_y, edges); },
        [&]() { calculate_x_edges(verts_x, verts_y, edges); },
        [&]() { calculate_corners(verts_x, verts_y, corner_verts, corner_edges); },
        [&]() {
          if (uv_attribute) {
            calculate_uvs(verts_x, verts_y, size_x, size_y, uv_attribute.span);
          }
        });
  });

  if (uv_attribute) {
    uv_attribute.finish();
  }

  if (verts_x > 1 || verts_y > 1) {
    mesh->tag_loose_verts_none();
  }
  if (verts_x > 1 && verts_y > 1) {
    mesh->tag_loose_edges_none();
  }
  mesh->tag_overlapping_none();

  const float3 bounds = float3(size_x * 0.5f, size_y * 0.5f, 0.0f);
  mesh->bounds_set_eager({-bounds, bounds});

  return mesh;
}

}  // namespace blender::geometry

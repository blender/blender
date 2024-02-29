/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>
#include <fmt/format.h>

#include "GEO_reverse_uv_sampler.hh"

#include "BLI_bounds.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_mask.hh"
#include "BLI_linear_allocator_chunked_list.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector.hh"
#include "BLI_offset_indices.hh"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

namespace blender::geometry {

struct Row {
  /** The min and max horizontal cell index that is used in this row. */
  int x_min = 0;
  int x_max = 0;
  /** Offsets into the array of indices below. Also see #OffsetIndices. */
  Array<int> offsets;
  /** A flat array containing the triangle indices contained in each cell. */
  Array<int> tri_indices;
};

struct ReverseUVSampler::LookupGrid {
  /** Minimum vertical cell index that contains triangles. */
  int y_min = 0;
  /** Information about all rows starting at `y_min`. */
  Array<Row> rows;
};

struct TriWithRange {
  int tri_index;
  int x_min;
  int x_max;
};

struct LocalRowData {
  linear_allocator::ChunkedList<TriWithRange, 8> tris;
  int x_min = INT32_MAX;
  int x_max = INT32_MIN;
};

struct LocalData {
  LinearAllocator<> allocator;
  Map<int, destruct_ptr<LocalRowData>> rows;
};

static int2 uv_to_cell(const float2 &uv, const int resolution)
{
  return int2{uv * resolution};
}

static Bounds<int2> tri_to_cell_bounds(const int3 &tri,
                                       const int resolution,
                                       const Span<float2> uv_map)
{
  const float2 &uv_0 = uv_map[tri[0]];
  const float2 &uv_1 = uv_map[tri[1]];
  const float2 &uv_2 = uv_map[tri[2]];

  const int2 cell_0 = uv_to_cell(uv_0, resolution);
  const int2 cell_1 = uv_to_cell(uv_1, resolution);
  const int2 cell_2 = uv_to_cell(uv_2, resolution);

  const int2 min_cell = math::min(math::min(cell_0, cell_1), cell_2);
  const int2 max_cell = math::max(math::max(cell_0, cell_1), cell_2);

  return {min_cell, max_cell};
}

/**
 * Add each triangle to the rows that it is in. After this, the information about each row is still
 * scattered across multiple thread-specific lists. Those separate lists are then joined in a
 * separate step.
 */
static void sort_tris_into_rows(const Span<float2> uv_map,
                                const Span<int3> corner_tris,
                                const int resolution,
                                threading::EnumerableThreadSpecific<LocalData> &data_per_thread)
{
  threading::parallel_for(corner_tris.index_range(), 256, [&](const IndexRange tris_range) {
    LocalData &local_data = data_per_thread.local();
    for (const int tri_i : tris_range) {
      const int3 &tri = corner_tris[tri_i];

      /* Compute the cells that the triangle touches approximately. */
      const Bounds<int2> cell_bounds = tri_to_cell_bounds(tri, resolution, uv_map);
      const TriWithRange tri_with_range{tri_i, cell_bounds.min.x, cell_bounds.max.x};

      /* Go over each row that the triangle is in. */
      for (int cell_y = cell_bounds.min.y; cell_y <= cell_bounds.max.y; cell_y++) {
        LocalRowData &row = *local_data.rows.lookup_or_add_cb(
            cell_y, [&]() { return local_data.allocator.construct<LocalRowData>(); });
        row.tris.append(local_data.allocator, tri_with_range);
        row.x_min = std::min<int>(row.x_min, cell_bounds.min.x);
        row.x_max = std::max<int>(row.x_max, cell_bounds.max.x);
      }
    }
  });
}

/**
 * Consolidates the data that has been gather for each row so that it is each to look up which
 * triangles are in each cell.
 */
static void finish_rows(const Span<int> all_ys,
                        const Span<const LocalData *> local_data_vec,
                        const Bounds<int> y_bounds,
                        ReverseUVSampler::LookupGrid &lookup_grid)
{
  threading::parallel_for(all_ys.index_range(), 8, [&](const IndexRange all_ys_range) {
    Vector<const LocalRowData *, 32> local_rows;
    for (const int y : all_ys.slice(all_ys_range)) {
      Row &row = lookup_grid.rows[y - y_bounds.min];

      local_rows.clear();
      for (const LocalData *local_data : local_data_vec) {
        if (const destruct_ptr<LocalRowData> *local_row = local_data->rows.lookup_ptr(y)) {
          local_rows.append(local_row->get());
        }
      }

      int x_min = INT32_MAX;
      int x_max = INT32_MIN;
      for (const LocalRowData *local_row : local_rows) {
        x_min = std::min(x_min, local_row->x_min);
        x_max = std::max(x_max, local_row->x_max);
      }

      const int x_num = x_max - x_min + 1;
      row.offsets.reinitialize(x_num + 1);
      {
        /* Count how many triangles are in each cell in the current row. */
        MutableSpan<int> counts = row.offsets;
        counts.fill(0);
        for (const LocalRowData *local_row : local_rows) {
          for (const TriWithRange &tri_with_range : local_row->tris) {
            for (int x = tri_with_range.x_min; x <= tri_with_range.x_max; x++) {
              counts[x - x_min]++;
            }
          }
        }
        offset_indices::accumulate_counts_to_offsets(counts);
      }
      const int tri_indices_num = row.offsets.last();
      row.tri_indices.reinitialize(tri_indices_num);

      /* Populate the array containing all triangle indices in all cells in this row. */
      Array<int, 1000> current_offsets(x_num, 0);
      for (const LocalRowData *local_row : local_rows) {
        for (const TriWithRange &tri_with_range : local_row->tris) {
          for (int x = tri_with_range.x_min; x <= tri_with_range.x_max; x++) {
            const int offset_x = x - x_min;
            row.tri_indices[row.offsets[offset_x] + current_offsets[offset_x]] =
                tri_with_range.tri_index;
            current_offsets[offset_x]++;
          }
        }
      }

      row.x_min = x_min;
      row.x_max = x_max;
    }
  });
}

ReverseUVSampler::ReverseUVSampler(const Span<float2> uv_map, const Span<int3> corner_tris)
    : uv_map_(uv_map), corner_tris_(corner_tris), lookup_grid_(std::make_unique<LookupGrid>())
{
  /* A lower resolution means that there will be fewer cells and more triangles in each cell. Fewer
   * cells make construction faster, but more triangles per cell make lookup slower. This value
   * needs to be determined experimentally. */
  resolution_ = std::max<int>(3, std::sqrt(corner_tris.size()) * 3);
  if (corner_tris.is_empty()) {
    return;
  }

  threading::EnumerableThreadSpecific<LocalData> data_per_thread;
  sort_tris_into_rows(uv_map_, corner_tris_, resolution_, data_per_thread);

  VectorSet<int> all_ys;
  Vector<const LocalData *> local_data_vec;
  for (const LocalData &local_data : data_per_thread) {
    local_data_vec.append(&local_data);
    for (const int y : local_data.rows.keys()) {
      all_ys.add(y);
    }
  }

  const Bounds<int> y_bounds = *bounds::min_max(all_ys.as_span());
  lookup_grid_->y_min = y_bounds.min;

  const int rows_num = y_bounds.max - y_bounds.min + 1;
  lookup_grid_->rows.reinitialize(rows_num);

  finish_rows(all_ys, local_data_vec, y_bounds, *lookup_grid_);
}

static Span<int> lookup_tris_in_cell(const int2 cell,
                                     const ReverseUVSampler::LookupGrid &lookup_grid)
{
  if (cell.y < lookup_grid.y_min) {
    return {};
  }
  if (cell.y >= lookup_grid.y_min + lookup_grid.rows.size()) {
    return {};
  }
  const Row &row = lookup_grid.rows[cell.y - lookup_grid.y_min];
  if (cell.x < row.x_min) {
    return {};
  }
  if (cell.x > row.x_max) {
    return {};
  }
  const int offset = row.offsets[cell.x - row.x_min];
  const int tris_num = row.offsets[cell.x - row.x_min + 1] - offset;
  return row.tri_indices.as_span().slice(offset, tris_num);
}

ReverseUVSampler::Result ReverseUVSampler::sample(const float2 &query_uv) const
{
  const int2 cell = uv_to_cell(query_uv, resolution_);
  const Span<int> tri_indices = lookup_tris_in_cell(cell, *lookup_grid_);

  float best_dist = FLT_MAX;
  float3 best_bary_weights;
  int best_tri_index;

  /* The distance to an edge that is allowed to be inside or outside the triangle. Without this,
   * the lookup can fail for floating point accuracy reasons when the uv is almost exact on an
   * edge. */
  const float edge_epsilon = 0.00001f;

  for (const int tri_i : tri_indices) {
    const int3 &tri = corner_tris_[tri_i];
    const float2 &uv_0 = uv_map_[tri[0]];
    const float2 &uv_1 = uv_map_[tri[1]];
    const float2 &uv_2 = uv_map_[tri[2]];
    float3 bary_weights;
    if (!barycentric_coords_v2(uv_0, uv_1, uv_2, query_uv, bary_weights)) {
      continue;
    }

    /* If #query_uv is in the triangle, the distance is <= 0. Otherwise, the larger the distance,
     * the further away the uv is from the triangle. */
    const float x_dist = std::max(-bary_weights.x, bary_weights.x - 1.0f);
    const float y_dist = std::max(-bary_weights.y, bary_weights.y - 1.0f);
    const float z_dist = std::max(-bary_weights.z, bary_weights.z - 1.0f);
    const float dist = std::max({x_dist, y_dist, z_dist});

    if (dist <= 0.0f && best_dist <= 0.0f) {
      const float worse_dist = std::max(dist, best_dist);
      /* Allow ignoring multiple triangle intersections if the uv is almost exactly on an edge. */
      if (worse_dist < -edge_epsilon) {
        /* The uv sample is in multiple triangles. */
        return Result{ResultType::Multiple};
      }
    }

    if (dist < best_dist) {
      best_dist = dist;
      best_bary_weights = bary_weights;
      best_tri_index = tri_i;
    }
  }

  /* Allow using the closest (but not intersecting) triangle if the uv is almost exactly on an
   * edge. */
  if (best_dist < edge_epsilon) {
    return Result{ResultType::Ok, best_tri_index, math::clamp(best_bary_weights, 0.0f, 1.0f)};
  }

  return Result{};
}

ReverseUVSampler::~ReverseUVSampler() = default;

void ReverseUVSampler::sample_many(const Span<float2> query_uvs,
                                   MutableSpan<Result> r_results) const
{
  BLI_assert(query_uvs.size() == r_results.size());
  threading::parallel_for(query_uvs.index_range(), 256, [&](const IndexRange range) {
    for (const int i : range) {
      r_results[i] = this->sample(query_uvs[i]);
    }
  });
}

}  // namespace blender::geometry

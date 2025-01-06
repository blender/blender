/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include <queue>

#include "BLI_bit_vector.hh"
#include "BLI_function_ref.hh"
#include "BLI_offset_indices.hh"

#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

struct BMVert;
struct Depsgraph;
struct Object;

namespace blender::ed::sculpt_paint::flood_fill {

struct FillDataMesh {
  std::queue<int> queue;
  BitVector<> visited_verts;
  Span<int> fake_neighbors;

  FillDataMesh(int size) : visited_verts(size) {}
  FillDataMesh(int size, Span<int> fake_neighbors)
      : visited_verts(size), fake_neighbors(fake_neighbors)
  {
    BLI_assert(fake_neighbors.is_empty() || size == fake_neighbors.size());
  }

  void add_initial(int vertex);
  void add_initial(Span<int> verts);
  void add_and_skip_initial(int vertex);
  void execute(Object &object,
               GroupedSpan<int> vert_to_face_map,
               FunctionRef<bool(int from_v, int to_v)> func);
};

struct FillDataGrids {
  std::queue<SubdivCCGCoord> queue;
  BitVector<> visited_verts;
  Span<int> fake_neighbors;

  FillDataGrids(int size) : visited_verts(size) {}
  FillDataGrids(int size, Span<int> fake_neighbors)
      : visited_verts(size), fake_neighbors(fake_neighbors)
  {
    BLI_assert(fake_neighbors.is_empty() || size == fake_neighbors.size());
  }

  void add_initial(SubdivCCGCoord vertex);
  void add_initial(const CCGKey &key, Span<int> verts);
  void add_and_skip_initial(SubdivCCGCoord vertex, int index);
  void execute(
      Object &object,
      const SubdivCCG &subdiv_ccg,
      FunctionRef<bool(SubdivCCGCoord from_v, SubdivCCGCoord to_v, bool is_duplicate)> func);
};

struct FillDataBMesh {
  std::queue<BMVert *> queue;
  BitVector<> visited_verts;
  Span<int> fake_neighbors;

  FillDataBMesh(int size) : visited_verts(size) {}
  FillDataBMesh(int size, Span<int> fake_neighbors)
      : visited_verts(size), fake_neighbors(fake_neighbors)
  {
    BLI_assert(fake_neighbors.is_empty() || size == fake_neighbors.size());
  }

  void add_initial(BMVert *vertex);
  void add_initial(BMesh &bm, Span<int> verts);
  void add_and_skip_initial(BMVert *vertex, int index);
  void execute(Object &object, FunctionRef<bool(BMVert *from_v, BMVert *to_v)> func);
};

}  // namespace blender::ed::sculpt_paint::flood_fill

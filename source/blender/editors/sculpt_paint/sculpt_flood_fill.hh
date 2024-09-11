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

#include "BKE_pbvh.hh"
#include "BKE_subdiv_ccg.hh"

struct BMVert;
struct Depsgraph;
struct Object;

namespace blender::ed::sculpt_paint::flood_fill {

struct FillData {
  std::queue<PBVHVertRef> queue;
  BitVector<> visited_verts;
};

struct FillDataMesh {
  std::queue<int> queue;
  BitVector<> visited_verts;

  FillDataMesh(int size) : visited_verts(size) {}

  void add_initial(int vertex);
  void add_and_skip_initial(int vertex);
  void add_initial_with_symmetry(const Depsgraph &depsgraph,
                                 const Object &object,
                                 const bke::pbvh::Tree &pbvh,
                                 int vertex,
                                 float radius);
  void execute(Object &object,
               GroupedSpan<int> vert_to_face_map,
               FunctionRef<bool(int from_v, int to_v)> func);
};

struct FillDataGrids {
  std::queue<SubdivCCGCoord> queue;
  BitVector<> visited_verts;

  FillDataGrids(int size) : visited_verts(size) {}

  void add_initial(SubdivCCGCoord vertex);
  void add_and_skip_initial(SubdivCCGCoord vertex, int index);
  void add_initial_with_symmetry(const Object &object,
                                 const bke::pbvh::Tree &pbvh,
                                 const SubdivCCG &subdiv_ccg,
                                 SubdivCCGCoord vertex,
                                 float radius);
  void execute(
      Object &object,
      const SubdivCCG &subdiv_ccg,
      FunctionRef<bool(SubdivCCGCoord from_v, SubdivCCGCoord to_v, bool is_duplicate)> func);
};

struct FillDataBMesh {
  std::queue<BMVert *> queue;
  BitVector<> visited_verts;

  FillDataBMesh(int size) : visited_verts(size) {}

  void add_initial(BMVert *vertex);
  void add_and_skip_initial(BMVert *vertex, int index);
  void add_initial_with_symmetry(const Object &object,
                                 const bke::pbvh::Tree &pbvh,
                                 BMVert *vertex,
                                 float radius);
  void execute(Object &object, FunctionRef<bool(BMVert *from_v, BMVert *to_v)> func);
};

/**
 * \deprecated See the individual FillData constructors instead of this method.
 */
FillData init_fill(Object &object);

void add_initial(FillData &flood, PBVHVertRef vertex);
void add_and_skip_initial(FillData &flood, PBVHVertRef vertex);
void add_initial_with_symmetry(const Depsgraph &depsgraph,
                               const Object &ob,
                               FillData &flood,
                               PBVHVertRef vertex,
                               float radius);
void execute(Object &object,
             FillData &flood,
             FunctionRef<bool(PBVHVertRef from_v, PBVHVertRef to_v, bool is_duplicate)> func);

}  // namespace blender::ed::sculpt_paint::flood_fill

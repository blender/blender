/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "sculpt_flood_fill.hh"

#include "BKE_attribute.hh"
#include "BKE_mesh.hh"

#include "DNA_mesh_types.h"

#include "paint_intern.hh"
#include "sculpt_hide.hh"
#include "sculpt_intern.hh"

#include "bmesh.hh"

/* -------------------------------------------------------------------- */
/** \name Sculpt Flood Fill API
 *
 * Iterate over connected vertices, starting from one or more initial vertices.
 * \{ */

namespace blender::ed::sculpt_paint::flood_fill {

void FillDataMesh::add_initial(const int vertex)
{
  this->queue.push(vertex);
}

void FillDataMesh::add_initial(const Span<int> verts)
{
  for (const int vert : verts) {
    this->add_initial(vert);
  }
}

void FillDataGrids::add_initial(const SubdivCCGCoord vertex)
{
  this->queue.push(vertex);
}

void FillDataGrids::add_initial(const CCGKey &key, const Span<int> verts)
{
  for (const int vert : verts) {
    this->add_initial(SubdivCCGCoord::from_index(key, vert));
  }
}

void FillDataBMesh::add_initial(BMVert *vertex)
{
  this->queue.push(vertex);
}

void FillDataBMesh::add_initial(BMesh &bm, const Span<int> verts)
{
  for (const int vert : verts) {
    this->add_initial(BM_vert_at_index(&bm, vert));
  }
}

void FillDataMesh::add_and_skip_initial(const int vertex)
{
  this->queue.push(vertex);
  this->visited_verts[vertex].set();
}

void FillDataGrids::add_and_skip_initial(const SubdivCCGCoord vertex, const int index)
{
  this->queue.push(vertex);
  this->visited_verts[index].set();
}

void FillDataBMesh::add_and_skip_initial(BMVert *vertex, const int index)
{
  this->queue.push(vertex);
  this->visited_verts[index].set();
}

void FillDataMesh::execute(Object &object,
                           const GroupedSpan<int> vert_to_face_map,
                           FunctionRef<bool(int from_v, int to_v)> func)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArray hide_vert = *attributes.lookup_or_default<bool>(
      ".hide_vert", bke::AttrDomain::Point, false);

  Vector<int> neighbors;
  while (!this->queue.empty()) {
    const int from_v = this->queue.front();
    this->queue.pop();

    vert_neighbors_get_mesh(faces, corner_verts, vert_to_face_map, hide_poly, from_v, neighbors);
    if (!this->fake_neighbors.is_empty() && this->fake_neighbors[from_v] != FAKE_NEIGHBOR_NONE) {
      neighbors.append(this->fake_neighbors[from_v]);
    }

    for (const int neighbor : neighbors) {
      if (this->visited_verts[neighbor]) {
        continue;
      }

      if (!hide_vert.is_empty() && hide_vert[neighbor]) {
        continue;
      }

      this->visited_verts[neighbor].set();
      if (func(from_v, neighbor)) {
        this->queue.push(neighbor);
      }
    }
  }
}

void FillDataGrids::execute(
    Object & /*object*/,
    const SubdivCCG &subdiv_ccg,
    FunctionRef<bool(SubdivCCGCoord from_v, SubdivCCGCoord to_v, bool is_duplicate)> func)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  while (!this->queue.empty()) {
    SubdivCCGCoord from_v = this->queue.front();
    this->queue.pop();

    SubdivCCGNeighbors neighbors;
    BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, from_v, true, neighbors);
    if (!this->fake_neighbors.is_empty() &&
        this->fake_neighbors[from_v.to_index(key)] != FAKE_NEIGHBOR_NONE)
    {
      neighbors.coords.insert(
          0, SubdivCCGCoord::from_index(key, this->fake_neighbors[from_v.to_index(key)]));
    }

    const int num_unique = neighbors.coords.size() - neighbors.num_duplicates;

    /* Flood fill expects the duplicate entries to be passed to the per-neighbor lambda first, so
     * iterate from the end of the vector to the beginning. */
    for (int i = neighbors.coords.size() - 1; i >= 0; i--) {
      SubdivCCGCoord neighbor = neighbors.coords[i];
      const int index_in_grid = neighbor.y * key.grid_size + neighbor.x;
      const int index = neighbor.grid_index * key.grid_area + index_in_grid;
      if (this->visited_verts[index]) {
        continue;
      }

      if (!subdiv_ccg.grid_hidden.is_empty() &&
          subdiv_ccg.grid_hidden[neighbor.grid_index][index_in_grid])
      {
        continue;
      }

      this->visited_verts[index].set();
      const bool is_duplicate = i >= num_unique;
      if (func(from_v, neighbor, is_duplicate)) {
        this->queue.push(neighbor);
      }
    }
  }
}

void FillDataBMesh::execute(Object &object, FunctionRef<bool(BMVert *from_v, BMVert *to_v)> func)
{
  BMesh *bm = object.sculpt->bm;
  BMeshNeighborVerts neighbors;
  while (!this->queue.empty()) {
    BMVert *from_v = this->queue.front();
    this->queue.pop();

    if (!this->fake_neighbors.is_empty() &&
        this->fake_neighbors[BM_elem_index_get(from_v)] != FAKE_NEIGHBOR_NONE)
    {
      neighbors.append(BM_vert_at_index(bm, this->fake_neighbors[BM_elem_index_get(from_v)]));
    }

    for (BMVert *neighbor : vert_neighbors_get_bmesh(*from_v, neighbors)) {
      const int neighbor_idx = BM_elem_index_get(neighbor);
      if (this->visited_verts[neighbor_idx]) {
        continue;
      }

      if (BM_elem_flag_test(neighbor, BM_ELEM_HIDDEN)) {
        continue;
      }

      this->visited_verts[neighbor_idx].set();
      if (func(from_v, neighbor)) {
        this->queue.push(neighbor);
      }
    }
  }
}

}  // namespace blender::ed::sculpt_paint::flood_fill

/** \} */

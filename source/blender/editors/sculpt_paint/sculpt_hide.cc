/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "sculpt_hide.hh"

#include "BKE_attribute.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

namespace blender::ed::sculpt_paint::hide {

Span<int> node_visible_verts(const bke::pbvh::MeshNode &node,
                             const Span<bool> hide_vert,
                             Vector<int> &indices)
{
  if (BKE_pbvh_node_fully_hidden_get(node)) {
    return {};
  }
  const Span<int> verts = bke::pbvh::node_unique_verts(node);
  if (hide_vert.is_empty()) {
    return verts;
  }
  indices.resize(verts.size());
  const int *end = std::copy_if(verts.begin(), verts.end(), indices.begin(), [&](const int vert) {
    return !hide_vert[vert];
  });
  indices.resize(end - indices.begin());
  return indices;
}

bool vert_visible_get(const Object &object, PBVHVertRef vertex)
{
  const SculptSession &ss = *object.sculpt;
  switch (ss.pbvh->type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArray hide_vert = *attributes.lookup_or_default<bool>(
          ".hide_vert", bke::AttrDomain::Point, false);
      return !hide_vert[vertex.i];
    }
    case bke::pbvh::Type::BMesh:
      return !BM_elem_flag_test((BMVert *)vertex.i, BM_ELEM_HIDDEN);
    case bke::pbvh::Type::Grids: {
      const CCGKey key = BKE_subdiv_ccg_key_top_level(*ss.subdiv_ccg);
      const int grid_index = vertex.i / key.grid_area;
      const int index_in_grid = vertex.i - grid_index * key.grid_area;
      if (!ss.subdiv_ccg->grid_hidden.is_empty()) {
        return !ss.subdiv_ccg->grid_hidden[grid_index][index_in_grid];
      }
    }
  }
  return true;
}

bool vert_all_faces_visible_get(const Span<bool> hide_poly,
                                const GroupedSpan<int> vert_to_face_map,
                                const int vert)
{
  if (hide_poly.is_empty()) {
    return true;
  }

  for (const int face : vert_to_face_map[vert]) {
    if (hide_poly[face]) {
      return false;
    }
  }
  return true;
}

bool vert_all_faces_visible_get(const Span<bool> hide_poly,
                                const SubdivCCG &subdiv_ccg,
                                const SubdivCCGCoord vert)
{
  const int face_index = BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, vert.grid_index);
  return hide_poly[face_index];
}

bool vert_all_faces_visible_get(BMVert *vert)
{
  BMEdge *edge = vert->e;

  if (!edge) {
    return true;
  }

  do {
    BMLoop *loop = edge->l;

    if (!loop) {
      continue;
    }

    do {
      if (BM_elem_flag_test(loop->f, BM_ELEM_HIDDEN)) {
        return false;
      }
    } while ((loop = loop->radial_next) != edge->l);
  } while ((edge = BM_DISK_EDGE_NEXT(edge, vert)) != vert->e);

  return true;
}
}  // namespace blender::ed::sculpt_paint::hide

/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "sculpt_hide.hh"

#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::hide {

Span<int> node_visible_verts(const bke::pbvh::MeshNode &node,
                             const Span<bool> hide_vert,
                             Vector<int> &indices)
{
  if (BKE_pbvh_node_fully_hidden_get(node)) {
    return {};
  }
  const Span<int> verts = node.verts();
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

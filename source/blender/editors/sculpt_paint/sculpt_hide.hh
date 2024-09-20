/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

struct BMVert;
struct Object;
struct SubdivCCG;
struct SubdivCCGCoord;
namespace blender::bke::pbvh {
struct MeshNode;
}

namespace blender::ed::sculpt_paint::hide {

Span<int> node_visible_verts(const bke::pbvh::MeshNode &node,
                             Span<bool> hide_vert,
                             Vector<int> &indices);

/* Determines if all faces attached to a given vertex are visible. */
bool vert_all_faces_visible_get(Span<bool> hide_poly, GroupedSpan<int> vert_to_face_map, int vert);
bool vert_all_faces_visible_get(Span<bool> hide_poly,
                                const SubdivCCG &subdiv_ccg,
                                SubdivCCGCoord vert);
bool vert_all_faces_visible_get(BMVert *vert);

}  // namespace blender::ed::sculpt_paint::hide

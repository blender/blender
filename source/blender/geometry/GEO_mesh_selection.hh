/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"

namespace blender::geometry {

/** A vertex is selected if it's used by a selected edge. */
IndexMask vert_selection_from_edge(Span<int2> edges,
                                   const IndexMask &edge_mask,
                                   int verts_num,
                                   IndexMaskMemory &memory);

/** A vertex is selected if it is used by a selected face. */
IndexMask vert_selection_from_face(OffsetIndices<int> faces,
                                   const IndexMask &face_mask,
                                   Span<int> corner_verts,
                                   int verts_num,
                                   IndexMaskMemory &memory);

/** An edge is selected if it is used by a selected face. */
IndexMask edge_selection_from_face(OffsetIndices<int> faces,
                                   const IndexMask &face_mask,
                                   Span<int> corner_edges,
                                   int edges_num,
                                   IndexMaskMemory &memory);

/** An edge is selected if both of its vertices are selected. */
IndexMask edge_selection_from_vert(Span<int2> edges,
                                   Span<bool> vert_selection,
                                   IndexMaskMemory &memory);

/** A face is selected if all of its vertices are selected. */
IndexMask face_selection_from_vert(OffsetIndices<int> faces,
                                   Span<int> corner_verts,
                                   Span<bool> vert_selection,
                                   IndexMaskMemory &memory);

/** A face is selected if all of its edges are selected. */
IndexMask face_selection_from_edge(OffsetIndices<int> faces,
                                   Span<int> corner_edges,
                                   Span<bool> edge_mask,
                                   IndexMaskMemory &memory);

}  // namespace blender::geometry

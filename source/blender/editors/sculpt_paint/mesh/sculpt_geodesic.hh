/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_set.hh"

namespace blender::ed::sculpt_paint::geodesic {

/**
 * Returns an array indexed by vertex index containing the geodesic distance to the closest vertex
 * in the initial vertex set.
 */
Array<float> distances_create(Span<float3> vert_positions,
                              Span<int2> edges,
                              OffsetIndices<int> faces,
                              Span<int> corner_verts,
                              GroupedSpan<int> vert_to_edge_map,
                              GroupedSpan<int> edge_to_face_map,
                              Span<bool> hide_poly,
                              const Set<int> &initial_verts,
                              float limit_radius);

}  // namespace blender::ed::sculpt_paint::geodesic

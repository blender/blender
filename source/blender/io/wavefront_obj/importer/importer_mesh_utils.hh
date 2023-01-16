/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

struct Object;
struct OBJImportParams;

namespace blender::io::obj {

/**
 * Given an invalid polygon (with holes or duplicated vertex indices),
 * turn it into possibly multiple polygons that are valid.
 *
 * \param vertex_coords: Polygon's vertex coordinate list.
 * \param face_vertex_indices: A polygon's indices that index into the given vertex coordinate
 * list.
 *
 * \return List of polygons with each element containing indices of one polygon. The indices
 * are into face_vertex_indices array.
 */
Vector<Vector<int>> fixup_invalid_polygon(Span<float3> vertex_coords,
                                          Span<int> face_vertex_indices);

/**
 * Apply axes transform to the Object, and clamp object dimensions to the specified value.
 */
void transform_object(Object *object, const OBJImportParams &import_params);

}  // namespace blender::io::obj

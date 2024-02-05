/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"
#include <string>

struct Object;
struct OBJImportParams;

namespace blender::io::obj {

/**
 * Given an invalid face (with holes or duplicated vertex indices),
 * turn it into possibly multiple faces that are valid.
 *
 * \param vert_coords: Polygon's vertex coordinate list.
 * \param face_vert_indices: A face's indices that index into the given vertex coordinate
 * list.
 *
 * \return List of faces with each element containing indices of one face. The indices
 * are into face_vert_indices array.
 */
Vector<Vector<int>> fixup_invalid_face(Span<float3> vert_coords, Span<int> face_vert_indices);

/**
 * Apply axes transform to the Object, and clamp object dimensions to the specified value.
 */
void transform_object(Object *object, const OBJImportParams &import_params);

std::string get_geometry_name(const std::string &full_name, char separator);

}  // namespace blender::io::obj

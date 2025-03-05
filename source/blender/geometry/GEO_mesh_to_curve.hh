/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"

#include "BKE_curves.hh"

struct Mesh;

/** \file
 * \ingroup geo
 */

namespace blender::geometry {

/**
 * Convert the mesh into one or many poly curves. Since curves cannot have branches,
 * intersections of more than three edges will become breaks in curves. Attributes that
 * are not built-in on meshes and not curves are transferred to the result curve.
 */
bke::CurvesGeometry mesh_edges_to_curves_convert(const Mesh &mesh,
                                                 const IndexMask &selection,
                                                 const bke::AttributeFilter &attribute_filter);

bke::CurvesGeometry create_curve_from_vert_indices(const bke::AttributeAccessor &mesh_attributes,
                                                   Span<int> vert_indices,
                                                   Span<int> curve_offsets,
                                                   IndexRange cyclic_curves,
                                                   const bke::AttributeFilter &attribute_filter);

/** Convert each mesh face into a cyclic curve. */
bke::CurvesGeometry mesh_faces_to_curves_convert(const Mesh &mesh,
                                                 const IndexMask &selection,
                                                 const bke::AttributeFilter &attribute_filter);

}  // namespace blender::geometry
